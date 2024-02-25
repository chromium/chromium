// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
#define CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/uri.h"
#include "net/base/host_port_pair.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace chromeos {

// Classes of printers tracked.  See doc/cups_printer_management.md for
// details on what these mean.
enum class COMPONENT_EXPORT(CHROMEOS_PRINTING) PrinterClass {
  kEnterprise,
  kAutomatic,
  kDiscovered,
  kSaved
};

COMPONENT_EXPORT(CHROMEOS_PRINTING) std::string ToString(PrinterClass pclass);

// This function checks if the given URI is a valid printer URI. |uri| is
// considered to be a valid printer URI if it has one of the scheme listed in
// the table below and meets the criteria defined there.
//
//  scheme  | userinfo |   host   |   port   |   path   |  query   | fragment
// ---------+----------+----------+----------+----------+----------+----------
//   http   |    NO    | required | optional | optional | optional |    NO
//   https  |    NO    | required | optional | optional | optional |    NO
//   ipp    |    NO    | required | optional | optional | optional |    NO
//   ipps   |    NO    | required | optional | optional | optional |    NO
//   lpd    | optional | required | optional | optional | optional |    NO
//   socket |    NO    | required | optional |    NO    | optional |    NO
//   ippusb |    NO    | required |    NO    | required | optional |    NO
//   usb    |    NO    | required |    NO    | required | optional |    NO
//
// If the given |uri| does not meet the criteria the function returns false and
// set an error message in |error_message| (if it is not nullptr). The message
// has the prefix "Malformed printer URI: ".
bool COMPONENT_EXPORT(CHROMEOS_PRINTING)
    IsValidPrinterUri(const Uri& uri, std::string* error_message = nullptr);

class COMPONENT_EXPORT(CHROMEOS_PRINTING) Printer {
 public:
  // Information needed to find the PPD file for this printer.
  //
  // If you add fields to this struct, you almost certainly will
  // want to update PpdResolver and PpdCache::GetCachePath.
  //
  // Exactly one of the fields below should be filled in.
  //
  // At resolution time, we look for a cached PPD that used the same
  // PpdReference before.
  //
  class PpdReference {
   public:
    // Returns true when this PPD reference is filled in; true whenever any of
    // the members is non-empty.
    bool IsFilled() const;

    // If non-empty, this is the url of a specific PPD the user has specified
    // for use with this printer.  The ppd can be gzipped or uncompressed.
    // Supported schemes for this url are file://, http:// and https://.
    std::string user_supplied_ppd_url;

    // String that identifies which ppd to use from the ppd server.
    // Where possible, this is the same as the ipp/ldap
    // printer-make-and-model field.
    std::string effective_make_and_model;

    // True if the printer should be auto-configured and a PPD is unnecessary.
    bool autoconf = false;
  };

  // The location where the printer is stored.
  enum Source {
    SRC_USER_PREFS,
    SRC_POLICY,
  };

  // An enumeration of printer protocols.
  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum PrinterProtocol {
    kUnknown = 0,
    kUsb = 1,
    kIpp = 2,
    kIpps = 3,
    kHttp = 4,
    kHttps = 5,
    kSocket = 6,
    kLpd = 7,
    kIppUsb = 8,
    kProtocolMax
  };

  // Constructs a printer object that is completely empty.
  Printer();

  // Constructs a printer object with the given |id|.
  explicit Printer(const std::string& id);

  // Copy constructor and assignment.
  Printer(const Printer& printer);
  Printer& operator=(const Printer& printer);

  ~Printer();

  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  const std::string& display_name() const { return display_name_; }
  void set_display_name(const std::string& display_name) {
    display_name_ = display_name;
  }

  const std::string& description() const { return description_; }
  void set_description(const std::string& description) {
    description_ = description;
  }

  const std::string& usb_printer_manufacturer() const {
    return usb_printer_manufacturer_;
  }
  void set_usb_printer_manufacturer(const std::string& manufacturer) {
    usb_printer_manufacturer_ = manufacturer;
  }

  const std::string& make_and_model() const { return make_and_model_; }
  void set_make_and_model(const std::string& make_and_model) {
    make_and_model_ = make_and_model;
  }

  const Uri& uri() const { return uri_; }

  // These methods set |uri| as a new uri. If |uri| is incorrect or does not
  // pass the IsValidPrinterUri(...) function defined above, no changes are made
  // to the object and false is returned. If |error_message| is not nullptr,
  // the error message is written there when the methods return false.
  bool SetUri(const Uri& uri, std::string* error_message = nullptr);
  bool SetUri(const std::string& uri, std::string* error_message = nullptr);

