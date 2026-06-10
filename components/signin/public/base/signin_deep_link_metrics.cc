// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_deep_link_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace signin_metrics {

namespace {

std::string_view GetSuffix(signin::ExternalEntryPoint entry_point) {
  switch (entry_point) {
    case signin::ExternalEntryPoint::kDesktopDefault:
      return ".DesktopDefault";
    case signin::ExternalEntryPoint::kUnknown:
      return ".Unknown";
  }
  NOTREACHED();
}

}  // namespace

void RecordUrlDetected(int entry_point_id) {
  base::UmaHistogramSparse("Signin.CrossDevice.UrlDetected", entry_point_id);
}

void RecordInitialAccountsNumber(signin::ExternalEntryPoint entry_point,
                                 int count) {
  const std::string_view metric = "Signin.CrossDevice.InitialAccountsNumber";
  base::UmaHistogramCounts100(metric, count);
  base::UmaHistogramCounts100(base::StrCat({metric, GetSuffix(entry_point)}),
                              count);
}

}  // namespace signin_metrics
