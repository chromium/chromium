// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_provider.h"

#include <algorithm>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/epson_driver_matching.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_line_reader.h"
#include "chromeos/printing/printing_constants.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

const char kEpsonGenericPPD[] = "epson generic escpr printer";
const char kLicenseKey[] = "license";

// Holds a metadata_v2 reverse-index response
struct ReverseIndexJSON {
  // Canonical name of printer
  std::string effective_make_and_model;

  // Name of printer manufacturer
  std::string manufacturer;

  // Name of printer model
  std::string model;

  // Restrictions for this manufacturer
  PpdProvider::LegacyRestrictions restrictions;
};

struct PpdLicenseJSON {
  // Canonical name of printer
  std::string effective_make_and_model;

  // Name of associated license. If empty, then there is no associated license.
  std::string license;
};

// Holds a metadata_v2 manufacturers response
struct ManufacturersJSON {
  // Name of printer manufacturer
  std::string name;

  // Key for lookup of this manutacturer's printers (JSON file)
  std::string reference;

  // Restrictions for this manufacturer
  PpdProvider::LegacyRestrictions restrictions;
};

// Holds a metadata_v2 printers response
struct PrintersJSON {
  // Name of printer
  std::string name;

  // Canonical name of printer
  std::string effective_make_and_model;

  // Restrictions for this printer
  PpdProvider::LegacyRestrictions restrictions;
};

// Holds a metadata_v2 ppd-index response
struct PpdIndexJSON {
  // Canonical name of printer
  std::string effective_make_and_model;

  // Ppd filename
  std::string ppd_filename;

  // Name of associated license. If empty, then there is no associated license.
  std::string license;
};

// A queued request to download printer information for a manufacturer.
// Note: Disabled copying/assigning since this holds a base::OnceCalback.
struct PrinterResolutionQueueEntry {
  PrinterResolutionQueueEntry() = default;
  PrinterResolutionQueueEntry(PrinterResolutionQueueEntry&& other) = default;
  ~PrinterResolutionQueueEntry() = default;

  // Localized manufacturer name
  std::string manufacturer;

  // URL we are going to pull from.
  GURL url;

  // User callback on completion.
  PpdProvider::ResolvePrintersCallback cb;

  DISALLOW_COPY_AND_ASSIGN(PrinterResolutionQueueEntry);
};

struct PpdLicenseQueueEntry {
  PpdLicenseQueueEntry() = default;
  PpdLicenseQueueEntry(const PpdLicenseQueueEntry&) = delete;
  PpdLicenseQueueEntry& operator=(const PpdLicenseQueueEntry&) = delete;
  PpdLicenseQueueEntry(PpdLicenseQueueEntry&& other) = default;
  ~PpdLicenseQueueEntry() = default;

  // Canonical printer name.
  std::string effective_make_and_model;

  // URL we are going to pull from.
  GURL url;

  // User callback upon completion of request.
  PpdProvider::ResolvePpdLicenseCallback cb;
};

// A queued request to download reverse index information for a make and model
// Note: Disabled copying/assigning since this holds a base::OnceCalback.
struct ReverseIndexQueueEntry {
  ReverseIndexQueueEntry() = default;
  ReverseIndexQueueEntry(ReverseIndexQueueEntry&& other) = default;
  ~ReverseIndexQueueEntry() = default;

  // Canonical Printer Name
  std::string effective_make_and_model;

  // URL we are going to pull from.
  GURL url;

  // User callback on completion.
  PpdProvider::ReverseLookupCallback cb;

  DISALLOW_COPY_AND_ASSIGN(ReverseIndexQueueEntry);
};

// Holds manufacturer to printers relation
struct ManufacturerMetadata {
  // Key used to look up the printer list on the server. This is initially
  // populated.
  std::string reference;

  // Map from localized printer name to canonical-make-and-model string for
  // the given printer. Populated on demand.
  std::unique_ptr<std::unordered_map<std::string, PrintersJSON>> printers;
};

// Carried information for an inflight PPD resolution.
// Note: Disabled copying/assigning since this holds a base::OnceCalback.
struct PpdResolutionQueueEntry {
  PpdResolutionQueueEntry() = default;
  PpdResolutionQueueEntry(PpdResolutionQueueEntry&& other) = default;
  ~PpdResolutionQueueEntry() = default;

  // Original reference being resolved.
  Printer::PpdReference reference;

  // If non-empty, the contents from the cache for this resolution.
  std::string cached_contents;

  // Callback to be invoked on completion.
  PpdProvider::ResolvePpdCallback callback;

  DISALLOW_COPY_AND_ASSIGN(PpdResolutionQueueEntry);
};

// Carried information for an inflight PPD reference resolution.
// Note: Disabled copying/assigning since this holds a base::OnceCalback.
struct PpdReferenceResolutionQueueEntry {
  PpdReferenceResolutionQueueEntry() = default;
  PpdReferenceResolutionQueueEntry(PpdReferenceResolutionQueueEntry&& other) =
      default;
  ~PpdReferenceResolutionQueueEntry() = default;

  // Metadata used to resolve to a unique PpdReference object.
  PrinterSearchData search_data;

  // If true, we have failed usb_index_resolution already.
  bool usb_resolution_attempted = false;

  // Callback to be invoked on completion.
  PpdProvider::ResolvePpdReferenceCallback cb;

  DISALLOW_COPY_AND_ASSIGN(PpdReferenceResolutionQueueEntry);
};


// Returns false if there are obvious errors in the reference that will prevent
// resolution.
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

// Fetch the file pointed at by |url| and store it in |file_contents|.
// Returns true if the fetch was successful.
bool FetchFile(const GURL& url, std::string* file_contents) {
  CHECK(url.is_valid());
  CHECK(url.SchemeIs("file"));
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Here we are un-escaping the file path represented by the url. If we don't
  // transform the url into a valid file path then the file may fail to be
  // opened by the system later.
  base::FilePath path;
  if (!net::FileURLToFilePath(url, &path)) {
    LOG(ERROR) << "Not a valid file URL.";
    return false;
  }
  return base::ReadFileToString(path, file_contents);
}

std::string ComputeLicense(const base::Value& dict) {
  std::string license;
  const std::string* found = dict.FindStringKey(kLicenseKey);
  if (found) {
    license = *found;
  }
  return license;
}

// Constructs and returns a printers' restrictions parsed from |dict|.
PpdProvider::LegacyRestrictions ComputeRestrictions(const base::Value& dict) {
  DCHECK(dict.is_dict());
  PpdProvider::LegacyRestrictions restrictions;

  const base::Value* min_milestone =
      dict.FindKeyOfType({"min_milestone"}, base::Value::Type::DOUBLE);
  const base::Value* max_milestone =
      dict.FindKeyOfType({"max_milestone"}, base::Value::Type::DOUBLE);

  if (min_milestone) {
    restrictions.min_milestone =
        base::Version(base::NumberToString(min_milestone->GetDouble()));
  }
  if (max_milestone) {
    restrictions.max_milestone =
        base::Version(base::NumberToString(max_milestone->GetDouble()));
  }

  return restrictions;
}

// Returns true if this |printer| is restricted from the
// |current_version|.
bool IsPrinterRestricted(const PrintersJSON& printer,
                         const base::Version& current_version) {
  const PpdProvider::LegacyRestrictions& restrictions = printer.restrictions;

  if (restrictions.min_milestone != base::Version("0.0") &&
      restrictions.min_milestone > current_version) {
    return true;
  }

  if (restrictions.max_milestone != base::Version("0.0") &&
      restrictions.max_milestone < current_version) {
    return true;
  }

  return false;
}

