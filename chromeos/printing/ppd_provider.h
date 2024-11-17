// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_PROVIDER_H_
#define CHROMEOS_PRINTING_PPD_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/version.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/usb_printer_id.h"

namespace network::mojom {
class URLLoaderFactory;
}

namespace chromeos {

class PpdCache;
class PrinterConfigCache;
class PpdMetadataManager;
class RemotePpdFetcher;

// Everything we might know about a printer when looking for a
// driver for it.  All of the default values for fields in this struct
// mean we *don't* have that piece of information.
//
// Fields are listed in search order preference -- we use earlier
// fields first to attempt to find a match.
struct COMPONENT_EXPORT(CHROMEOS_PRINTING) PrinterSearchData {
  PrinterSearchData();
  PrinterSearchData(const PrinterSearchData& other);
  ~PrinterSearchData();

  // Make-and-model string guesses.
  std::vector<std::string> make_and_model;

  // 16-bit usb identifiers.
  int usb_vendor_id = 0;
  int usb_product_id = 0;

  // Original make and model for USB printer. Note, it is used only in metrics
  // for USB printers (in printer_event_tracker.cc).
  std::string usb_manufacturer;
  std::string usb_model;

  // Method of printer discovery.
  enum PrinterDiscoveryType {
    kUnknown = 0,
    kManual = 1,
    kUsb = 2,
    kZeroconf = 3,
    kDiscoveryTypeMax
  };
  PrinterDiscoveryType discovery_type;

  // Set of MIME types supported by this printer.
  std::vector<std::string> supported_document_formats;

  // Representation of IEEE1284 standard printing device ID.
  // Contains a set of languages this printer understands.
  UsbPrinterId printer_id;
};

// PpdProvider is responsible for mapping printer descriptions to
// CUPS-PostScript Printer Description (PPD) files.  It provides PPDs that a
// user previously identified for use, and falls back to querying quirksserver
// based on manufacturer/model of the printer.
//
// All functions in this class must be called from a sequenced context.
class COMPONENT_EXPORT(CHROMEOS_PRINTING) PpdProvider
    : public base::RefCounted<PpdProvider> {
 public:
  // Possible result codes of a Resolve*() call.
  enum CallbackResultCode {
    SUCCESS,

    // Looked for a PPD for this configuration, but couldn't find a match.
    // Never returned for QueryAvailable().
    NOT_FOUND,

    // Failed to contact an external server needed to finish resolution.
    SERVER_ERROR,

    // Other error that is not expected to be transient.
    INTERNAL_ERROR,

    // The provided PPD was too large to be processed.
    PPD_TOO_LARGE,
  };

  // Defines the limitations on when we show a particular PPD
  // Not to be confused with the new Restrictions struct used in the
  // v3 PpdProvider, defined in ppd_metadata_parser.h
  struct LegacyRestrictions {
    // Minimum milestone for ChromeOS build
    base::Version min_milestone = base::Version("0.0");

    // Maximum milestone for ChromeOS build
    base::Version max_milestone = base::Version("0.0");
  };

  struct ResolvedPpdReference {
    // The name of the model of printer or printer line
    std::string name;

    // Correct PpdReferece to be used with this printer
    Printer::PpdReference ppd_ref;
  };

  // Result of a ResolvePpd() call.
  // If the result code is SUCCESS, then:
  //    string holds the contents of a PPD (that may or may not be gzipped).
  // Otherwise, these fields will be empty.
  using ResolvePpdCallback =
      base::OnceCallback<void(CallbackResultCode, const std::string&)>;

  // Result of a ResolveManufacturers() call.  If the result code is SUCCESS,
  // then the vector contains a sorted list of manufacturers for which we have
  // at least one printer driver.
  using ResolveManufacturersCallback =
      base::OnceCallback<void(CallbackResultCode,
                              const std::vector<std::string>&)>;

  // A list of printer names paired with the PpdReference that should be used
  // for that printer.
  using ResolvedPrintersList = std::vector<ResolvedPpdReference>;

  // Result of a ResolvePrinters() call.  If the result code is SUCCESS, then
  // the vector contains a sorted list <model_name, PpdReference> tuples of all
  // printer models from the given manufacturer for which we have a driver,
  // sorted by model_name.
  using ResolvePrintersCallback =
      base::OnceCallback<void(CallbackResultCode, const ResolvedPrintersList&)>;

  // Result of a ResolvePpdReference call.  If the result code is SUCCESS, then
  // the second argument contains the a PpdReference that we have high
  // confidence can be used to obtain a driver for the printer.  NOT_FOUND means
  // we couldn't confidently figure out a driver for the printer.  If we got
  // NOT_FOUND from a USB printer, we may have been able to determine the
  // manufacturer name which is the third argument.
  using ResolvePpdReferenceCallback =
      base::OnceCallback<void(CallbackResultCode,
                              const Printer::PpdReference& ref,
                              const std::string& manufacturer)>;

  // Result of a ResolvePpdLicense call. If |result| is SUCCESS, then
  // |license_name| will be used to indicate the license associated with the
  // requested PPD. If |license_name| is empty, then the requested PPD does not
  // require a license.
  using ResolvePpdLicenseCallback =
      base::OnceCallback<void(CallbackResultCode result,
                              const std::string& license_name)>;

  // Result of a ReverseLookup call.  If the result code is SUCCESS, then
  // |manufactuer| and |model| contain the strings that could have generated
  // the reference being looked up.
  using ReverseLookupCallback =
      base::OnceCallback<void(CallbackResultCode,
                              const std::string& manufacturer,
                              const std::string& model)>;

  // Called to get the current URLLoaderFactory on demand. Needs to be
  // Repeating since it gets called once per fetch.
  using LoaderFactoryGetter =
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>;

  // Create and return a new PpdProvider with the given cache and options.
  // A references to |url_context_getter| is taken.
  static scoped_refptr<PpdProvider> Create(
      const base::Version& current_version,
      scoped_refptr<PpdCache> cache,
      std::unique_ptr<PpdMetadataManager> metadata_manager,
      std::unique_ptr<PrinterConfigCache> config_cache,
      std::unique_ptr<RemotePpdFetcher> remote_ppd_fetcher);

  // Return a printable name for |code|.
  static std::string_view CallbackResultCodeName(CallbackResultCode code);

  // Get all manufacturers for which we have drivers.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  //
  // PpdProvider will enqueue calls to this method and answer them in
  // the order received; it will opt to invoke |cb| with failure if the
  // queue grows overlong, failing the oldest calls first. The exact
  // queue length at which this occurs is unspecified.
  virtual void ResolveManufacturers(ResolveManufacturersCallback cb) = 0;

  // Get all models from a given manufacturer.
  // |manufacturer| must be a value returned from a successful
  // ResolveManufacturers() call performed from this PpdProvider
  // instance.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolvePrinters(const std::string& manufacturer,
                               ResolvePrintersCallback cb) = 0;

