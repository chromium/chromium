// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
#define CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_

#include <string>

#include "base/optional.h"
#include "chromeos/chromeos_export.h"
#include "net/base/host_port_pair.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace chromeos {

class UriComponents;

// Parses |printer_uri| into its components and returns an optional
// UriComponents depending on whether or not |printer_uri| was parsed
// successfully.
CHROMEOS_EXPORT base::Optional<UriComponents> ParseUri(
    const std::string& printer_uri);

// Classes of printers tracked.  See doc/cups_printer_management.md for
// details on what these mean.
enum class CHROMEOS_EXPORT PrinterClass {
  kEnterprise,
  kAutomatic,
  kDiscovered,
  kSaved
};

class CHROMEOS_EXPORT Printer {
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
  struct PpdReference {
    // If non-empty, this is the url of a specific PPD the user has specified
    // for use with this printer.  The ppd can be gzipped or uncompressed.  This
    // url must use a file:// scheme.
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

  // Returns the |manufacturer| of the printer.
  // DEPRECATED(skau@chromium.org): Use make_and_model() instead.
  const std::string& manufacturer() const { return manufacturer_; }
  void set_manufacturer(const std::string& manufacturer) {
    manufacturer_ = manufacturer;
  }

  // Returns the |model| of the printer.
  // DEPRECATED(skau@chromium.org): Use make_and_model() instead.
  const std::string& model() const { return model_; }
  void set_model(const std::string& model) { model_ = model; }

  const std::string& make_and_model() const { return make_and_model_; }
  void set_make_and_model(const std::string& make_and_model) {
    make_and_model_ = make_and_model;
  }

  const std::string& uri() const { return uri_; }
  void set_uri(const std::string& uri) { uri_ = uri; }

  const PpdReference& ppd_reference() const { return ppd_reference_; }
  PpdReference* mutable_ppd_reference() { return &ppd_reference_; }

  bool supports_ippusb() const { return supports_ippusb_; }
  void set_supports_ippusb(bool supports_ippusb) {
    supports_ippusb_ = supports_ippusb;
  }

  const std::string& uuid() const { return uuid_; }
  void set_uuid(const std::string& uuid) { uuid_ = uuid; }

  // Returns true if the printer should be automatically configured using
  // IPP Everywhere.  Computed using information from |ppd_reference_| and
  // |uri_|.
  bool IsIppEverywhere() const;

  // Returns the hostname and port for |uri_|.  Assumes that the uri is
  // well formed.  Returns an empty string if |uri_| is not set.
  net::HostPortPair GetHostAndPort() const;

  // Returns the |uri_| with the host and port replaced with |ip|.  Returns an
  // empty string if |uri_| is empty.
  std::string ReplaceHostAndPort(const net::IPEndPoint& ip) const;

  // Returns the printer protocol the printer is configured with.
  Printer::PrinterProtocol GetProtocol() const;

  // Returns true if the current protocol of the printer is one of the following
  // "network protocols":
  //   [kIpp, kIpps, kHttp, kHttps, kSocket, kLpd]
  bool HasNetworkProtocol() const;

  // Returns true if the current protocol of the printer is either kUSb or
  // kIppUsb.
  bool IsUsbProtocol() const;

  Source source() const { return source_; }
  void set_source(const Source source) { source_ = source; }

  // Parses the printers's uri into its components and returns an optional
  // containing a UriComponents object depending on whether or not the uri was
  // successfully parsed.
  base::Optional<UriComponents> GetUriComponents() const;

 private:
  // Globally unique identifier. Empty indicates a new printer.
  std::string id_;

  // User defined string for printer identification.
  std::string display_name_;

  // User defined string for additional printer information.
  std::string description_;

  // The manufacturer of the printer, e.g. HP
  // DEPRECATED(skau@chromium.org): Migrating to make_and_model.  This is kept
  // for backward compatibility until migration is complete.
  std::string manufacturer_;

  // The model of the printer, e.g. OfficeJet 415
  // DEPRECATED(skau@chromium.org): Migrating to make_and_model.  This is kept
  // for backward compatibility until migration is complete.
  std::string model_;

  // The manufactuer and model of the printer in one string. e.g. HP OfficeJet
  // 415. This is either read from or derived from printer information and is
  // not necessarily suitable for display.
  std::string make_and_model_;

  // The full path for the printer. Suitable for configuration in CUPS.
  // Contains protocol, hostname, port, and queue.
  std::string uri_;

  // When non-empty, the uri to use with cups instead of uri_.  This field
  // is ephemeral, and not saved to sync service.  This allows us to do
  // on the fly rewrites of uris to work around limitations in the OS such
  // as CUPS not being able to directly resolve mDNS addresses, see crbug/626377
  // for details.
  std::string effective_uri_;

  // How to find the associated postscript printer description.
  PpdReference ppd_reference_;

  // Represents whether or not the printer supports printing using ipp-over-usb.
  bool supports_ippusb_ = false;

  // The UUID from an autoconf protocol for deduplication. Could be empty.
  std::string uuid_;

  // The datastore which holds this printer.
  Source source_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_CONFIGURATION_H_
