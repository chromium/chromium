// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/printing/epson_driver_matching.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_metadata_manager.h"
#include "chromeos/printing/ppd_provider_v3.h"
#include "chromeos/printing/printer_config_cache.h"
#include "chromeos/printing/printer_configuration.h"
#include "net/base/backoff_entry.h"

namespace chromeos {
namespace {

// The exact queue length at which PpdProvider will begin to post
// failure callbacks in response to its queue-able public methods.
// Arbitrarily chosen.
// See also: struct MethodDeferralContext
constexpr size_t kMethodDeferralLimit = 20;

// Backoff policy for retrying
// PpdProviderImpl::TryToGetMetadataManagerLocale(). Specifies that we
// *  perform the first retry with a 1s delay,
// *  double the retry delay thereafter, and
// *  cap the retry delay at 32s.
//
// We perform backoff to prevent the PpdProviderImpl from running at
// full sequence speed if it continuously fails to obtain a metadata
// locale.
constexpr net::BackoffEntry::Policy kBackoffPolicy{
    /*num_errors_to_ignore=*/0,
    /*initial_delay_ms=*/1000,
    /*multiply_factor=*/2.0,
    /*jitter_factor=*/0.0,
    /*maximum_backoff_ms=*/32000LL,
    /*entry_lifetime_ms=*/-1LL,
    /*always_use_initial_delay=*/true};

// Age limit for time-sensitive API calls. Typically denotes "Please
// respond with data no older than kMaxDataAge." Arbitrarily chosen.
constexpr base::TimeDelta kMaxDataAge = base::TimeDelta::FromMinutes(30LL);

// Effective-make-and-model string that describes a printer capable of
// using the generic Epson PPD.
const char kEpsonGenericEmm[] = "epson generic escpr printer";

// Helper struct for PpdProviderImpl. Allows PpdProviderImpl to defer
// its public method calls, which PpdProviderImpl will do when the
// PpdMetadataManager is not ready to deal with locale-sensitive PPD
// metadata.
//
// Note that the semantics of this struct demand two things of the
// deferable public methods of PpdProviderImpl:
// 1. that they check for its presence and
// 2. that they check its |current_method_is_being_failed| member to
//    prevent infinite re-enqueueing of public methods once the queue
//    is full.
struct MethodDeferralContext {
  MethodDeferralContext() : backoff_entry(&kBackoffPolicy) {}
  ~MethodDeferralContext() = default;

  // This struct is not copyable.
  MethodDeferralContext(const MethodDeferralContext&) = delete;
  MethodDeferralContext& operator=(const MethodDeferralContext&) = delete;

  // Pops the first entry from |deferred_methods| and synchronously runs
  // it with the intent to fail it.
  void FailOneEnqueuedMethod() {
    DCHECK(!current_method_is_being_failed);

    // Explicitly activates the failure codepath for whatever public
    // method of PpdProviderImpl that we'll now Run().
    current_method_is_being_failed = true;

    std::move(deferred_methods.front()).Run();
    deferred_methods.pop();
    current_method_is_being_failed = false;
  }

  // Fails all |deferred_methods| synchronously.
  void FailAllEnqueuedMethods() {
    while (!deferred_methods.empty()) {
      FailOneEnqueuedMethod();
    }
  }

  // Dequeues and posts all |deferred_methods| onto our sequence.
  void FlushAndPostAll() {
    while (!deferred_methods.empty()) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(deferred_methods.front()));
      deferred_methods.pop();
    }
  }

  bool IsFull() { return deferred_methods.size() >= kMethodDeferralLimit; }

  // This bool is checked during execution of a queue-able public method
  // of PpdProviderImpl. If it is true, then
  // 1. the current queue-able public method was previously enqueued,
  // 2. the deferral queue is full, and so
  // 3. the current queue-able public method was posted for the sole
  //    purpose of being _failed_, and should not be re-enqueued.
  bool current_method_is_being_failed = false;

  base::queue<base::OnceCallback<void()>> deferred_methods;

  // Implements retry backoff for
  // PpdProviderImpl::TryToGetMetadataManagerLocale().
  net::BackoffEntry backoff_entry;
};