// Modifies |printers| by removing any restricted printers excluded from the
// current |version|, as judged by IsPrinterPermitted.
void FilterRestrictedPpdReferences(const base::Version& version,
                                   std::vector<PrintersJSON>* printers) {
  base::EraseIf(*printers, [&version](const PrintersJSON& printer) {
    return IsPrinterRestricted(printer, version);
  });
}

// TODO(crbug.com/953968): Implement network lookup to PPD server to get the USB
// manufacturer.
const std::unordered_map<int, std::string>& GetVendorIdMap() {
  static base::NoDestructor<std::unordered_map<int, std::string>> keys(
      {{0x05ac, "Apple"},
       {0x04f9, "Brother"},
       {0x04a9, "Canon"},
       {0x049f, "Compaq"},
       {0x413c, "Dell"},
       {0x04b8, "Epson"},
       {0x0550, "Fuji Xerox"},
       {0x03f0, "HP"},
       {0x040a, "Kodak"},
       {0x0482, "Kyocera"},
       {0x043d, "LexMark"},
       {0x0409, "NEC"},
       {0x06bc, "Oki"},
       {0x04da, "Panasonic"},
       {0x05ca, "Ricoh"},
       {0x04e8, "Samsung"},
       {0x04dd, "Sharp"},
       {0x0930, "Toshiba"},
       {0x0924, "Xerox"}});
  return *keys;
}

class PpdProviderImpl : public PpdProvider {
 public:
  // What kind of thing is the fetcher currently fetching?  We use this to
  // determine what to do when the fetcher returns a result.
  enum FetcherTarget {
    FT_LOCALES,        // Locales metadata.
    FT_MANUFACTURERS,  // List of manufacturers metadata.
    FT_PRINTERS,       // List of printers from a manufacturer.
    FT_PPD_INDEX,      // Master ppd index.
    FT_PPD,            // A Ppd file.
    FT_REVERSE_INDEX,  // List of sharded printers from a manufacturer
    FT_USB_DEVICES,    // USB device id to canonical name map.
    FT_LICENSE,        // License information for a PPD file.
  };

  PpdProviderImpl(const std::string& browser_locale,
                  LoaderFactoryGetter loader_factory_getter,
                  scoped_refptr<PpdCache> ppd_cache,
                  const base::Version& current_version,
                  const PpdProvider::Options& options)
      : browser_locale_(browser_locale),
        loader_factory_getter_(loader_factory_getter),
        ppd_cache_(ppd_cache),
        disk_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
        version_(current_version),
        options_(options) {}

