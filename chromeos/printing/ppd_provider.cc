// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_provider.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chromeos/printing/epson_driver_matching.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_metadata_manager.h"
#include "chromeos/printing/printer_config_cache.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printing_constants.h"
#include "chromeos/printing/remote_ppd_fetcher.h"
#include "components/device_event_log/device_event_log.h"
#include "net/base/filename_util.h"

namespace chromeos {
namespace {

// Age limit for time-sensitive API calls. Typically denotes "Please
// respond with data no older than kMaxDataAge." Arbitrarily chosen.
constexpr base::TimeDelta kMaxDataAge = base::Minutes(30LL);

// Effective-make-and-model string that describes a printer capable of
// using the generic Epson PPD.
const char kEpsonGenericEmm[] = "epson generic escpr printer";

bool PpdReferenceIsWellFormed(const Printer::PpdReference& reference) {
  int filled_fields = 0;
  if (!reference.user_supplied_ppd_url.empty()) {
    ++filled_fields;
    GURL tmp_url(reference.user_supplied_ppd_url);
    const bool is_http = tmp_url.SchemeIsHTTPOrHTTPS();
    const bool is_file = tmp_url.SchemeIs("file");
    const bool has_supported_scheme = is_http || is_file;
    if (!tmp_url.is_valid() || !has_supported_scheme) {
      LOG(ERROR) << "Invalid url for a user-supplied ppd: "
                 << reference.user_supplied_ppd_url;
      return false;
    }
  }
  if (!reference.effective_make_and_model.empty()) {
    ++filled_fields;
  }

  // All effective-make-and-model strings should be lowercased, since v2.
  // Since make-and-model strings could include non-Latin chars, only checking
  // that it excludes all upper-case chars A-Z.
  if (!base::ranges::all_of(reference.effective_make_and_model,
                            [](char c) { return !base::IsAsciiUpper(c); })) {
    return false;
  }
  // Should have exactly one non-empty field.
  return filled_fields == 1;
}

std::string PpdPathInServingRoot(std::string_view ppd_basename) {
  return base::StrCat({"ppds_for_metadata_v3/", ppd_basename});
}

// Zebra printers that support ZPL contain "Zebra" (or "Zebra Technologies") and
// "ZPL" in the IEEE 1284 device id make and model.
bool SupportsGenericZebraPPD(const PrinterSearchData& search_data) {
  return search_data.printer_id.make().starts_with("Zebra") &&
         base::Contains(search_data.printer_id.model(), "ZPL");
}

// This class implements the PpdProvider interface for the v3 metadata
// (https://crbug.com/888189).
class PpdProviderImpl : public PpdProvider {
 public:
  PpdProviderImpl(const base::Version& current_version,
                  scoped_refptr<PpdCache> cache,
                  std::unique_ptr<PpdMetadataManager> metadata_manager,
                  std::unique_ptr<PrinterConfigCache> config_cache,
                  std::unique_ptr<RemotePpdFetcher> remote_ppd_fetcher)
      : version_(current_version),
        ppd_cache_(cache),
        metadata_manager_(std::move(metadata_manager)),
        config_cache_(std::move(config_cache)),
        remote_ppd_fetcher_(std::move(remote_ppd_fetcher)),
        file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  void ResolveManufacturers(ResolveManufacturersCallback cb) override {
    metadata_manager_->GetManufacturers(kMaxDataAge, std::move(cb));
  }

  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {
    PpdMetadataManager::GetPrintersCallback manager_callback =
        base::BindOnce(&PpdProviderImpl::OnPrintersGotten,
                       weak_factory_.GetWeakPtr(), std::move(cb));
    metadata_manager_->GetPrinters(manufacturer, kMaxDataAge,
                                   std::move(manager_callback));
  }