  // Attempt to find a PpdReference for the given printer.  You should supply
  // as much information in search_data as you can.
  virtual void ResolvePpdReference(const PrinterSearchData& search_data,
                                   ResolvePpdReferenceCallback cb) = 0;

  // Given a PpdReference, attempt to get the PPD for printing.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolvePpd(const Printer::PpdReference& reference,
                          ResolvePpdCallback cb) = 0;

  // Retrieves the name of the PPD license associated with the given printer
  // |effective_make_and_model|. If the name of the retrieved license is empty,
  // then the PPD does not require a license. If |effective_make_and_model| is
  // already present in the cache, then |cb| will fire immediately. Otherwise,
  // the PpdIndex will be fetched in order to retrieve the associated license.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolvePpdLicense(std::string_view effective_make_and_model,
                                 ResolvePpdLicenseCallback cb) = 0;

  // For a given PpdReference, retrieve the make and model strings used to
  // construct that reference.
  //
  // PpdProvider will enqueue calls to this method and answer them in
  // the order received; it will opt to invoke |cb| with failure if the
  // queue grows overlong, failing the oldest calls first. The exact
  // queue length at which this occurs is unspecified.
  virtual void ReverseLookup(const std::string& effective_make_and_model,
                             ReverseLookupCallback cb) = 0;

  // Transform from ppd reference to ppd cache key.  This is exposed for
  // testing, and should not be used by other code.
  static std::string PpdReferenceToCacheKey(
      const Printer::PpdReference& reference);

  // Used to "dereference" the PPD previously named by the cache key from
  // Printer::PpdReference::effective_make_and_model.
  static std::string PpdBasenameToCacheKey(std::string_view ppd_basename);

 protected:
  friend class base::RefCounted<PpdProvider>;
  virtual ~PpdProvider() {}
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_PROVIDER_H_
