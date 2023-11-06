// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/cups_printer_status.h"

#include "base/values.h"

#include <stddef.h>

namespace chromeos {

using CupsPrinterStatusReason = CupsPrinterStatus::CupsPrinterStatusReason;

CupsPrinterStatusReason::CupsPrinterStatusReason(const Reason& reason,
                                                 const Severity& severity)
    : reason_(reason), severity_(severity) {}

CupsPrinterStatusReason::~CupsPrinterStatusReason() = default;

const CupsPrinterStatusReason::Reason& CupsPrinterStatusReason::GetReason()
    const {
  return reason_;
}

const CupsPrinterStatusReason::Severity& CupsPrinterStatusReason::GetSeverity()
    const {
  return severity_;
}

CupsPrinterStatus::CupsPrinterStatus(const std::string& printer_id)
    : printer_id_(printer_id), timestamp_(base::Time::Now()) {}
CupsPrinterStatus::CupsPrinterStatus() = default;
CupsPrinterStatus::CupsPrinterStatus(const CupsPrinterStatus& other) = default;
CupsPrinterStatus& CupsPrinterStatus::operator=(
    const CupsPrinterStatus& other) = default;

CupsPrinterStatus::~CupsPrinterStatus() = default;

const std::string& CupsPrinterStatus::GetPrinterId() const {
  return printer_id_;
}

const base::flat_set<CupsPrinterStatusReason>&
CupsPrinterStatus::GetStatusReasons() const {
  return status_reasons_;
}

const base::Time& CupsPrinterStatus::GetTimestamp() const {
  return timestamp_;
}

void CupsPrinterStatus::AddStatusReason(
    const CupsPrinterStatusReason::Reason& reason,
    const CupsPrinterStatusReason::Severity& severity) {
  status_reasons_.emplace(CupsPrinterStatusReason(reason, severity));
}

void CupsPrinterStatus::SetAuthenticationInfo(
    const PrinterAuthenticationInfo& auth_info) {
  auth_info_ = auth_info;
}

base::Value::Dict CupsPrinterStatus::ConvertToValue() const {
  base::Value::Dict dict;
  dict.Set("printerId", printer_id_);
  dict.Set("timestamp", timestamp_.InMillisecondsFSinceUnixEpochIgnoringNull());
  base::Value::List status_reasons;
  for (const CupsPrinterStatusReason& reason : status_reasons_) {
    base::Value::Dict status_reason;
    status_reason.Set("reason", static_cast<int>(reason.GetReason()));
    status_reason.Set("severity", static_cast<int>(reason.GetSeverity()));
    status_reasons.Append(std::move(status_reason));
  }
  dict.Set("statusReasons", std::move(status_reasons));
  return dict;
}

}  // namespace chromeos