  // This method examines the members of |search_data| in turn and seeks
  // out an appropriate PPD from the serving root. The order is
  // 1. |search_data|::make_and_model - we seek out
  //    effective-make-and-model strings from forward index metadata.
  // 2. |search_data|::usb_*_id - we seek out a device with a matching
  //    ID from USB index metadata.
  // 3. |search_data|::make_and_model - we check if any among these
  //    effective-make-and-model strings describe a printer for which
  //    we can use the generic Epson PPD.
  //
  // *  This method observes and honors PPD restrictions (furnished by
  //    forward index metadata) and will ignore PPDs that are not
  //    advertised to run with the current |version_|.
  void ResolvePpdReference(const PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {
    // In v3 metadata, effective-make-and-model strings are only
    // expressed in lowercased ASCII.
    PrinterSearchData lowercased_search_data(search_data);
    for (std::string& emm : lowercased_search_data.make_and_model) {
      emm = base::ToLowerASCII(emm);
    }

    // Any Zebra printer that supports ZPL uses the same PPD file, which is
    // kept in the PPD index with the key "zebra zpl label printer".
    if (SupportsGenericZebraPPD(lowercased_search_data)) {
      lowercased_search_data.make_and_model.clear();
      lowercased_search_data.make_and_model.push_back(
          "zebra zpl label printer");
    }

    ResolvePpdReferenceContext context(lowercased_search_data, std::move(cb));

    // Initiate step 1 if possible.
    if (!lowercased_search_data.make_and_model.empty()) {
      auto callback = base::BindOnce(
          &PpdProviderImpl::TryToResolvePpdReferenceFromForwardIndices,
          weak_factory_.GetWeakPtr(), std::move(context));
      metadata_manager_->FindAllEmmsAvailableInIndex(
          lowercased_search_data.make_and_model, kMaxDataAge,
          std::move(callback));
      return;
    }

    // Otherwise, jump straight to step 2.
    TryToResolvePpdReferenceFromUsbIndices(std::move(context));
  }

  // This method invokes |cb| with the contents of a successfully
  // retrieved PPD appropriate for |reference|.
  //
  // As a side effect, this method may attempt
  // *  to read a PPD from the user's files (if the PPD is a
  //    user-supplied local file) or
  // *  to download a PPD from an http(s) URL (if the PPD is specified by a
  //    user-supplied remote URL
  // *  to download a PPD from the serving root (if the PPD is specified by
  //    effective-make-and-model).
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {
    // In v3 metadata, effective-make-and-model strings are only
    // expressed in lowercased ASCII.
    Printer::PpdReference lowercased_reference(reference);
    lowercased_reference.effective_make_and_model =
        base::ToLowerASCII(lowercased_reference.effective_make_and_model);

    if (!PpdReferenceIsWellFormed(lowercased_reference)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb),
                                    CallbackResultCode::INTERNAL_ERROR, ""));
      return;
    }

    if (!lowercased_reference.user_supplied_ppd_url.empty()) {
      ResolveUserSuppliedPpd(lowercased_reference, std::move(cb));
      return;
    }

    std::vector<std::string> target_emm = {
        lowercased_reference.effective_make_and_model};
    auto callback =
        base::BindOnce(&PpdProviderImpl::OnPpdBasenameSoughtFromForwardIndex,
                       weak_factory_.GetWeakPtr(),
                       std::move(lowercased_reference), std::move(cb));
    metadata_manager_->FindAllEmmsAvailableInIndex(target_emm, kMaxDataAge,
                                                   std::move(callback));
  }

  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {
    // In v3 metadata, effective-make-and-model strings are only
    // expressed in lowercased ASCII.
    std::string lowercased_effective_make_and_model =
        base::ToLowerASCII(effective_make_and_model);

    // Delegates directly to the PpdMetadataManager.
    metadata_manager_->SplitMakeAndModel(lowercased_effective_make_and_model,
                                         kMaxDataAge, std::move(cb));
  }

  // This method depends on forward indices.
  void ResolvePpdLicense(std::string_view effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    // In v3 metadata, effective-make-and-model strings are only
    // expressed in lowercased ASCII.
    const std::string lowercased_effective_make_and_model =
        base::ToLowerASCII(effective_make_and_model);

    auto callback = base::BindOnce(
        &PpdProviderImpl::FindLicenseForEmm, weak_factory_.GetWeakPtr(),
        lowercased_effective_make_and_model, std::move(cb));
    metadata_manager_->FindAllEmmsAvailableInIndex(
        {lowercased_effective_make_and_model}, kMaxDataAge,
        std::move(callback));
  }

 protected:
  ~PpdProviderImpl() override = default;

 private:
  // Convenience container used throughout ResolvePpdReference().
  struct ResolvePpdReferenceContext {
    ResolvePpdReferenceContext(const PrinterSearchData& search_data_arg,
                               ResolvePpdReferenceCallback cb_arg)
        : search_data(search_data_arg), cb(std::move(cb_arg)) {}
    ~ResolvePpdReferenceContext() = default;

    // This container is not copyable and is move-only.
    ResolvePpdReferenceContext(ResolvePpdReferenceContext&& other) = default;
    ResolvePpdReferenceContext& operator=(ResolvePpdReferenceContext&& other) =
        default;