// This class implements the PpdProvider interface for the v3 metadata
// (https://crbug.com/888189).
class PpdProviderImpl : public PpdProvider {
 public:
  PpdProviderImpl(base::StringPiece browser_locale,
                  const base::Version& current_version,
                  scoped_refptr<PpdCache> cache,
                  std::unique_ptr<PpdMetadataManager> metadata_manager,
                  std::unique_ptr<PrinterConfigCache> config_cache)
      : browser_locale_(std::string(browser_locale)),
        version_(current_version),
        cache_(cache),
        deferral_context_(std::make_unique<MethodDeferralContext>()),
        metadata_manager_(std::move(metadata_manager)),
        config_cache_(std::move(config_cache)),
        file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
    // Immediately attempts to obtain a metadata locale.
    TryToGetMetadataManagerLocale();
  }

  void ResolveManufacturers(ResolveManufacturersCallback cb) override {
    // Do we need
    // 1. to defer this method?
    // 2. to fail this method (which was already previously deferred)?
    if (deferral_context_) {
      if (deferral_context_->current_method_is_being_failed) {
        auto failure_cb = base::BindOnce(
            std::move(cb), PpdProvider::CallbackResultCode::SERVER_ERROR,
            std::vector<std::string>());
        base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                         std::move(failure_cb));
        return;
      }

      if (deferral_context_->IsFull()) {
        deferral_context_->FailOneEnqueuedMethod();
        DCHECK(!deferral_context_->IsFull());
      }
      base::OnceCallback<void()> this_method =
          base::BindOnce(&PpdProviderImpl::ResolveManufacturers,
                         weak_factory_.GetWeakPtr(), std::move(cb));
      deferral_context_->deferred_methods.push(std::move(this_method));
      return;
    }

    metadata_manager_->GetManufacturers(kMaxDataAge, std::move(cb));
  }

  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {
    // Caller must not call ResolvePrinters() before a successful reply
    // from ResolveManufacturers(). ResolveManufacturers() cannot have
    // been successful if the |deferral_context_| still exists.
    if (deferral_context_) {
      auto failure_cb = base::BindOnce(
          std::move(cb), PpdProvider::CallbackResultCode::INTERNAL_ERROR,
          ResolvedPrintersList());
      base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                       std::move(failure_cb));
      return;
    }

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
  // *  This method is not locale-sensitive.
  void ResolvePpdReference(const PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {
    ResolvePpdReferenceContext context(search_data, std::move(cb));

    // Initiate step 1 if possible.
    if (!search_data.make_and_model.empty()) {
      auto callback = base::BindOnce(
          &PpdProviderImpl::TryToResolvePpdReferenceFromForwardIndices,
          weak_factory_.GetWeakPtr(), std::move(context));
      metadata_manager_->FindAllEmmsAvailableInIndex(
          search_data.make_and_model, kMaxDataAge, std::move(callback));
      return;
    }

    // Otherwise, jump straight to step 2.
    TryToResolvePpdReferenceFromUsbIndices(std::move(context));
  }

  // This method depends on a successful prior call to
  // ResolvePpdReference().
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {
    // Do we need
    // 1. to defer this method?
    // 2. to fail this method (which was already previously deferred)?
    if (deferral_context_) {
      if (deferral_context_->current_method_is_being_failed) {
        auto failure_cb = base::BindOnce(
            std::move(cb), PpdProvider::CallbackResultCode::SERVER_ERROR, "",
            "");
        base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                         std::move(failure_cb));
        return;
      }

      if (deferral_context_->IsFull()) {
        deferral_context_->FailOneEnqueuedMethod();
        DCHECK(!deferral_context_->IsFull());
      }
      base::OnceCallback<void()> this_method = base::BindOnce(
          &PpdProviderImpl::ReverseLookup, weak_factory_.GetWeakPtr(),
          effective_make_and_model, std::move(cb));
      deferral_context_->deferred_methods.push(std::move(this_method));
      return;
    }

    // TODO(crbug.com/888189): implement this.
  }

  // This method depends on forward indices, which are not
  // locale-sensitive.
  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    auto callback = base::BindOnce(
        &PpdProviderImpl::FindLicenseForEmm, weak_factory_.GetWeakPtr(),
        std::string(effective_make_and_model), std::move(cb));
    metadata_manager_->FindAllEmmsAvailableInIndex(
        {std::string(effective_make_and_model)}, kMaxDataAge,
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

  // Readies |metadata_manager_| to call methods which require a
  // successful callback from PpdMetadataManager::GetLocale().
  //
  // |this| is largely useless if its |metadata_manager_| is not ready
  // to traffick in locale-sensitive PPD metadata, so we want this
  // method to eventually succeed.
  void TryToGetMetadataManagerLocale() {
    auto callback =
        base::BindOnce(&PpdProviderImpl::OnMetadataManagerLocaleGotten,
                       weak_factory_.GetWeakPtr());
    metadata_manager_->GetLocale(std::move(callback));
  }

  // Evaluates true if our |version_| falls within the bounds set by
  // |restrictions|.
  bool CurrentVersionSatisfiesRestrictions(
      const Restrictions& restrictions) const {
    if ((restrictions.min_milestone.IsValid() &&
         version_ < restrictions.min_milestone) ||
        (restrictions.max_milestone.IsValid() &&
         version_ > restrictions.max_milestone)) {
      return false;
    }
    return true;
  }

  // Callback fed to PpdMetadataManager::GetLocale().
  void OnMetadataManagerLocaleGotten(bool succeeded) {
    if (!succeeded) {
      // Uh-oh, we concretely failed to get a metadata locale. We should
      // fail all outstanding deferred methods and let callers retry as
      // they see fit.
      deferral_context_->FailAllEnqueuedMethods();

      // Inform the BackoffEntry of our failure; let it adjust the
      // retry delay.
      deferral_context_->backoff_entry.InformOfRequest(false);

      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PpdProviderImpl::TryToGetMetadataManagerLocale,
                         weak_factory_.GetWeakPtr()),
          deferral_context_->backoff_entry.GetTimeUntilRelease());
      return;
    }
    deferral_context_->FlushAndPostAll();

    // It is no longer necessary to defer public method calls.
    deferral_context_.reset();
  }

  // Callback fed to PpdMetadataManager::GetPrinters().
  void OnPrintersGotten(ResolvePrintersCallback cb,
                        bool succeeded,
                        const ParsedPrinters& printers) {
    if (!succeeded) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), CallbackResultCode::SERVER_ERROR,
                         ResolvedPrintersList()));
      return;
    }

    ResolvedPrintersList printers_available_to_our_version;
    for (const ParsedPrinter& printer : printers) {
      if (!printer.restrictions.has_value() ||
          CurrentVersionSatisfiesRestrictions(printer.restrictions.value())) {
        Printer::PpdReference ppd_reference;
        ppd_reference.effective_make_and_model =
            printer.effective_make_and_model;
        printers_available_to_our_version.push_back(ResolvedPpdReference{
            printer.user_visible_printer_name, ppd_reference});
      }
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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
      base::StringPiece effective_make_and_model,
      const base::flat_map<std::string, ParsedIndexValues>&
          forward_index_subset) const {
    const auto& iter = forward_index_subset.find(effective_make_and_model);
    if (iter == forward_index_subset.end()) {
      return nullptr;
    }

    for (const ParsedIndexLeaf& index_leaf : iter->second.values) {
      if (!index_leaf.restrictions.has_value() ||
          CurrentVersionSatisfiesRestrictions(
              index_leaf.restrictions.value())) {
        return &index_leaf;
      }
    }

    return nullptr;
  }

  static void SuccessfullyResolvePpdReferenceWithEmm(
      base::StringPiece effective_make_and_model,
      ResolvePpdReferenceCallback cb) {
    Printer::PpdReference reference;
    reference.effective_make_and_model = std::string(effective_make_and_model);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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
    for (base::StringPiece effective_make_and_model :
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
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), CallbackResultCode::NOT_FOUND,
                         /*license_name=*/""));
      return;
    }

    // Note that the license can also be empty; this denotes that
    // no license is associated with this particular
    // |effective_make_and_model| in this |version_|.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), CallbackResultCode::SUCCESS,
                                  index_leaf->license));
  }

  // Locale of the browser, as returned by
  // BrowserContext::GetApplicationLocale();
  const std::string browser_locale_;

  // Current version used to filter restricted ppds
  const base::Version version_;

  // Provides PPD storage on-device.
  scoped_refptr<PpdCache> cache_;

  // Used to
  // 1. to determine if |this| should defer locale-sensitive public
  //    method calls and
  // 2. to defer those method calls, if necessary.
  // These deferrals are only necessary before the |metadata_manager_|
  // is ready to deal with locale-sensitive PPD metadata. This member is
  // reset once deferrals are unnecessary.
  std::unique_ptr<MethodDeferralContext> deferral_context_;

  // Interacts with and controls PPD metadata.
  std::unique_ptr<PpdMetadataManager> metadata_manager_;

  // Fetches PPDs from the Chrome OS Printing team's serving root.
  std::unique_ptr<PrinterConfigCache> config_cache_;

  // Where to run disk operations.
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::WeakPtrFactory<PpdProviderImpl> weak_factory_{this};
};

