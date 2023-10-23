// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_CUPS_PRINTER_STATUS_H_
#define CHROMEOS_PRINTING_CUPS_PRINTER_STATUS_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"

namespace chromeos {

// Holds information about authentication required by a printer.
struct PrinterAuthenticationInfo {
  // URI of OAuth2 Authorization Server and scope. Empty strings if not set.
  std::string oauth_server;
  std::string oauth_scope;

  bool operator==(const PrinterAuthenticationInfo& other) const {
    return oauth_server == other.oauth_server &&
           oauth_scope == other.oauth_scope;
  }
};

// A container for the results of a printer status query. A printer status query
// can return multiple error reasons so CupsPrinterStatus contains multiple
// CupsPrinterStatusReasons. |timestamp| is set at the time of CupsPrinterStatus
// creation.
class COMPONENT_EXPORT(CHROMEOS_PRINTING) CupsPrinterStatus {
 public:
  // A combination of a reason, which describes the state of a printer, and a
  // severity, which is the level of seriousness of that state.
  class COMPONENT_EXPORT(CHROMEOS_PRINTING) CupsPrinterStatusReason {
   public:
    using Reason = crosapi::mojom::StatusReason::Reason;
    using Severity = crosapi::mojom::StatusReason::Severity;

    CupsPrinterStatusReason(const Reason& reason, const Severity& severity);
    ~CupsPrinterStatusReason();

    const Reason& GetReason() const;
    const Severity& GetSeverity() const;

    bool operator==(const CupsPrinterStatusReason& other) const {
      return reason_ == other.reason_ && severity_ == other.severity_;
    }

    // Comparison operator to support storing CupsPrinterStatusReason in a
    // flat_set. Two CupsPrinterStatusReasons are considered matching iff
    // |reason| and |severity| are the same.
    bool operator<(const CupsPrinterStatusReason& other) const {
      return reason_ < other.reason_ ||
             (reason_ == other.reason_ && severity_ < other.severity_);
    }

   private:
    Reason reason_;
    Severity severity_;
  };

  // Creates a CupsPrinterStatus from an existing printing::PrinterStatus.
  explicit CupsPrinterStatus(const std::string& printer_id);

  CupsPrinterStatus();
  CupsPrinterStatus(const CupsPrinterStatus& other);
  CupsPrinterStatus& operator=(const CupsPrinterStatus& other);

  ~CupsPrinterStatus();

  bool operator==(const CupsPrinterStatus& other) const {
    return status_reasons_ == other.status_reasons_ &&
           auth_info_ == other.auth_info_;
  }

  const std::string& GetPrinterId() const;

  // Returns set of status reasons. Each reason describing status of the
  // printer.
  const base::flat_set<CupsPrinterStatusReason>& GetStatusReasons() const;

  const PrinterAuthenticationInfo& GetAuthenticationInfo() const {
    return auth_info_;
  }

  const base::Time& GetTimestamp() const;

  // Adds a new CupsPrinterStatusReason to an existing CupsPrinterStatus.
  void AddStatusReason(const CupsPrinterStatusReason::Reason& reason,
                       const CupsPrinterStatusReason::Severity& severity);

  void SetAuthenticationInfo(const PrinterAuthenticationInfo& auth_info);

  base::Value::Dict ConvertToValue() const;

 private:
  std::string printer_id_;
  base::flat_set<CupsPrinterStatusReason> status_reasons_;
  PrinterAuthenticationInfo auth_info_;
  base::Time timestamp_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_CUPS_PRINTER_STATUS_H_
