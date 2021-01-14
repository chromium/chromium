// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/scanning/scanning_metrics_handler.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "chromeos/components/scanning/scanning_uma.h"

namespace chromeos {

namespace {

namespace mojo_ipc = scanning::mojom;

// Scan job settings constants.
constexpr char kSourceType[] = "sourceType";
constexpr char kFileType[] = "fileType";
constexpr char kColorMode[] = "colorMode";
constexpr char kPageSize[] = "pageSize";
constexpr char kResolution[] = "resolution";

}  // namespace

ScanningMetricsHandler::ScanningMetricsHandler() = default;

ScanningMetricsHandler::~ScanningMetricsHandler() = default;

void ScanningMetricsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "recordScanJobSettings",
      base::BindRepeating(&ScanningMetricsHandler::HandleRecordScanJobSettings,
                          base::Unretained(this)));
}

void ScanningMetricsHandler::HandleRecordScanJobSettings(
    const base::ListValue* args) {
  AllowJavascript();

  const base::DictionaryValue* scan_job_settings = nullptr;
  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetDictionary(0, &scan_job_settings));

  base::UmaHistogramEnumeration(
      "Scanning.ScanJobSettings.Source",
      static_cast<mojo_ipc::SourceType>(
          scan_job_settings->FindIntPath(kSourceType).value()));
  base::UmaHistogramEnumeration(
      "Scanning.ScanJobSettings.FileType",
      static_cast<mojo_ipc::FileType>(
          scan_job_settings->FindIntPath(kFileType).value()));
  base::UmaHistogramEnumeration(
      "Scanning.ScanJobSettings.ColorMode",
      static_cast<mojo_ipc::ColorMode>(
          scan_job_settings->FindIntPath(kColorMode).value()));
  base::UmaHistogramEnumeration(
      "Scanning.ScanJobSettings.PageSize",
      static_cast<mojo_ipc::PageSize>(
          scan_job_settings->FindIntPath(kPageSize).value()));
  const scanning::ScanJobSettingsResolution resolution =
      scanning::GetResolutionEnumValue(
          scan_job_settings->FindIntPath(kResolution).value());
  if (resolution != scanning::ScanJobSettingsResolution::kUnexpectedDpi) {
    base::UmaHistogramEnumeration("Scanning.ScanJobSettings.Resolution",
                                  resolution);
  }
}

}  // namespace chromeos