    PrinterSearchData search_data;
    ResolvePpdReferenceCallback cb;
  };

  // Used internally in ResolvePpd(). Describes the physical, bitwise
  // origin of a PPD.
  //
  // Example: a PPD previously downloaded from the serving root is saved
  // into the local PpdCache. A subsequent call to ResolvePpd() searches
  // the local PpdCache and returns this PPD. Internally, the methods
  // that comprise ResolvePpd() treat this as kFromPpdCache.
  enum class ResolvedPpdOrigin {
    kFromServingRoot,
    kFromUserSuppliedUrl,
    kFromPpdCache,
  };

  // Returns an empty string on failure.
  static std::string FetchFile(const GURL& ppd_url) {
    DCHECK(ppd_url.is_valid());
    DCHECK(ppd_url.SchemeIs("file"));
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    base::FilePath path;
    if (!net::FileURLToFilePath(ppd_url, &path)) {
      LOG(ERROR) << "Not a valid file URL.";
      return "";
    }

    std::string file_contents;
    if (!base::ReadFileToString(path, &file_contents)) {
      return "";
    }
    return file_contents;
  }

  // Evaluates true if our |version_| falls within the bounds set by
  // |restrictions|.
  bool CurrentVersionSatisfiesRestrictions(
      const Restrictions& restrictions) const {
    if ((restrictions.min_milestone.has_value() &&
         restrictions.min_milestone.value().IsValid() &&
         version_ < restrictions.min_milestone) ||
        (restrictions.max_milestone.has_value() &&
         restrictions.max_milestone.value().IsValid() &&
         version_ > restrictions.max_milestone)) {
      return false;
    }
    return true;
  }