  const PpdReference& ppd_reference() const { return ppd_reference_; }
  PpdReference* mutable_ppd_reference() { return &ppd_reference_; }

  bool supports_ippusb() const { return supports_ippusb_; }
  void set_supports_ippusb(bool supports_ippusb) {
    supports_ippusb_ = supports_ippusb;
  }

  const std::string& print_server_uri() const { return print_server_uri_; }
  void set_print_server_uri(const std::string& print_server_uri) {
    print_server_uri_ = print_server_uri;
  }

  const std::string& uuid() const { return uuid_; }
  void set_uuid(const std::string& uuid) { uuid_ = uuid; }

  // Returns true if the printer should be automatically configured using IPP
  // Everywhere.  Computed using information from |ppd_reference_| and |uri_|.
  bool IsIppEverywhere() const;

  // Returns true if the printer should use driverless autoconfiguration through
  // IPP-USB instead of the USB printer class.
  bool RequiresDriverlessUsb() const;

  // Returns the hostname and port for |uri_|.  Assumes that the uri is
  // well formed.  Returns an empty string if |uri_| is not set.
  net::HostPortPair GetHostAndPort() const;

  // Returns the |uri_| with the host and port replaced with |ip|.  Returns an
  // empty Uri if |uri_| is empty.
  Uri ReplaceHostAndPort(const net::IPEndPoint& ip) const;

  // Returns the printer protocol the printer is configured with.
  Printer::PrinterProtocol GetProtocol() const;

  // Returns true if the current protocol of the printer is one of the following
  // "network protocols": [kIpp, kIpps, kHttp, kHttps, kSocket, kLpd]
  bool HasNetworkProtocol() const;

  // Returns true if the current protocol of the printer is either kUSb or
  // kIppUsb.
  bool IsUsbProtocol() const;

  // Returns true if the current protocol is either local (kUsb or kIppUsb) or
  // secure (kIpps or kHttps).
  bool HasSecureProtocol() const;

  // Returns true if the host component of the printer's URI ends with
  // ".local"
  //
  // This method is meaningless to call without a URI set.
  bool IsZeroconf() const;

  // Returns true if the printer uri is set and false when the uri is empty.
  bool HasUri() const { return !uri_.GetScheme().empty(); }

  Source source() const { return source_; }
  void set_source(const Source source) { source_ = source; }

  const CupsPrinterStatus& printer_status() const { return printer_status_; }
  void set_printer_status(const chromeos::CupsPrinterStatus& printer_status) {
    printer_status_ = printer_status;
  }

  // Setter and getter for flag marking that the printer is used in the finch
  // experiment created for b:184293121.
  bool AffectedByIppUsbMigration() const {
    return experimental_setup_of_usb_printer_with_ipp_and_ppd_;
  }
  void SetAffectedByIppUsbMigration(bool flag) {
    experimental_setup_of_usb_printer_with_ipp_and_ppd_ = flag;
  }

 private:
  // Globally unique identifier. Empty indicates a new printer.
  std::string id_;

  // User defined string for printer identification.
  std::string display_name_;

  // User defined string for additional printer information.
  std::string description_;

  // Holds manufacturer name read from USB printer.
  std::string usb_printer_manufacturer_;

  // The manufactuer and model of the printer in one string. e.g. HP OfficeJet
  // 415. This is either read from or derived from printer information and is
  // not necessarily suitable for display.
  std::string make_and_model_;

  // The full path for the printer. Suitable for configuration in CUPS.
  // Contains protocol, hostname, port, and queue.
  Uri uri_;

  // How to find the associated postscript printer description.
  PpdReference ppd_reference_;

  // Represents whether or not the printer supports printing using ipp-over-usb.
  bool supports_ippusb_ = false;

  // When non-empty, the uri of the print server the printer was added from. We
  // use this to determine if a printer is a print server printer.
  std::string print_server_uri_;

  // The UUID from an autoconf protocol for deduplication. Could be empty.
  std::string uuid_;

  // The datastore which holds this printer.
  Source source_;

  // The current status of the printer
  chromeos::CupsPrinterStatus printer_status_;

  // This flag is set for printers that take part in the finch experiment
  // created for b/184293121.
  bool experimental_setup_of_usb_printer_with_ipp_and_ppd_ = false;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