// Copied directly from v2 PpdProvider
// TODO(crbug.com/888189): figure out where this fits in the big picture
bool PpdReferenceIsWellFormed(const Printer::PpdReference& reference) {
  int filled_fields = 0;
  if (!reference.user_supplied_ppd_url.empty()) {
    ++filled_fields;
    GURL tmp_url(reference.user_supplied_ppd_url);
    if (!tmp_url.is_valid() || !tmp_url.SchemeIs("file")) {
      LOG(ERROR) << "Invalid url for a user-supplied ppd: "
                 << reference.user_supplied_ppd_url
                 << " (must be a file:// URL)";
      return false;
    }
  }
  if (!reference.effective_make_and_model.empty()) {
    ++filled_fields;
  }

  // All effective-make-and-model strings should be lowercased, since v2.
  // Since make-and-model strings could include non-Latin chars, only checking
  // that it excludes all upper-case chars A-Z.
  if (!std::all_of(reference.effective_make_and_model.begin(),
                   reference.effective_make_and_model.end(),
                   [](char c) -> bool { return !base::IsAsciiUpper(c); })) {
    return false;
  }
  // Should have exactly one non-empty field.
  return filled_fields == 1;
}

}  // namespace

PrinterSearchData::PrinterSearchData() = default;
PrinterSearchData::PrinterSearchData(const PrinterSearchData& other) = default;
PrinterSearchData::~PrinterSearchData() = default;