  // Callback fed to PpdMetadataManager::GetPrinters().
  void OnPrintersGotten(ResolvePrintersCallback cb,
                        bool succeeded,
                        const ParsedPrinters& printers) {
    if (!succeeded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), CallbackResultCode::SERVER_ERROR,
                         ResolvedPrintersList()));
      return;
    }

    ResolvedPrintersList printers_available_to_our_version;
    for (const ParsedPrinter& printer : printers) {
      if (CurrentVersionSatisfiesRestrictions(printer.restrictions)) {
        Printer::PpdReference ppd_reference;
        ppd_reference.effective_make_and_model =
            printer.effective_make_and_model;
        printers_available_to_our_version.push_back(ResolvedPpdReference{
            printer.user_visible_printer_name, ppd_reference});
      }
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), CallbackResultCode::SUCCESS,
                                  printers_available_to_our_version));
  }

  // Finds the first ParsedIndexLeaf keyed on |effective_make_and_model|
  // from |forward_index_subset| (a slice of forward index metadata)
  // that is allowed for use in our current |version_|.
  //
  // Note that |forward_index_subset| has the type returned by
  // PpdMetadataManager::FindAllEmmsAvailableInIndexCallback.
  const ParsedIndexLeaf* FirstAllowableParsedIndexLeaf(
      std::string_view effective_make_and_model,
      const base::flat_map<std::string, ParsedIndexValues>&
          forward_index_subset) const {
    const auto& iter = forward_index_subset.find(effective_make_and_model);
    if (iter == forward_index_subset.end()) {
      return nullptr;
    }

    for (const ParsedIndexLeaf& index_leaf : iter->second.values) {
      if (CurrentVersionSatisfiesRestrictions(index_leaf.restrictions)) {
        return &index_leaf;
      }
    }

    return nullptr;
  }

  static void SuccessfullyResolvePpdReferenceWithEmm(
      std::string_view effective_make_and_model,
      ResolvePpdReferenceCallback cb) {
    Printer::PpdReference reference;
    reference.effective_make_and_model = std::string(effective_make_and_model);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), CallbackResultCode::SUCCESS,
                                  std::move(reference), /*manufacturer=*/""));
  }

  // Fails a prior call to ResolvePpdReference().
  // |usb_manufacturer| may be empty
  // *  if we didn't find a manufacturer name for the given vendor ID or
  // *  if we invoked this method directly with an empty manufacturer
  //    name: "this wasn't a USB printer in the first place, so there's
  //    no USB manufacturer to speak of."
  static void FailToResolvePpdReferenceWithUsbManufacturer(
      ResolvePpdReferenceCallback cb,
      const std::string& usb_manufacturer) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), CallbackResultCode::NOT_FOUND,
                                  Printer::PpdReference(), usb_manufacturer));
  }

  // Entry point to fail a prior call to ResolvePpdReference().
  void FailToResolvePpdReference(ResolvePpdReferenceContext context) {
    if (context.search_data.discovery_type ==
        PrinterSearchData::PrinterDiscoveryType::kUsb) {
      auto callback = base::BindOnce(
          &PpdProviderImpl::FailToResolvePpdReferenceWithUsbManufacturer,
          std::move(context.cb));
      metadata_manager_->GetUsbManufacturerName(
          context.search_data.usb_vendor_id, kMaxDataAge, std::move(callback));
      return;
    }

    // If |search_data| does not describe a USB printer, the |cb| is
    // posted in the same way, but with an empty USB manufacturer name.
    FailToResolvePpdReferenceWithUsbManufacturer(std::move(context.cb),
                                                 /*manufacturer=*/"");
  }

  // Continues a prior call to ResolvePpdReference() (step 3).
  // This callback is fed to
  // PpdMetadataManager::FindAllEmmsAvailableInIndex(), as we treat the
  // hardcoded generic Epson effective-make-and-model string like any
  // other emm, duly verifying its presence in forward index metadata
  // and that it is not restricted from running in this |version_|.
  void OnForwardIndicesSearchedForGenericEpsonEmm(
      ResolvePpdReferenceContext context,
      const base::flat_map<std::string, ParsedIndexValues>&
          forward_index_results) {
    const ParsedIndexLeaf* const index_leaf =
        FirstAllowableParsedIndexLeaf(kEpsonGenericEmm, forward_index_results);
    if (index_leaf) {
      SuccessfullyResolvePpdReferenceWithEmm(kEpsonGenericEmm,
                                             std::move(context.cb));
      return;
    }

    // This really shouldn't happen, but we couldn't build a
    // PpdReference that would point to the generic Epson PPD.
    // (This might mean that the serving root is badly messed up in a
    // way that escaped the attention of the Chrome OS printing team.)
    FailToResolvePpdReference(std::move(context));
  }

  // Continues a prior call to ResolvePpdReference() (step 3).
  void TryToResolvePpdReferenceWithGenericEpsonPpd(
      ResolvePpdReferenceContext context) {
    if (CanUseEpsonGenericPPD(context.search_data)) {
      auto callback = base::BindOnce(
          &PpdProviderImpl::OnForwardIndicesSearchedForGenericEpsonEmm,
          weak_factory_.GetWeakPtr(), std::move(context));
      metadata_manager_->FindAllEmmsAvailableInIndex(
          {kEpsonGenericEmm}, kMaxDataAge, std::move(callback));
      return;
    }

    // At this point, we couldn't build a PpdReference using the
    // generic Epson PPD. ResolvePpdReference() can only fail now.
    FailToResolvePpdReference(std::move(context));
  }

  // Continues a prior call to ResolvePpdReference() (step 2).
  // This callback is fed to
  // PpdMetadataManager::FindAllEmmsAvailableInIndex().
  void OnForwardIndicesSearchedForUsbEmm(
      ResolvePpdReferenceContext context,
      const std::string& effective_make_and_model_from_usb_index,
      const base::flat_map<std::string, ParsedIndexValues>&
          forward_index_results) {
    const ParsedIndexLeaf* const index_leaf = FirstAllowableParsedIndexLeaf(
        effective_make_and_model_from_usb_index, forward_index_results);
    if (index_leaf) {
      SuccessfullyResolvePpdReferenceWithEmm(
          effective_make_and_model_from_usb_index, std::move(context.cb));
      return;
    }

    // At this point, we couldn't build a PpdReference from the
    // effective-make-and-model string sourced from the USB index.
    // ResolvePpdReference() continues to its next step.
    TryToResolvePpdReferenceWithGenericEpsonPpd(std::move(context));
  }

  // Continues a prior call to ResolvePpdReference() (step 2).
  // This callback is fed to PpdMetadataManager::FindDeviceInUsbIndex().
  void OnUsbIndicesSearched(ResolvePpdReferenceContext context,
                            const std::string& effective_make_and_model) {
    if (!effective_make_and_model.empty()) {
      auto callback =
          base::BindOnce(&PpdProviderImpl::OnForwardIndicesSearchedForUsbEmm,
                         weak_factory_.GetWeakPtr(), std::move(context),
                         effective_make_and_model);
      metadata_manager_->FindAllEmmsAvailableInIndex(
          {effective_make_and_model}, kMaxDataAge, std::move(callback));
      return;
    }

    // At this point, we couldn't build a PpdReference from a USB index
    // search. ResolvePpdReference() continues to its next step.
    TryToResolvePpdReferenceWithGenericEpsonPpd(std::move(context));
  }

  // Continues a prior call to ResolvePpdReference() (step 2).
  void TryToResolvePpdReferenceFromUsbIndices(
      ResolvePpdReferenceContext context) {
    const int vendor_id = context.search_data.usb_vendor_id;
    const int product_id = context.search_data.usb_product_id;
    if (vendor_id && product_id) {
      auto callback =
          base::BindOnce(&PpdProviderImpl::OnUsbIndicesSearched,
                         weak_factory_.GetWeakPtr(), std::move(context));
      metadata_manager_->FindDeviceInUsbIndex(vendor_id, product_id,
                                              kMaxDataAge, std::move(callback));
      return;
    }

    // At this point, we couldn't use |search_data| to search USB indices.
    // ResolvePpdReference() continues to its next step.
    TryToResolvePpdReferenceWithGenericEpsonPpd(std::move(context));
  }

  // Continues a prior call to ResolvePpdReference() (step 1).
  // This callback is fed to
  // PpdMetadataManager::FindAllEmmsAvailableInIndexCallback().
  void TryToResolvePpdReferenceFromForwardIndices(
      ResolvePpdReferenceContext context,
      const base::flat_map<std::string, ParsedIndexValues>&
          forward_index_results) {
    // Sweeps through the results of the forward index metadata search.
    // If any effective-make-and-model string advertises an available
    // PPD, we use that result to post |cb|.
    for (std::string_view effective_make_and_model :
         context.search_data.make_and_model) {
      const ParsedIndexLeaf* const index_leaf = FirstAllowableParsedIndexLeaf(
          effective_make_and_model, forward_index_results);
      if (!index_leaf) {
        continue;
      }

      SuccessfullyResolvePpdReferenceWithEmm(effective_make_and_model,
                                             std::move(context.cb));
      return;
    }

    // At this point, we couldn't build a PpdReference directly from a
    // forward index search. ResolvePpdReference() continues to step 2.
    TryToResolvePpdReferenceFromUsbIndices(std::move(context));
  }

  // Continues a prior call to ResolvePpd().
  //
  // Stores a PPD with |ppd_contents| in the PPD Cache.
  // Caller must provide nonempty |ppd_basename| when |ppd_origin|
  // identifies the PPD as coming from the the serving root.
  void StorePpdWithContents(const std::string& ppd_contents,
                            std::optional<std::string> ppd_basename,
                            ResolvedPpdOrigin ppd_origin,
                            Printer::PpdReference reference) {
    switch (ppd_origin) {
      case ResolvedPpdOrigin::kFromPpdCache:
        // This very PPD was retrieved from the local PpdCache; there's no
        // point in storing it again.
        return;

      case ResolvedPpdOrigin::kFromServingRoot:
        // To service the two-step "dereference" of resolving a PPD from
        // the serving root, we need to Store() the basename of this PPD
        // in the local PpdCache.
        DCHECK(ppd_basename.has_value());
        DCHECK(!ppd_basename->empty());
        DCHECK(!reference.effective_make_and_model.empty());

        ppd_cache_->Store(PpdBasenameToCacheKey(ppd_basename.value()),
                          ppd_contents);
        ppd_cache_->Store(PpdReferenceToCacheKey(reference),
                          ppd_basename.value());
        break;

      case ResolvedPpdOrigin::kFromUserSuppliedUrl:
        // No special considerations for a user-supplied PPD; we can
        // Store() it directly by mapping the user-supplied URI to the
        // PPD contents.
        DCHECK(!reference.user_supplied_ppd_url.empty());

        ppd_cache_->Store(PpdReferenceToCacheKey(reference), ppd_contents);
        break;
    }
  }

  // Continues a prior call to ResolvePpd().
  //
  // Called when we have the contents of the PPD being resolved; we are
  // on the cusp of being able to invoke the |cb|.
  void ResolvePpdWithContents(ResolvedPpdOrigin ppd_origin,
                              std::optional<std::string> ppd_basename,
                              std::string ppd_contents,
                              Printer::PpdReference reference,
                              ResolvePpdCallback cb) {
    DCHECK(!ppd_contents.empty());

    if (ppd_contents.size() > kMaxPpdSizeBytes) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), CallbackResultCode::PPD_TOO_LARGE, ""));
      return;
    }

    StorePpdWithContents(ppd_contents, std::move(ppd_basename), ppd_origin,
                         std::move(reference));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), CallbackResultCode::SUCCESS,
                                  std::move(ppd_contents)));
  }

  // Continues a prior call to ResolvePpd().
  //
  // Called back by PrinterConfigCache::Fetch() when we've fetched
  // a PPD from the serving root.
  void OnPpdFetchedFromServingRoot(
      Printer::PpdReference reference,
      ResolvePpdCallback cb,
      const PrinterConfigCache::FetchResult& result) {
    if (!result.succeeded || result.contents.empty()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), CallbackResultCode::SERVER_ERROR, ""));
      return;
    }

    ResolvePpdWithContents(ResolvedPpdOrigin::kFromServingRoot,
                           /*ppd_basename=*/result.key,
                           /*ppd_contents=*/result.contents,
                           std::move(reference), std::move(cb));
  }

  // Continues a prior call to ResolvePpd().
  //
  // Called when we seek a mapping from an effective-make-and-model
  // string to a PPD basename by querying the local PpdCache.
  void OnPpdBasenameSoughtInPpdCache(Printer::PpdReference reference,
                                     ResolvePpdCallback cb,
                                     const PpdCache::FindResult& result) {
    if (!result.success || result.contents.empty()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), CallbackResultCode::NOT_FOUND, ""));
      return;
    }

    std::string cache_key = PpdBasenameToCacheKey(result.contents);
    ppd_cache_->Find(
        cache_key,
        base::BindOnce(&PpdProviderImpl::OnPpdFromServingRootSoughtInPpdCache,
                       weak_factory_.GetWeakPtr(),
                       /*ppd_basename=*/result.contents, std::move(reference),
                       std::move(cb)));
  }

  // Continues a prior call to ResolvePpd().
  //
  // Called when we have a PPD basename already and seek its contents
  // in the local PpdCache.
  void OnPpdFromServingRootSoughtInPpdCache(
      const std::string& ppd_basename,
      Printer::PpdReference reference,
      ResolvePpdCallback cb,
      const PpdCache::FindResult& result) {
    if (!result.success || result.contents.empty()) {
      // We have the PPD basename, but not the contents of the PPD
      // itself in our local PpdCache. We must seek out the contents
      // from the serving root.
      auto callback = base::BindOnce(
          &PpdProviderImpl::OnPpdFetchedFromServingRoot,
          weak_factory_.GetWeakPtr(), std::move(reference), std::move(cb));
      config_cache_->Fetch(PpdPathInServingRoot(ppd_basename), kMaxDataAge,
                           std::move(callback));
      return;
    }

    ResolvePpdWithContents(ResolvedPpdOrigin::kFromPpdCache, ppd_basename,
                           result.contents, std::move(reference),
                           std::move(cb));
  }

  // Continues a prior call to ResolvePpd().
  //
  // Called back by PpdMetadataManager::FindAllEmmsAvailableInIndex().
  //
  // 1. Maps |reference|::effective_make_and_model to a PPD basename.
  //    a.  Attempts to do so with fresh forward index metadata if
  //        possible, searching |forward_index_subset| for the best
  //        available PPD.
  //    b.  Falls back to directly querying the local PpdCache instance,
  //        e.g. if the network is unreachable.
  // 2. Uses basename derived in previous step to retrieve the
  //    appropriate PPD from the local PpdCache instance.
  void OnPpdBasenameSoughtFromForwardIndex(
      Printer::PpdReference reference,
      ResolvePpdCallback cb,
      const base::flat_map<std::string, ParsedIndexValues>&
          forward_index_subset) {
    const ParsedIndexLeaf* const leaf = FirstAllowableParsedIndexLeaf(
        reference.effective_make_and_model, forward_index_subset);
    if (!leaf || leaf->ppd_basename.empty()) {
      // The forward index doesn't advise what the best fit PPD is for
      // |reference|::effective_make_and_model. We can look toward the
      // local PpdCache to see if we saved it previously.
      std::string cache_key = PpdReferenceToCacheKey(reference);
      ppd_cache_->Find(
          cache_key,
          base::BindOnce(&PpdProviderImpl::OnPpdBasenameSoughtInPpdCache,
                         weak_factory_.GetWeakPtr(), std::move(reference),
                         std::move(cb)));
      return;
    }

    // The forward index does advertise a best-fit PPD basename. We
    // check the local PpdCache to see if we already have it.
    PRINTER_LOG(DEBUG) << reference.effective_make_and_model << " mapped to "
                       << leaf->ppd_basename;
    ppd_cache_->Find(
        PpdBasenameToCacheKey(leaf->ppd_basename),
        base::BindOnce(&PpdProviderImpl::OnPpdFromServingRootSoughtInPpdCache,
                       weak_factory_.GetWeakPtr(), leaf->ppd_basename,
                       std::move(reference), std::move(cb)));
  }

  // Continues a prior call to ResolvePpd().
  //
  // Called when we finish searching the PpdCache for a user-supplied
  // PPD. This contrasts with the slightly more involved two-step
  // "dereference" process in searching the PpdCache for a PPD retrieved
  // from the serving root.
  void OnUserSuppliedPpdSoughtInPpdCache(
      Printer::PpdReference reference,
      CallbackResultCode result_if_unsuccessful,
      ResolvePpdCallback cb,
      const PpdCache::FindResult& result) {
    if (!result.success) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), result_if_unsuccessful, ""));
      return;
    }

    ResolvePpdWithContents(ResolvedPpdOrigin::kFromPpdCache,
                           /*ppd_basename=*/std::nullopt, result.contents,
                           std::move(reference), std::move(cb));
  }

  // Continues a prior call to ResolvePpd().
  //
  // Called when we finish fetching a PPD file from device-local storage
  // (e.g. from the user's home directory, not from the PpdCache).
  void OnUserSuppliedPpdFetchedFromLocalFile(Printer::PpdReference reference,
                                             ResolvePpdCallback cb,
                                             const std::string& result) {
    if (result.empty()) {
      // We didn't find a nonempty PPD at the location specified by the
      // user. Try searching the PpdCache and fail with NOT_FOUND if not found
      // in PpdCache.
      std::string cache_key = PpdReferenceToCacheKey(reference);
      ppd_cache_->Find(
          cache_key,
          base::BindOnce(&PpdProviderImpl::OnUserSuppliedPpdSoughtInPpdCache,
                         weak_factory_.GetWeakPtr(), std::move(reference),
                         CallbackResultCode::NOT_FOUND, std::move(cb)));
      return;
    }

    ResolvePpdWithContents(ResolvedPpdOrigin::kFromUserSuppliedUrl,
                           /*ppd_basename=*/std::nullopt, result,
                           std::move(reference), std::move(cb));
  }

  // Continues a prior call to ResolvePpd().
  //
  // Called when we finish fetching the contents of a PPD file from a remote
  // URL.
  void OnUserSuppliedPpdFetchedFromRemoteUrl(
      Printer::PpdReference reference,
      ResolvePpdCallback cb,
      RemotePpdFetcher::FetchResultCode code,
      std::string result) {
    if (code != RemotePpdFetcher::FetchResultCode::kSuccess) {
      // Fetching the PPD from remote URL was unsuccessful. Try searching the
      // PpdCache and fail with SERVER_ERROR if not found in PpdCache.
      std::string cache_key = PpdReferenceToCacheKey(reference);
      ppd_cache_->Find(
          cache_key,
          base::BindOnce(&PpdProviderImpl::OnUserSuppliedPpdSoughtInPpdCache,
                         weak_factory_.GetWeakPtr(), std::move(reference),
                         CallbackResultCode::SERVER_ERROR, std::move(cb)));
      return;
    }

    ResolvePpdWithContents(ResolvedPpdOrigin::kFromUserSuppliedUrl,
                           /*ppd_basename=*/std::nullopt, std::move(result),
                           std::move(reference), std::move(cb));
  }

  // Continues a prior call to ResolvePpd().
  //
  // 1. Attempts to invoke |cb| with the file named by
  //    |reference|::user_suplied_ppd_url - i.e. a live fetch from
  //    local disk or an http:// url.
  // 2. Attempts to search the local PpdCache instance for the file
  //    whose cache key was built from
  //    |reference|::user_supplied_ppd_url.
  void ResolveUserSuppliedPpd(Printer::PpdReference reference,
                              ResolvePpdCallback cb) {
    DCHECK(!reference.user_supplied_ppd_url.empty());
    GURL url(reference.user_supplied_ppd_url);
    if (url.SchemeIsHTTPOrHTTPS()) {
      ResolveUserSuppliedPpdFromRemoteUrl(url, std::move(reference),
                                          std::move(cb));
    } else {
      ResolveUserSuppliedPpdFromLocalFile(url, std::move(reference),
                                          std::move(cb));
    }
  }

  void ResolveUserSuppliedPpdFromLocalFile(GURL file_url,
                                           Printer::PpdReference reference,
                                           ResolvePpdCallback cb) {
    file_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&FetchFile, file_url),
        base::BindOnce(&PpdProviderImpl::OnUserSuppliedPpdFetchedFromLocalFile,
                       weak_factory_.GetWeakPtr(), std::move(reference),
                       std::move(cb)));
  }

  void ResolveUserSuppliedPpdFromRemoteUrl(GURL url,
                                           Printer::PpdReference reference,
                                           ResolvePpdCallback cb) {
    remote_ppd_fetcher_->Fetch(
        url,
        base::BindOnce(&PpdProviderImpl::OnUserSuppliedPpdFetchedFromRemoteUrl,
                       weak_factory_.GetWeakPtr(), std::move(reference),
                       std::move(cb)));
  }

  // Continues a prior call to ResolvePpdLicense().
  // This callback is fed to
  // PpdMetadataManager::FindAllEmmsAvailableInIndexCallback().
  void FindLicenseForEmm(const std::string& effective_make_and_model,
                         ResolvePpdLicenseCallback cb,
                         const base::flat_map<std::string, ParsedIndexValues>&
                             forward_index_results) {
    const ParsedIndexLeaf* const index_leaf = FirstAllowableParsedIndexLeaf(
        effective_make_and_model, forward_index_results);
    if (!index_leaf) {
      // This particular |effective_make_and_model| is invisible to the
      // current |version_|; either it is restricted or it is missing
      // entirely from the forward indices.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), CallbackResultCode::NOT_FOUND,
                         /*license_name=*/""));
      return;
    }

    // Note that the license can also be empty; this denotes that
    // no license is associated with this particular
    // |effective_make_and_model| in this |version_|.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), CallbackResultCode::SUCCESS,
                                  index_leaf->license));
  }

  // Current version used to filter restricted ppds
  const base::Version version_;

  // Provides PPD storage on-device.
  scoped_refptr<PpdCache> ppd_cache_;

  // Interacts with and controls PPD metadata.
  std::unique_ptr<PpdMetadataManager> metadata_manager_;

  // Fetches PPDs from the Chrome OS Printing team's serving root.
  std::unique_ptr<PrinterConfigCache> config_cache_;

  // Fetches PPDs from remote http:// or https:// URLs.
  std::unique_ptr<RemotePpdFetcher> remote_ppd_fetcher_;

  // Where to run disk operations.
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::WeakPtrFactory<PpdProviderImpl> weak_factory_{this};
};

}  // namespace

