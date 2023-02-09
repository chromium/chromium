// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_METADATA_MANAGER_H_
#define CHROMEOS_PRINTING_PPD_METADATA_MANAGER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chromeos/printing/ppd_metadata_parser.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_config_cache.h"

namespace chromeos {

enum class PpdIndexChannel { kProduction, kStaging, kDev, kLocalhost };

// A PpdMetadataManager is the class responsible for fetching and
// parsing PPD metadata to answer high-level queries about metadata.
//
// This class must be called from a sequenced context.
class COMPONENT_EXPORT(CHROMEOS_PRINTING) PpdMetadataManager {
 public:
  // Used by GetLocale().
  // Argument denotes success of setting metadata locale for |this|.
  using GetLocaleCallback = base::OnceCallback<void(bool)>;

  // Used by GetPrinters().
  // Arguments denote
  // *  whether the call succeeded and
  // *  upon success, ParsedPrinters as requested.
  using GetPrintersCallback =
      base::OnceCallback<void(bool, const ParsedPrinters&)>;

  // Used by FindAllEmmsAvailableInIndex().
  // Contains a map
  // *  whose keys are effective-make-and-model strings (provided by the
  //    caller) that are available in forward index metadata and
  // *  whose values contain the corresponding information read from the
  //    forward index metadata.
  using FindAllEmmsAvailableInIndexCallback = base::OnceCallback<void(
      const base::flat_map<std::string, ParsedIndexValues>&)>;

  // Used by FindDeviceInUsbIndex().
  // *  Contains the effective-make-and-model string corresponding to
  //    the vendor id / product id pair originally provided by caller.
  // *  The argument is empty if no appropriate effective-make-and-model
  //    string was found.
  using FindDeviceInUsbIndexCallback =
      base::OnceCallback<void(const std::string&)>;

  // Used by GetUsbManufacturerName().
  // *  Contains the unlocalized manufacturer name corresponding to the
  //    vendor id originally provided by caller.
  // *  The argument is empty if the manufacturer name is not found.
  using GetUsbManufacturerNameCallback =
      base::OnceCallback<void(const std::string&)>;

  // Assumes ownership of |config_cache|.
  static std::unique_ptr<PpdMetadataManager> Create(
      base::StringPiece browser_locale,
      PpdIndexChannel channel,
      base::Clock* clock,
      std::unique_ptr<PrinterConfigCache> config_cache);

  virtual ~PpdMetadataManager() = default;

  // Primes |this| with the best-fit locale advertised by the Chrome OS
  // Printing serving root. The best-fit locale is the one closest to
  // the |browser_locale| passed to Create(). "Closest" is an
  // implementation-defined concept.
  //
  // If a best-fit locale is already set in |this|, |this| invokes |cb|
  // immediately and indicates success.
  //
  // With few exceptions, caller must not call any other method of
  // |this| until the |cb| indicates success. Exceptional methods are
  // documented in this header file.
  //
  // See also: SetLocaleForTesting()
  virtual void GetLocale(GetLocaleCallback cb) = 0;

  // Calls |cb| with a list of manufacturers.
  // *  On success, the list is created from metadata no older than
  //    |age|.
  // *  On failure, the first argument to |cb| is set accordingly.
  virtual void GetManufacturers(
      base::TimeDelta age,
      PpdProvider::ResolveManufacturersCallback cb) = 0;

  // Calls |cb| with a map of printers made by |manufacturer|.
  // *  Caller must have previously successfully called
  //    GetManufacturers().
  // *  On success, the map is created from metadata no older than
  //    |age|.
  // *  On failure, the first argument to |cb| is set accordingly.
  virtual void GetPrinters(base::StringPiece manufacturer,
                           base::TimeDelta age,
                           GetPrintersCallback cb) = 0;

  // Calls |cb| with the subset of strings from |emms| that are
  // available in forward index metadata mapped to the corresponding
  // values read from forward index metadata.
  // *  Does not rely on prior call to GetLocale().
  // *  During operation, operates with metadata no older than |age|.
  // *  On failure, calls |cb| with an empty map.
  virtual void FindAllEmmsAvailableInIndex(
      const std::vector<std::string>& emms,
      base::TimeDelta age,
      FindAllEmmsAvailableInIndexCallback cb) = 0;

  // Searches USB index metadata for a printer with the given
  // |vendor_id| and |product_id|, calling |cb| with the appropriate
  // effective-make-and-model string if one is found.
  // *  Does not rely on prior call to GetLocale().
  // *  During operation, operates with metadata no older than |age|.
  // *  On failure, calls |cb| with an empty string.
  virtual void FindDeviceInUsbIndex(int vendor_id,
                                    int product_id,
                                    base::TimeDelta age,
                                    FindDeviceInUsbIndexCallback cb) = 0;

  // Searches the USB vendor ID map for a manufacturer with the given
  // |vendor_id|, calling |cb| with the name found (if any).
  // *  Does not rely on prior call to GetLocale().
  // *  During operation, operates with metadata no older than |age|.
  // *  On failure, calls |cb| with an empty string.
  virtual void GetUsbManufacturerName(int vendor_id,
                                      base::TimeDelta age,
                                      GetUsbManufacturerNameCallback cb) = 0;

  // Calls |cb| with the make and model of
  // |effective_make_and_model|.
  // *  On success, the split is performed against metadata no older than
  //    |age|.
  // *  On failure, the first argument to |cb| is set accordingly.
  //
  // The split is defined by the reverse index metadata that this method
  // fetches, appropriate to the |effective_make_and_model|.
  // Googlers: you may consult
  // go/cros-printing:ppd-metadata#reverse-index
  virtual void SplitMakeAndModel(base::StringPiece effective_make_and_model,
                                 base::TimeDelta age,
                                 PpdProvider::ReverseLookupCallback cb) = 0;

  // Returns a borrowed pointer to the PrinterConfigCache passed to
  // Create(); |this| retains ownership.
  virtual PrinterConfigCache* GetPrinterConfigCacheForTesting() const = 0;

  // Fakes a successful call to GetLocale(), setting the internal
  // locale of |this| to |locale|.
  //
  // This method is useful for bypassing a real call to GetLocale(),
  // which consumers of this class ordinarily must complete successfully
  // before calling any other method of |this|.
  virtual void SetLocaleForTesting(base::StringPiece locale) = 0;

  // Fakes a successful call to GetManufacturers(), providing |this|
  // with a list of manufacturers.
  //
  // This method returns true if |this| successfully parses and stores
  // off the list of |manufacturers_json|. Caller must verify that this
  // method returns true.
  virtual bool SetManufacturersForTesting(
      base::StringPiece manufacturers_json) = 0;

  // Returns the metadata locale currently set in |this|.
  virtual base::StringPiece ExposeMetadataLocaleForTesting() const = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_METADATA_MANAGER_H_