// static; copied directly from v2 PpdProvider
// TODO(crbug.com/888189): figure out where this fits in the big picture
std::string PpdProvider::PpdReferenceToCacheKey(
    const Printer::PpdReference& reference) {
  DCHECK(PpdReferenceIsWellFormed(reference));
  // The key prefixes here are arbitrary, but ensure we can't have an (unhashed)
  // collision between keys generated from different PpdReference fields.
  if (!reference.effective_make_and_model.empty()) {
    return std::string("em:") + reference.effective_make_and_model;
  } else {
    return std::string("up:") + reference.user_supplied_ppd_url;
  }
}

// static
scoped_refptr<PpdProvider> PpdProvider::Create(
    const std::string& browser_locale,
    network::mojom::URLLoaderFactory* loader_factory,
    scoped_refptr<PpdCache> ppd_cache,
    const base::Version& current_version,
    const PpdProvider::Options& options) {
  NOTREACHED();  // TODO(crbug.com/888189): deprecate this Create().
  return nullptr;
}

// free function; _not_ static
scoped_refptr<PpdProvider> CreateV3Provider(
    base::StringPiece browser_locale,
    const base::Version& current_version,
    scoped_refptr<PpdCache> cache,
    std::unique_ptr<PpdMetadataManager> metadata_manager,
    std::unique_ptr<PrinterConfigCache> config_cache) {
  return base::MakeRefCounted<PpdProviderImpl>(
      browser_locale, current_version, cache, std::move(metadata_manager),
      std::move(config_cache));
}

}  // namespace chromeos