PrinterSearchData::PrinterSearchData() = default;
PrinterSearchData::PrinterSearchData(const PrinterSearchData& other) = default;
PrinterSearchData::~PrinterSearchData() = default;

// static
//
// Used in production but also exposed for testing.
std::string PpdProvider::PpdReferenceToCacheKey(
    const Printer::PpdReference& reference) {
  DCHECK(PpdReferenceIsWellFormed(reference));
  // The key prefixes here are arbitrary, but ensure we can't have an (unhashed)
  // collision between keys generated from different PpdReference fields.
  if (!reference.effective_make_and_model.empty()) {
    return base::StrCat(
        {"emm_for_metadata_v3:", reference.effective_make_and_model});
  } else {
    // Retains the legacy salt from the v2 PpdProvider. This is done
    // to minimize user breakage when we roll out the v3 PpdProvider.
    return base::StrCat({"up:", reference.user_supplied_ppd_url});
  }
}

// static
//
// Used in production but also exposed for testing.
std::string PpdProvider::PpdBasenameToCacheKey(std::string_view ppd_basename) {
  return base::StrCat({"ppd_basename_for_metadata_v3:", ppd_basename});
}

// static
scoped_refptr<PpdProvider> PpdProvider::Create(
    const base::Version& current_version,
    scoped_refptr<PpdCache> cache,
    std::unique_ptr<PpdMetadataManager> metadata_manager,
    std::unique_ptr<PrinterConfigCache> config_cache,
    std::unique_ptr<RemotePpdFetcher> remote_ppd_fetcher) {
  return base::MakeRefCounted<PpdProviderImpl>(
      current_version, cache, std::move(metadata_manager),
      std::move(config_cache), std::move(remote_ppd_fetcher));
}

// static
std::string_view PpdProvider::CallbackResultCodeName(CallbackResultCode code) {
  switch (code) {
    case SUCCESS:
      return "SUCCESS";
    case NOT_FOUND:
      return "NOT_FOUND";
    case SERVER_ERROR:
      return "SERVER_ERROR";
    case INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case PPD_TOO_LARGE:
      return "PPD_TOO_LARGE";
  }
}

}  // namespace chromeos