  // Resolving manufacturers requires a couple of steps, because of
  // localization.  First we have to figure out what locale to use, which
  // involves grabbing a list of available locales from the server.  Once we
  // have decided on a locale, we go out and fetch the manufacturers map in that
  // localization.
  //
  // This means when a request comes in, we either queue it and start background
  // fetches if necessary, or we satisfy it immediately from memory.
  void ResolveManufacturers(ResolveManufacturersCallback cb) override {
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "ResolveManufacturers must be called from a SequencedTaskRunner"
           "context";
    if (cached_metadata_.get() != nullptr) {
      // We already have this in memory.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::SUCCESS,
                                    GetManufacturerList()));
      return;
    }
    manufacturers_resolution_queue_.push_back(std::move(cb));
    MaybeStartFetch();
  }

  // If there are any queued ppd reference resolutions, attempt to make progress
  // on them.  Returns true if a fetch was started, false if no fetch was
  // started.
  bool MaybeStartNextPpdReferenceResolution() {
    while (!ppd_reference_resolution_queue_.empty()) {
      auto& next = ppd_reference_resolution_queue_.front();
      auto& search_data = next.search_data;

      // Have we successfully resolved next yet?
      bool resolved_next = false;
      if (!search_data.make_and_model.empty()) {
        // Check the index for each make-and-model guess.
        for (const std::string& make_and_model : search_data.make_and_model) {
          // Check if we need to load its ppd_index
          int ppd_index_shard = IndexShard(make_and_model);
          if (!base::Contains(cached_ppd_idxs_, ppd_index_shard)) {
            StartFetch(GetPpdIndexURL(ppd_index_shard), FT_PPD_INDEX);
            return true;
          }
          if (base::Contains(cached_ppd_idxs_[ppd_index_shard],
                             make_and_model)) {
            // Found a hit, satisfy this resolution.
            RunPpdReferenceResolutionSucceeded(std::move(next.cb),
                                               make_and_model);
            resolved_next = true;
            break;
          }
        }
      }
      if (resolved_next)
        continue;

      // If we get to this point, either we don't have any make and model
      // guesses for the front entry, or they all missed.  Try USB ids
      // instead.
      if (!next.usb_resolution_attempted && search_data.usb_vendor_id &&
          search_data.usb_product_id) {
        StartFetch(GetUsbURL(search_data.usb_vendor_id), FT_USB_DEVICES);
        return true;
      }

      // If possible, here we fall back to OEM designated generic PPDs.
      if (CanUseEpsonGenericPPD(search_data)) {
        // Found a hit, satisfy this resolution.
        RunPpdReferenceResolutionSucceeded(std::move(next.cb),
                                           kEpsonGenericPPD);
      } else {
        // We don't have anything else left to try.
        if (search_data.discovery_type ==
            PrinterSearchData::PrinterDiscoveryType::kUsb) {
          // We've reached unsupported USB printer, try to grab the manufacturer
          // name.
          ResolveUsbManufacturer(std::move(next.cb), search_data.usb_vendor_id);
        } else {
          // Non-USB printer, so we fail resolution normally.
          RunPpdReferenceResolutionNotFound(std::move(next.cb),
                                            "" /* Empty Manufacturer */);
        }
      }
    }
    // Didn't start any fetches.
    return false;
  }

  // If there is work outstanding that requires a URL fetch to complete, start
  // going on it.
  void MaybeStartFetch() {
    if (fetch_inflight_) {
      // We'll call this again when the outstanding fetch completes.
      return;
    }

    if (MaybeStartNextPpdReferenceResolution()) {
      return;
    }

    if (!manufacturers_resolution_queue_.empty() ||
        !reverse_index_resolution_queue_.empty()) {
      if (locale_.empty()) {
        // Don't have a locale yet, figure that out first.
        StartFetch(GetLocalesURL(), FT_LOCALES);
      } else {
        // Get manufacturers based on the locale we have.
        if (!manufacturers_resolution_queue_.empty()) {
          StartFetch(GetManufacturersURL(locale_), FT_MANUFACTURERS);
        } else if (!reverse_index_resolution_queue_.empty()) {
          // Update the url with the locale before fetching
          ReverseIndexQueueEntry& entry =
              reverse_index_resolution_queue_.front();
          entry.url = GetReverseIndexURL(entry.effective_make_and_model);
          StartFetch(entry.url, FT_REVERSE_INDEX);
        }
      }
      return;
    }
    if (!printers_resolution_queue_.empty()) {
      StartFetch(printers_resolution_queue_.front().url, FT_PRINTERS);
      return;
    }
    while (!ppd_resolution_queue_.empty()) {
      auto& next = ppd_resolution_queue_.front();
      if (!next.reference.user_supplied_ppd_url.empty()) {
        DCHECK(next.reference.effective_make_and_model.empty());
        GURL url(next.reference.user_supplied_ppd_url);
        DCHECK(url.is_valid());
        StartFetch(url, FT_PPD);
        return;
      }
      DCHECK(!next.reference.effective_make_and_model.empty());
      int ppd_index_shard = IndexShard(next.reference.effective_make_and_model);
      if (!base::Contains(cached_ppd_idxs_, ppd_index_shard)) {
        // Have to have the ppd index before we can resolve by ppd server
        // key.
        StartFetch(GetPpdIndexURL(ppd_index_shard), FT_PPD_INDEX);
        return;
      }
      // Get the URL from the ppd index and start the fetch.
      auto& cached_ppd_index = cached_ppd_idxs_[ppd_index_shard];
      auto it = cached_ppd_index.find(next.reference.effective_make_and_model);
      if (it != cached_ppd_index.end()) {
        StartFetch(GetPpdURL(it->second), FT_PPD);
        return;
      }
      // This ppd reference isn't in the index.  That's not good. Fail
      // out the current resolution and go try to start the next
      // thing if there is one.
      LOG(ERROR) << "PPD " << next.reference.effective_make_and_model
                 << " not found in server index";

      FinishPpdResolution(std::move(next.callback), {},
                          PpdProvider::INTERNAL_ERROR);
      ppd_resolution_queue_.pop_front();
    }
    if (!ppd_license_resolution_queue_.empty()) {
      StartFetch(ppd_license_resolution_queue_.front().url, FT_LICENSE);
    }
  }

  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {
    std::unordered_map<std::string, ManufacturerMetadata>::iterator it;
    if (cached_metadata_.get() == nullptr ||
        (it = cached_metadata_->find(manufacturer)) ==
            cached_metadata_->end()) {
      // User error.
      LOG(ERROR) << "Can't resolve printers for unknown manufacturer "
                 << manufacturer;
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::INTERNAL_ERROR,
                                    ResolvedPrintersList()));
      return;
    }
    if (it->second.printers.get() != nullptr) {
      // Satisfy from the cache.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::SUCCESS,
                                    GetManufacturerPrinterList(it->second)));
    } else {
      // We haven't resolved this manufacturer yet.
      PrinterResolutionQueueEntry entry;
      entry.manufacturer = manufacturer;
      entry.url = GetPrintersURL(it->second.reference);
      entry.cb = std::move(cb);
      printers_resolution_queue_.push_back(std::move(entry));
      MaybeStartFetch();
    }
  }

  void ResolvePpdReference(const PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {
    // In v2 metadata, we work with lowercased effective_make_and_models.
    PrinterSearchData lowercase_search_data(search_data);
    for (auto& make_and_model : lowercase_search_data.make_and_model) {
      make_and_model = base::ToLowerASCII(make_and_model);
    }

    PpdReferenceResolutionQueueEntry entry;
    entry.search_data = lowercase_search_data;
    entry.cb = std::move(cb);
    ppd_reference_resolution_queue_.push_back(std::move(entry));
    MaybeStartFetch();
  }

  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {
    // In v2 metadata, we work with lowercased effective_make_and_models.
    Printer::PpdReference lowercase_reference(reference);
    lowercase_reference.effective_make_and_model =
        base::ToLowerASCII(lowercase_reference.effective_make_and_model);

    // Do a sanity check here, so we can assume |reference| is well-formed in
    // the rest of this class.
    if (!PpdReferenceIsWellFormed(lowercase_reference)) {
      FinishPpdResolution(std::move(cb), {}, PpdProvider::INTERNAL_ERROR);
      return;
    }

    // First step, check the cache.  If the cache lookup fails, we'll (try to)
    // consult the server.
    ppd_cache_->Find(PpdReferenceToCacheKey(lowercase_reference),
                     base::BindOnce(&PpdProviderImpl::ResolvePpdCacheLookupDone,
                                    weak_factory_.GetWeakPtr(),
                                    lowercase_reference, std::move(cb)));
  }

  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    if (effective_make_and_model.empty()) {
      LOG(WARNING) << "Cannot resolve an empty make and model";
      PostResolvePpdLicenseFailure(PpdProvider::NOT_FOUND, std::move(cb));
      return;
    }

    // In v2 metadata, we work with lowercased effective_make_and_models.
    std::string lowercase_effective_make_and_model =
        base::ToLowerASCII(effective_make_and_model);

    // Check to see if |lowercase_effective_make_and_model| is already present
    // in |cached_licenses_|.
    auto iter = cached_licenses_.find(lowercase_effective_make_and_model);
    if (iter != cached_licenses_.end()) {
      PostResolvePpdLicenseSuccess(std::move(cb), iter->second);
      return;
    }

    PpdLicenseQueueEntry entry;
    entry.effective_make_and_model = lowercase_effective_make_and_model;
    entry.url = GetPpdIndexURL(lowercase_effective_make_and_model);
    entry.cb = std::move(cb);
    ppd_license_resolution_queue_.push_back(std::move(entry));
    MaybeStartFetch();
  }

  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {
    if (effective_make_and_model.empty()) {
      LOG(WARNING) << "Cannot resolve an empty make and model";
      PostReverseLookupFailure(PpdProvider::NOT_FOUND, std::move(cb));
      return;
    }

    // In v2 metadata, we work with lowercased effective_make_and_models.
    std::string lowercase_effective_make_and_model =
        base::ToLowerASCII(effective_make_and_model);

    ReverseIndexQueueEntry entry;
    entry.effective_make_and_model = lowercase_effective_make_and_model;
    entry.url = GetReverseIndexURL(lowercase_effective_make_and_model);
    entry.cb = std::move(cb);
    reverse_index_resolution_queue_.push_back(std::move(entry));
    MaybeStartFetch();
  }

  // Common handler that gets called whenever a fetch completes.  Note this
  // is used both for |fetcher_| fetches (i.e. http[s]) and file-based fetches;
  // |source| may be null in the latter case.
  void OnURLFetchComplete(std::unique_ptr<std::string> body) {
    response_body_ = std::move(body);

    switch (fetcher_target_) {
      case FT_LOCALES:
        OnLocalesFetchComplete();
        break;
      case FT_MANUFACTURERS:
        OnManufacturersFetchComplete();
        break;
      case FT_PRINTERS:
        OnPrintersFetchComplete();
        break;
      case FT_PPD_INDEX:
        OnPpdIndexFetchComplete(fetcher_->GetFinalURL());
        break;
      case FT_PPD:
        OnPpdFetchComplete();
        break;
      case FT_REVERSE_INDEX:
        OnReverseIndexComplete();
        break;
      case FT_USB_DEVICES:
        OnUsbFetchComplete();
        break;
      case FT_LICENSE:
        OnLicenseFetchComplete();
        break;
      default:
        LOG(DFATAL) << "Unknown fetch source";
    }
    fetch_inflight_ = false;
    MaybeStartFetch();
  }

 private:
  // Return the URL used to look up the supported locales list.
  GURL GetLocalesURL() {
    return GURL(options_.ppd_server_root + "/metadata_v2/locales.json");
  }

  GURL GetUsbURL(int vendor_id) {
    DCHECK_GT(vendor_id, 0);
    DCHECK_LE(vendor_id, 0xffff);

    return GURL(base::StringPrintf("%s/metadata_v2/usb-%04x.json",
                                   options_.ppd_server_root.c_str(),
                                   vendor_id));
  }

  // Return the URL used to get the |ppd_index_shard| index.
  GURL GetPpdIndexURL(int ppd_index_shard) {
    return GURL(base::StringPrintf("%s/metadata_v2/index-%02d.json",
                                   options_.ppd_server_root.c_str(),
                                   ppd_index_shard));
  }

  // Return the URL of the index shard associated with
  // |effective_make_and_model|.
  GURL GetPpdIndexURL(const std::string& effective_make_and_model) {
    return GetPpdIndexURL(IndexShard(effective_make_and_model));
  }

  // Return the ppd index shard number from its |url|.
  int GetShardFromUrl(const GURL& url) {
    auto url_str = url.spec();
    if (url_str.empty()) {
      return -1;
    }

    // Strip shard number from 2 digits following 'index'
    int idx_pos = url_str.find_first_of("0123456789", url_str.find("index-"));
    int shard_number;
    return base::StringToInt(url_str.substr(idx_pos, 2), &shard_number)
               ? shard_number
               : -1;
  }

  // Return the URL to get a localized manufacturers map.
  GURL GetManufacturersURL(const std::string& locale) {
    return GURL(base::StringPrintf("%s/metadata_v2/manufacturers-%s.json",
                                   options_.ppd_server_root.c_str(),
                                   locale.c_str()));
  }

  // Return the URL used to get a list of printers from the manufacturer |ref|.
  GURL GetPrintersURL(const std::string& ref) {
    return GURL(base::StringPrintf(
        "%s/metadata_v2/%s", options_.ppd_server_root.c_str(), ref.c_str()));
  }

  // Return the URL used to get a ppd with the given filename.
  GURL GetPpdURL(const std::string& filename) {
    return GURL(base::StringPrintf(
        "%s/ppds/%s", options_.ppd_server_root.c_str(), filename.c_str()));
  }

  // Return the URL to get a localized, shared manufacturers map.
  GURL GetReverseIndexURL(const std::string& effective_make_and_model) {
    return GURL(base::StringPrintf("%s/metadata_v2/reverse_index-%s-%02d.json",
                                   options_.ppd_server_root.c_str(),
                                   locale_.c_str(),
                                   IndexShard(effective_make_and_model)));
  }

  // Create and return a fetcher that has the usual (for this class) flags set
  // and calls back to OnURLFetchComplete in this class when it finishes.
  void StartFetch(const GURL& url, FetcherTarget target) {
    DCHECK(!fetch_inflight_);
    DCHECK_EQ(fetcher_.get(), nullptr);
    fetcher_target_ = target;
    fetch_inflight_ = true;

    if (url.SchemeIs("http") || url.SchemeIs("https")) {
      auto resource_request = std::make_unique<network::ResourceRequest>();
      resource_request->url = url;
      resource_request->load_flags =
          net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
      resource_request->credentials_mode =
          network::mojom::CredentialsMode::kOmit;

      // TODO(luum): confirm correct traffic annotation
      fetcher_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                  MISSING_TRAFFIC_ANNOTATION);

      // TODO(luum): consider using unbounded size
      fetcher_->DownloadToString(
          loader_factory_getter_.Run(),
          base::BindOnce(&PpdProviderImpl::OnURLFetchComplete, this),
          network::SimpleURLLoader::kMaxBoundedStringDownloadSize);

    } else if (url.SchemeIs("file")) {
      auto file_contents = std::make_unique<std::string>();
      std::string* content_ptr = file_contents.get();
      base::PostTaskAndReplyWithResult(
          disk_task_runner_.get(), FROM_HERE,
          base::BindOnce(&FetchFile, url, content_ptr),
          base::BindOnce(&PpdProviderImpl::OnFileFetchComplete, this,
                         std::move(file_contents)));
    }
  }

  // Handle the result of a file fetch.
  void OnFileFetchComplete(std::unique_ptr<std::string> file_contents,
                           bool success) {
    file_fetch_success_ = success;
    file_fetch_contents_ = success ? *file_contents : "";
    OnURLFetchComplete(nullptr);
  }

  // Tidy up loose ends on an outstanding PPD resolution.
  //
  // If |ppd_contents| is non-empty, the request is resolved successfully
  // using those contents.  Otherwise |error_code| and an empty ppd
  // is given to |cb|.
  void FinishPpdResolution(ResolvePpdCallback cb,
                           const std::string& ppd_contents,
                           PpdProvider::CallbackResultCode error_code) {
    if (!ppd_contents.empty() && error_code != SUCCESS) {
      error_code = SUCCESS;
      LOG(WARNING) << "Resolved from cache due to network issue";
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), error_code, ppd_contents));
  }

  // Callback when the cache lookup for a ppd request finishes.  If we hit in
  // the cache, satisfy the resolution, otherwise kick it over to the fetcher
  // queue to be grabbed from a server.
  void ResolvePpdCacheLookupDone(const Printer::PpdReference& reference,
                                 ResolvePpdCallback cb,
                                 const PpdCache::FindResult& result) {
    // If all of the following are true, we use the cache result now:
    //
    // * It was a cache hit
    // * The reference is not from a user-supplied ppd file
    // * The cached data was fresh.
    //
    // In all other cases, we go through the full resolution flow (passing along
    // the cached data, if we got any), even if we got something from the cache.
    if (result.success && reference.user_supplied_ppd_url.empty() &&
        result.age < options_.cache_staleness_age) {
      DCHECK(!result.contents.empty());
      FinishPpdResolution(std::move(cb), result.contents, PpdProvider::SUCCESS);
    } else {
      // Save the cache result (if any), and queue up for resolution.
      ppd_resolution_queue_.push_back(
          {reference, result.contents, std::move(cb)});
      MaybeStartFetch();
    }
  }

  // Handler for the completion of the locales fetch.  This response should be a
  // list of strings, each of which is a locale in which we can answer queries
  // on the server.  The server (should) guarantee that we get 'en' as an
  // absolute minimum.
  //
  // Combine this information with the browser locale to figure out the best
  // locale to use, and then start a fetch of the manufacturers map in that
  // locale.
  void OnLocalesFetchComplete() {
    std::string contents;
    if (ValidateAndGetResponseAsString(&contents) != PpdProvider::SUCCESS) {
      FailQueuedMetadataResolutions(PpdProvider::SERVER_ERROR);
      return;
    }

    base::Optional<base::Value> top_list = base::JSONReader::Read(contents);
    if (!top_list.has_value() || !top_list.value().is_list()) {
      // We got something malformed back.
      FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
      return;
    }

    // This should just be a simple list of locale strings.
    std::vector<std::string> available_locales;
    bool found_en = false;
    for (const base::Value& entry : top_list.value().GetList()) {
      std::string tmp;
      // Locales should have at *least* a two-character country code.  100 is an
      // arbitrary upper bound for length to protect against extreme bogosity.
      if (!entry.GetAsString(&tmp) || tmp.size() < 2 || tmp.size() > 100) {
        FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
        return;
      }
      if (tmp == "en") {
        found_en = true;
      }
      available_locales.push_back(tmp);
    }
    if (available_locales.empty() || !found_en) {
      // We have no locales, or we didn't get an english locale (which is our
      // ultimate fallback)
      FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
      return;
    }
    // Everything checks out, set the locale, head back to fetch dispatch
    // to start the manufacturer fetch.
    locale_ = GetBestLocale(available_locales);
  }

  // Called when the |fetcher_| is expected have the results of a
  // manufacturer map (which maps localized manufacturer names to keys for
  // looking up printers from that manufacturer).  Use this information to
  // populate manufacturer_map_, and resolve all queued ResolveManufacturers()
  // calls.
  void OnManufacturersFetchComplete() {
    DCHECK_EQ(nullptr, cached_metadata_.get());
    std::vector<ManufacturersJSON> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParseManufacturersJSON(&contents);
    if (code != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Failed manufacturer parsing";
      FailQueuedMetadataResolutions(code);
      return;
    }
    cached_metadata_ = std::make_unique<
        std::unordered_map<std::string, ManufacturerMetadata>>();

    for (const auto& entry : contents) {
      (*cached_metadata_)[entry.name].reference = entry.reference;
    }

    std::vector<std::string> manufacturer_list = GetManufacturerList();
    // Complete any queued manufacturer resolutions.
    for (auto& cb : manufacturers_resolution_queue_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::SUCCESS,
                                    manufacturer_list));
    }
    manufacturers_resolution_queue_.clear();
  }

  // The outstanding fetch associated with the front of
  // |printers_resolution_queue_| finished, use the response to satisfy that
  // ResolvePrinters() call.
  void OnPrintersFetchComplete() {
    CHECK(cached_metadata_.get() != nullptr);
    DCHECK(!printers_resolution_queue_.empty());
    std::vector<PrintersJSON> contents;

    PpdProvider::CallbackResultCode code =
        ValidateAndParsePrintersJSON(&contents);

    if (code != PpdProvider::SUCCESS) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(printers_resolution_queue_.front().cb), code,
                         ResolvedPrintersList()));
    } else {
      // This should be a list of lists of 2-element strings, where the first
      // element is the localized name of the printer and the second element
      // is the canonical name of the printer.
      const std::string& manufacturer =
          printers_resolution_queue_.front().manufacturer;
      auto it = cached_metadata_->find(manufacturer);

      // If we kicked off a resolution, the entry better have already been
      // in the map.
      CHECK(it != cached_metadata_->end());

      // Create the printer map in the cache, and populate it.
      auto& manufacturer_metadata = it->second;
      CHECK(manufacturer_metadata.printers.get() == nullptr);
      manufacturer_metadata.printers =
          std::make_unique<std::unordered_map<std::string, PrintersJSON>>();

      for (const auto& entry : contents) {
        manufacturer_metadata.printers->insert({entry.name, entry});
      }

      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(printers_resolution_queue_.front().cb),
                         PpdProvider::SUCCESS,
                         GetManufacturerPrinterList(manufacturer_metadata)));
    }
    printers_resolution_queue_.pop_front();
  }

  // Called when |fetcher_| should have just received an index mapping
  // ppd server keys to ppd filenames.  Use this to populate
  // |cached_ppd_idxs_|.
  void OnPpdIndexFetchComplete(GURL url) {
    std::vector<PpdIndexJSON> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParsePpdIndexJSON(&contents);
    if (code != PpdProvider::SUCCESS) {
      FailQueuedServerPpdResolutions(code);
    } else {
      int ppd_index_shard = GetShardFromUrl(url);
      if (ppd_index_shard < 0) {
        FailQueuedServerPpdResolutions(PpdProvider::INTERNAL_ERROR);
        return;
      }
      auto& cached_ppd_index = cached_ppd_idxs_[ppd_index_shard];
      for (const auto& entry : contents) {
        cached_ppd_index.insert(
            {entry.effective_make_and_model, entry.ppd_filename});
        // Cache the license information while we have it.
        cached_licenses_.insert(
            {entry.effective_make_and_model, entry.license});
      }
    }
  }

  // This is called when |fetcher_| should have just downloaded a ppd.  If we
  // downloaded something successfully, use it to satisfy the front of the ppd
  // resolution queue, otherwise fail out that resolution.
  void OnPpdFetchComplete() {
    DCHECK(!ppd_resolution_queue_.empty());
    std::string contents;
    auto& entry = ppd_resolution_queue_.front();
    if ((ValidateAndGetResponseAsString(&contents) != PpdProvider::SUCCESS)) {
      //  If we cannot retrieve a PPD from the server, we want the user to be
      //  able to keep working.  So, if we retrieved a PPD from cache, even if
      //  it's stale, provide it to the caller.
      if (entry.cached_contents.empty()) {
        FinishPpdResolution(std::move(entry.callback), std::string(),
                            PpdProvider::SERVER_ERROR);
      } else {
        LOG(WARNING) << "Using stale cache PPD.  Unable to fetch from server";
        FinishPpdResolution(std::move(entry.callback), entry.cached_contents,
                            PpdProvider::SUCCESS);
      }
    } else if (contents.size() > kMaxPpdSizeBytes) {
      FinishPpdResolution(std::move(entry.callback), entry.cached_contents,
                          PpdProvider::PPD_TOO_LARGE);
    } else if (contents.empty()) {
      FinishPpdResolution(std::move(entry.callback), entry.cached_contents,
                          PpdProvider::INTERNAL_ERROR);
    } else {
      // Success.  Cache it and return it to the user.
      ppd_cache_->Store(
          PpdReferenceToCacheKey(ppd_resolution_queue_.front().reference),
          contents);
      FinishPpdResolution(std::move(entry.callback), contents,
                          PpdProvider::SUCCESS);
    }
    ppd_resolution_queue_.pop_front();
  }

  // This is called when |fetch_| should have just downloaded an index file. If
  // the index was downloaded successfully it is used to to determine the PPD
  // license associated with a printer.
  void OnLicenseFetchComplete() {
    DCHECK(!ppd_license_resolution_queue_.empty());
    std::vector<PpdLicenseJSON> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParseLicenseJSON(&contents);
    PpdLicenseQueueEntry entry =
        std::move(ppd_license_resolution_queue_.front());
    ppd_license_resolution_queue_.pop_front();

    if (code != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Request Failed or failed to parse index contents";
      PostResolvePpdLicenseFailure(code, std::move(entry.cb));
      return;
    }

    auto found =
        std::find_if(contents.begin(), contents.end(),
                     [&entry](const PpdLicenseJSON& license_json) -> bool {
                       return license_json.effective_make_and_model ==
                              entry.effective_make_and_model;
                     });

    if (found == contents.end()) {
      LOG(ERROR) << "Failed to lookup printer in retrieved license response";
      PostResolvePpdLicenseFailure(PpdProvider::NOT_FOUND, std::move(entry.cb));
      return;
    }

    // Place the resolved license into the cache
    cached_licenses_.insert({found->effective_make_and_model, found->license});

    PostResolvePpdLicenseSuccess(std::move(entry.cb), found->license);
  }

  // This is called when |fetch_| should have just downloaded a reverse index
  // file. If we downloaded something successfully, used the downloaded results
  // to satisfy the callback in the first item of the reverse index resolution
  // queue.
  void OnReverseIndexComplete() {
    DCHECK(!reverse_index_resolution_queue_.empty());
    std::vector<ReverseIndexJSON> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParseReverseIndexJSON(&contents);
    auto& entry = reverse_index_resolution_queue_.front();

    if (code != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Request Failed or failed reverse index parsing";
      PostReverseLookupFailure(code, std::move(entry.cb));
    } else {
      auto found = std::find_if(contents.begin(), contents.end(),
                                [&entry](const ReverseIndexJSON& rij) -> bool {
                                  return rij.effective_make_and_model ==
                                         entry.effective_make_and_model;
                                });
      if (found != contents.end()) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(entry.cb), PpdProvider::SUCCESS,
                                      found->manufacturer, found->model));
      } else {
        LOG(ERROR) << "Failed to lookup printer in retrieved data response";
        PostReverseLookupFailure(PpdProvider::NOT_FOUND, std::move(entry.cb));
      }
    }
    reverse_index_resolution_queue_.pop_front();
  }

  // Called when |fetcher_| should have just downloaded a usb device map
  // for the vendor at the head of the |ppd_reference_resolution_queue_|.
  void OnUsbFetchComplete() {
    DCHECK(!ppd_reference_resolution_queue_.empty());
    std::string contents;
    std::string buffer;
    PpdProvider::CallbackResultCode result =
        ValidateAndGetResponseAsString(&buffer);
    int desired_device_id =
        ppd_reference_resolution_queue_.front().search_data.usb_product_id;
    if (result == PpdProvider::SUCCESS) {
      // Parse the JSON response.  This should be a list of the form
      // [
      //  [0x3141, "some canonical name"],
      //  [0x5926, "some othercanonical name"]
      // ]
      // So we scan through the response looking for our desired device id.
      base::Optional<base::Value> top_list = base::JSONReader::Read(buffer);
      if (!top_list.has_value() || !top_list.value().is_list()) {
        // We got something malformed back.
        LOG(ERROR) << "Malformed top list";
        result = PpdProvider::INTERNAL_ERROR;
      } else {
        // We'll set result to SUCCESS if we do find the device.
        result = PpdProvider::NOT_FOUND;
        for (const auto& entry : top_list.value().GetList()) {
          int device_id;
          const base::ListValue* sub_list;

          // Each entry should be a size-2 list with an integer and a string.
          if (!entry.GetAsList(&sub_list) || sub_list->GetSize() != 2 ||
              !sub_list->GetInteger(0, &device_id) ||
              !sub_list->GetString(1, &contents) || device_id < 0 ||
              device_id > 0xffff) {
            // Malformed data.
            LOG(ERROR) << "Malformed line in usb device list";
            result = PpdProvider::INTERNAL_ERROR;
            break;
          }
          if (device_id == desired_device_id) {
            // Found it.
            result = PpdProvider::SUCCESS;
            break;
          }
        }
      }
    }
    if (result == PpdProvider::SUCCESS) {
      RunPpdReferenceResolutionSucceeded(
          std::move(ppd_reference_resolution_queue_.front().cb), contents);
    } else {
      ppd_reference_resolution_queue_.front().usb_resolution_attempted = true;
    }
  }

  // Something went wrong during metadata fetches.  Fail all queued metadata
  // resolutions with the given error code.
  void FailQueuedMetadataResolutions(PpdProvider::CallbackResultCode code) {
    for (auto& cb : manufacturers_resolution_queue_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), code, std::vector<std::string>()));
    }
    manufacturers_resolution_queue_.clear();
  }

  // Give up on all server-based ppd and ppd reference resolutions inflight,
  // because we failed to grab the necessary index data from the server.
  //
  // User-based ppd resolutions, which depend on local files, are left in the
  // queue.
  //
  // Other entries use cached data, if they found some, or failed outright.
  void FailQueuedServerPpdResolutions(PpdProvider::CallbackResultCode code) {
    base::circular_deque<PpdResolutionQueueEntry> filtered_queue;
    for (auto& entry : ppd_resolution_queue_) {
      if (!entry.reference.user_supplied_ppd_url.empty()) {
        filtered_queue.emplace_back(std::move(entry));
      } else {
        FinishPpdResolution(std::move(entry.callback), entry.cached_contents,
                            code);
      }
    }
    ppd_resolution_queue_ = std::move(filtered_queue);

    // Everything in the PpdReference queue also depends on server information,
    // so should also be failed.
    auto task_runner = base::SequencedTaskRunnerHandle::Get();
    for (auto& entry : ppd_reference_resolution_queue_) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(entry.cb), code, Printer::PpdReference(),
                         "" /* usb_manufacturer */));
    }
    ppd_reference_resolution_queue_.clear();
  }

  // Given a list of possible locale strings (e.g. 'en-GB'), determine which of
  // them we should use to best serve results for the browser locale (which was
  // given to us at construction time).
  std::string GetBestLocale(const std::vector<std::string>& available_locales) {
    // First look for an exact match.  If we find one, just use that.
    for (const std::string& available : available_locales) {
      if (available == browser_locale_) {
        return available;
      }
    }

    // Next, look for an available locale that is a parent of browser_locale_.
    // Return the most specific one.  For example, if we want 'en-GB-foo' and we
    // don't have an exact match, but we do have 'en-GB' and 'en', we will
    // return 'en-GB' -- the most specific match which is a parent of the
    // requested locale.
    size_t best_len = 0;
    size_t best_idx = -1;
    for (size_t i = 0; i < available_locales.size(); ++i) {
      const std::string& available = available_locales[i];
      if (base::StartsWith(browser_locale_, available + "-") &&
          available.size() > best_len) {
        best_len = available.size();
        best_idx = i;
      }
    }
    if (best_idx != static_cast<size_t>(-1)) {
      return available_locales[best_idx];
    }

    // Last chance for a match, look for the locale that matches the *most*
    // pieces of locale_, with ties broken by being least specific.  So for
    // example, if we have 'es-GB', 'es-GB-foo' but no 'es' available, and we're
    // requesting something for 'es', we'll get back 'es-GB' -- the least
    // specific thing that matches some of the locale.
    std::vector<base::StringPiece> browser_locale_pieces =
        base::SplitStringPiece(browser_locale_, "-", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_ALL);
    size_t best_match_size = 0;
    size_t best_match_specificity;
    best_idx = -1;
    for (size_t i = 0; i < available_locales.size(); ++i) {
      const std::string& available = available_locales[i];
      std::vector<base::StringPiece> available_pieces = base::SplitStringPiece(
          available, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      size_t match_size = 0;
      for (; match_size < available_pieces.size() &&
             match_size < browser_locale_pieces.size();
           ++match_size) {
        if (available_pieces[match_size] != browser_locale_pieces[match_size]) {
          break;
        }
      }
      if (match_size > 0 &&
          (best_idx == static_cast<size_t>(-1) ||
           match_size > best_match_size ||
           (match_size == best_match_size &&
            available_pieces.size() < best_match_specificity))) {
        best_idx = i;
        best_match_size = match_size;
        best_match_specificity = available_pieces.size();
      }
    }
    if (best_idx != static_cast<size_t>(-1)) {
      return available_locales[best_idx];
    }

    // Everything else failed.  Throw up our hands and default to english.
    return "en";
  }

  // Get the results of a fetch.  This is a little tricky, because a fetch
  // may have been done by |fetcher_|, or it may have been a file access, in
  // which case we want to look at |file_fetch_contents_|.  We distinguish
  // between the cases based on whether or not |fetcher_| is null.
  //
  // We return NOT_FOUND for 404 or file not found, SERVER_ERROR for other
  // errors, SUCCESS if everything was good.
  CallbackResultCode ValidateAndGetResponseAsString(std::string* contents) {
    CallbackResultCode ret;
    if (fetcher_.get() != nullptr) {
      int net_error = fetcher_->NetError();
      if (net_error != net::OK) {
        ret = net_error == net::ERR_FILE_NOT_FOUND ? PpdProvider::NOT_FOUND
                                                   : PpdProvider::SERVER_ERROR;
      } else if (response_body_.get() == nullptr) {
        ret = PpdProvider::SERVER_ERROR;
      } else {
        *contents = std::move(*response_body_);
        ret = PpdProvider::SUCCESS;
      }
      fetcher_.reset();
    } else {
      // It's a file load.
      if (file_fetch_success_) {
        *contents = file_fetch_contents_;
      } else {
        contents->clear();
      }
      // A failure to load a file is always considered a NOT FOUND error (even
      // if the underlying causes is lack of access or similar, this seems to be
      // the best match for intent.
      ret = file_fetch_success_ ? PpdProvider::SUCCESS : PpdProvider::NOT_FOUND;
      file_fetch_contents_.clear();
    }
    return ret;
  }

  // Ensures that the fetched JSON is in the expected format, that is leading
  // with exactly |num_strings| and followed by an optional dictionary. Returns
  // PpdProvider::SUCCESS and saves JSON list in |top_list| if format is valid,
  // returns PpdProvider::INTERNAL_ERROR otherwise.
  PpdProvider::CallbackResultCode ParseAndValidateJSONFormat(
      base::Value::ListStorage* top_list,
      size_t num_strings) {
    std::string buffer;

    auto fetch_result = ValidateAndGetResponseAsString(&buffer);
    if (fetch_result != PpdProvider::SUCCESS) {
      return fetch_result;
    }

    base::Optional<base::Value> ret_list = base::JSONReader::Read(buffer);
    if (!ret_list.has_value()) {
      LOG(ERROR) << "Failed to read contents of retrieved JSON";
      return PpdProvider::INTERNAL_ERROR;
    }

    if (!ret_list.value().is_list()) {
      LOG(ERROR) << "JSON object is not a list";
      return PpdProvider::INTERNAL_ERROR;
    }

    *top_list = ret_list->TakeList();
    for (const auto& entry : *top_list) {
      if (!entry.is_list()) {
        LOG(ERROR) << "Found unexpected non-list entry in JSON object";
        return PpdProvider::INTERNAL_ERROR;
      }

      // entry must start with |num_strings| strings
      base::Value::ConstListView list = entry.GetList();
      if (list.size() < num_strings) {
        LOG(ERROR) << "List is smaller than expected";
        return PpdProvider::INTERNAL_ERROR;
      }
      for (size_t i = 0; i < num_strings; ++i) {
        if (!list[i].is_string()) {
          LOG(ERROR) << "Found unexpected non-string value in list";
          return PpdProvider::INTERNAL_ERROR;
        }
      }

      // entry may not have more than |num_strings| strings and one dict
      if (list.size() > num_strings + 1) {
        LOG(ERROR) << "List is larger than expected";
        return PpdProvider::INTERNAL_ERROR;
      }

      // entry may optionally have a last arg that must be a dict
      if (list.size() == num_strings + 1 && !list[num_strings].is_dict()) {
        LOG(ERROR) << "List size exceeds " << num_strings
                   << " and final element is not a dictionary";
        return PpdProvider::INTERNAL_ERROR;
      }
    }

    return PpdProvider::SUCCESS;
  }

  // Convenience function which logs the error message associated with the value
  // of |result|. The given |type| is used to indicate which type of JSON
  // metadata file the validation error occurred on.
  void LogJSONValidationError(const std::string& type,
                              PpdProvider::CallbackResultCode result) {
    DCHECK(result != PpdProvider::SUCCESS);
    switch (result) {
      case PpdProvider::NOT_FOUND:
        LOG(ERROR) << "Could not find the " << type << " metadata file";
        break;
      case PpdProvider::SERVER_ERROR:
        LOG(ERROR) << "Failed to retrieve the " << type
                   << " metadata from the server";
        break;
      case PpdProvider::INTERNAL_ERROR:
        LOG(ERROR) << "Failed to parse the " << type << " metadata";
        break;
      default:
        break;
    }
  }

  // Attempts to parse the PpdIndexJSON reply to |fetcher| into the passed
  // contents. This function differs from ValidateAndParsePpdIndexJSON in that
  // here were are only concerned with the value of the optional "license" field
  // and not the name of the PPD file. Returns PpdProvider::SUCCESS on valid
  // JSON formatting and filled |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParseLicenseJSON(
      std::vector<PpdLicenseJSON>* contents) {
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 2);
    if (ret != PpdProvider::SUCCESS) {
      LogJSONValidationError("PpdIndex", ret);
      return ret;
    }

    for (const auto& entry : top_list) {
      base::span<const base::Value> list = entry.GetList();
      PpdLicenseJSON license_json;
      license_json.effective_make_and_model = list[0].GetString();
      if (list.size() == 3) {
        license_json.license = ComputeLicense(list[2]);
      }
      contents->push_back(std::move(license_json));
    }

    return PpdProvider::SUCCESS;
  }

  // Attempts to parse a ReverseIndexJSON reply to |fetcher| into the passed
  // contents. Returns PpdProvider::SUCCESS on valid JSON formatting and filled
  // |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParseReverseIndexJSON(
      std::vector<ReverseIndexJSON>* contents) {
    DCHECK(contents != nullptr);
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 3);
    if (ret != PpdProvider::SUCCESS) {
      LogJSONValidationError("ReverseIndex", ret);
      return ret;
    }

    // Fetched data should be in the form {[effective_make_and_model],
    // [manufacturer], [model], [dictionary of metadata]}
    for (const auto& entry : top_list) {
      base::Value::ConstListView list = entry.GetList();

      ReverseIndexJSON rij_entry;
      rij_entry.effective_make_and_model = list[0].GetString();
      rij_entry.manufacturer = list[1].GetString();
      rij_entry.model = list[2].GetString();

      // Populate restrictions, if available
      if (list.size() > 3) {
        rij_entry.restrictions = ComputeRestrictions(list[3]);
      }

      contents->push_back(rij_entry);
    }
    return PpdProvider::SUCCESS;
  }

  // Attempts to parse a ManufacturersJSON reply to |fetcher| into the passed
  // contents. Returns PpdProvider::SUCCESS on valid JSON formatting and filled
  // |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParseManufacturersJSON(
      std::vector<ManufacturersJSON>* contents) {
    DCHECK(contents != nullptr);
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 2);
    if (ret != PpdProvider::SUCCESS) {
      LogJSONValidationError("Manufacturers", ret);
      return ret;
    }

    // Fetched data should be in form [[name], [canonical name],
    // {restrictions}]
    for (const auto& entry : top_list) {
      base::Value::ConstListView list = entry.GetList();
      ManufacturersJSON mj_entry;
      mj_entry.name = list[0].GetString();
      mj_entry.reference = list[1].GetString();

      // Populate restrictions, if available
      if (list.size() > 2) {
        mj_entry.restrictions = ComputeRestrictions(list[2]);
      }

      contents->push_back(mj_entry);
    }

    return PpdProvider::SUCCESS;
  }

  // Attempts to parse a PrintersJSON reply to |fetcher| into the passed
  // contents. Returns PpdProvider::SUCCESS on valid JSON formatting and filled
  // |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParsePrintersJSON(
      std::vector<PrintersJSON>* contents) {
    DCHECK(contents != nullptr);
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 2);
    if (ret != PpdProvider::SUCCESS) {
      LogJSONValidationError("Printers", ret);
      return ret;
    }

    // Fetched data should be in form [[name], [canonical name],
    // {restrictions}]
    for (const auto& entry : top_list) {
      base::Value::ConstListView list = entry.GetList();
      PrintersJSON pj_entry;
      pj_entry.name = list[0].GetString();
      pj_entry.effective_make_and_model = list[1].GetString();

      // Populate restrictions, if available
      if (list.size() > 2) {
        pj_entry.restrictions = ComputeRestrictions(list[2]);
      }

      contents->push_back(pj_entry);
    }

    return PpdProvider::SUCCESS;
  }

  // Attempts to parse a PpdIndexJSON reply to |fetcher| into the passed
  // contents. Returns PpdProvider::SUCCESS on valid JSON formatting and filled
  // |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParsePpdIndexJSON(
      std::vector<PpdIndexJSON>* contents) {
    DCHECK(contents != nullptr);
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 2);
    if (ret != PpdProvider::SUCCESS) {
      LogJSONValidationError("PpdIndex", ret);
      return ret;
    }

    // Fetched data should be in the form {[effective_make_and_model],
    // [manufacturer], [model], [dictionary of metadata]}
    for (const auto& entry : top_list) {
      base::Value::ConstListView list = entry.GetList();

      PpdIndexJSON pij_entry;
      pij_entry.effective_make_and_model = list[0].GetString();
      pij_entry.ppd_filename = list[1].GetString();
      // Compute the license information in order to cache it for later.
      if (list.size() == 3) {
        pij_entry.license = ComputeLicense(list[2]);
      }
      contents->push_back(pij_entry);
    }
    return PpdProvider::SUCCESS;
  }

  // Create the list of manufacturers from |cached_metadata_|.  Requires that
  // the manufacturer list has already been resolved.
  std::vector<std::string> GetManufacturerList() const {
    CHECK(cached_metadata_.get() != nullptr);
    std::vector<std::string> ret;
    ret.reserve(cached_metadata_->size());
    for (const auto& entry : *cached_metadata_) {
      ret.push_back(entry.first);
    }
    // TODO(justincarlson) -- this should be a localization-aware sort.
    sort(ret.begin(), ret.end());
    return ret;
  }

  // Get the list of printers from a given manufacturer from |cached_metadata_|.
  // Requires that we have already resolved this from the server.
  ResolvedPrintersList GetManufacturerPrinterList(
      const ManufacturerMetadata& meta) const {
    CHECK(meta.printers.get() != nullptr);
    std::vector<PrintersJSON> printers;
    printers.reserve(meta.printers->size());
    for (const auto& entry : *meta.printers) {
      printers.push_back(entry.second);
    }
    // TODO(justincarlson) -- this should be a localization-aware sort.
    sort(printers.begin(), printers.end(),
         [](const PrintersJSON& a, const PrintersJSON& b) -> bool {
           return a.name < b.name;
         });
    FilterRestrictedPpdReferences(version_, &printers);

    ResolvedPrintersList ret;
    ret.reserve(printers.size());
    for (const auto& printer : printers) {
      Printer::PpdReference ppd_ref;
      ppd_ref.effective_make_and_model = printer.effective_make_and_model;
      ret.push_back({printer.name, ppd_ref});
    }
    return ret;
  }

  void ResolveUsbManufacturer(ResolvePpdReferenceCallback cb, int vendor_id) {
    std::string manufacturer;
    if (base::Contains(GetVendorIdMap(), vendor_id)) {
      manufacturer = GetVendorIdMap().at(vendor_id);
    } else {
      LOG(ERROR) << "Unable to find vendor_id: " << vendor_id;
    }
    // This look up is done asynchronously since we will later be using a server
    // look up for the manufacturer name.
    RunPpdReferenceResolutionNotFound(std::move(cb), manufacturer);
  }

  void PostResolvePpdLicenseSuccess(ResolvePpdLicenseCallback cb,
                                    const std::string& license) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb), PpdProvider::SUCCESS, license));
  }

  // Convenience function for issuing a failure response to ResolvePpdLicense().
  // Posts the callback |cb| with the given |result|. The value of the returned
  // license string should not matter since there is an error response.
  void PostResolvePpdLicenseFailure(CallbackResultCode result,
                                    ResolvePpdLicenseCallback cb) {
    DCHECK_NE(result, PpdProvider::SUCCESS);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), result, std::string()));
  }

  void PostReverseLookupFailure(CallbackResultCode result,
                                ReverseLookupCallback cb) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb), result, std::string(), std::string()));
  }

  // Helper function that runs |cb| with the PpdProvider::SUCCESS as the result.
  void RunPpdReferenceResolutionSucceeded(ResolvePpdReferenceCallback cb,
                                          const std::string& make_and_model) {
    DCHECK(!ppd_reference_resolution_queue_.empty());

    Printer::PpdReference ret;
    ret.effective_make_and_model = make_and_model;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::SUCCESS, ret,
                                  "" /* usb_manufacturer */));
    ppd_reference_resolution_queue_.pop_front();
  }

  // Helper function that runs |cb| with the PpdProvider::NOT_FOUND as the
  // result.
  void RunPpdReferenceResolutionNotFound(ResolvePpdReferenceCallback cb,
                                         const std::string& manufacturer) {
    DCHECK(!ppd_reference_resolution_queue_.empty());

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), PpdProvider::NOT_FOUND,
                                  Printer::PpdReference(), manufacturer));
    ppd_reference_resolution_queue_.pop_front();
  }

  // The hash function to calculate the hash of canonical identifiers to the
  // name of the ppd file for that printer.
  int IndexShard(std::string effective_make_and_model) {
    unsigned int hash = 5381;
    int kNumIndexShards = 20;

    for (char c : effective_make_and_model) {
      hash = hash * 33 + c;
    }
    return hash % kNumIndexShards;
  }

  // Map from (localized) manufacturer name to metadata for that manufacturer.
  // This is populated lazily.  If we don't yet have a manufacturer list, the
  // top pointer will be null.  When we create the top level map, then each
  // value will only contain a reference which can be used to resolve the
  // printer list from that manufacturer.  On demand, we use these references to
  // resolve the actual printer lists.
  std::unique_ptr<std::unordered_map<std::string, ManufacturerMetadata>>
      cached_metadata_;

  // Cached contents of the server indexs, which maps first a shard number to
  // the corresponding index map of PpdReference::effective_make_and_models to a
  // urls for the corresponding ppds. Starts as an empty map and filled lazily
  // as we need to fill in more indexs.
  std::unordered_map<int, std::unordered_map<std::string, std::string>>
      cached_ppd_idxs_;

  // Caches mappings between effective_make_and_model values and the name of
  // their associated license.
  std::unordered_map<std::string, std::string> cached_licenses_;

  // Queued ResolveManufacturers() calls.  We will simultaneously resolve
  // all queued requests, so no need for a deque here.
  std::vector<ResolveManufacturersCallback> manufacturers_resolution_queue_;

  // Queued ResolvePrinters() calls.
  base::circular_deque<PrinterResolutionQueueEntry> printers_resolution_queue_;

  // Queued ResolvePpd() requests.
  base::circular_deque<PpdResolutionQueueEntry> ppd_resolution_queue_;

  // Queued ResolvePpdReference() requests.
  base::circular_deque<PpdReferenceResolutionQueueEntry>
      ppd_reference_resolution_queue_;

  // Queued ResolvePpdLicense() requests;
  base::circular_deque<PpdLicenseQueueEntry> ppd_license_resolution_queue_;

  // Queued ReverseIndex() calls.
  base::circular_deque<ReverseIndexQueueEntry> reverse_index_resolution_queue_;

  // Locale we're using for grabbing stuff from the server.  Empty if we haven't
  // determined it yet.
  std::string locale_;

  // If the fetcher is active, what's it fetching?
  FetcherTarget fetcher_target_;

  // Fetcher used for all network fetches.  This is explicitly reset() when
  // a fetch has been processed.
  std::unique_ptr<network::SimpleURLLoader> fetcher_;
  bool fetch_inflight_ = false;
  std::unique_ptr<std::string> response_body_;

  // Locale of the browser, as returned by
  // BrowserContext::GetApplicationLocale();
  const std::string browser_locale_;

  LoaderFactoryGetter loader_factory_getter_;

  // For file:// fetches, a staging buffer and result flag for loading the file.
  std::string file_fetch_contents_;
  bool file_fetch_success_;

  // Cache of ppd files.
  scoped_refptr<PpdCache> ppd_cache_;

  // Where to run disk operations.
  scoped_refptr<base::SequencedTaskRunner> disk_task_runner_;

  // Current version used to filter restricted ppds
  base::Version version_;

  // Construction-time options, immutable.
  const PpdProvider::Options options_;

  base::WeakPtrFactory<PpdProviderImpl> weak_factory_{this};

 protected:
  ~PpdProviderImpl() override = default;
};

}  // namespace

// static
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

PrinterSearchData::PrinterSearchData() = default;
PrinterSearchData::PrinterSearchData(const PrinterSearchData& other) = default;
PrinterSearchData::~PrinterSearchData() = default;

// static
scoped_refptr<PpdProvider> PpdProvider::Create(
    const std::string& browser_locale,
    LoaderFactoryGetter loader_factory_getter,
    scoped_refptr<PpdCache> ppd_cache,
    const base::Version& current_version,
    const PpdProvider::Options& options) {
  return scoped_refptr<PpdProvider>(
      new PpdProviderImpl(browser_locale, loader_factory_getter, ppd_cache,
                          current_version, options));
}
}  // namespace chromeos
