// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The actual URLs are stored in an internal version of url_constants.grd
// to prevent Chromium forks from accidentally sending metrics to Google
// servers. The URLs can be found here:
// https://chrome-internal.googlesource.com/chrome/components/metrics/internal/+/main/url_constants.grd
// Further, the reason why the URLs are provided through GRIT is for LGPL
// compliance reasons.

#include "components/metrics/server_urls.h"

#include <string>

#include "components/metrics/grit/metrics_url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace metrics {

namespace {

GURL GetUrl(int id) {
  // GRIT messages cannot be empty, so dashes are used as placerholders to
  // represent empty strings.
  std::string url_string = l10n_util::GetStringUTF8(id);
  if (url_string == "-") {
    return GURL();
  }
  return GURL(std::move(url_string));
}

}  // namespace

// TODO(crbug.com/358224254): Remove mime types from GRIT.
const char kMetricsMimeType[] = "application/vnd.chrome.uma";
const char kUkmMimeType[] = "application/vnd.chrome.ukm";

GURL GetMetricsServerUrl() {
  // TODO(crbug.com/358224254): Remove "NEW" from the GRIT name.
  static const GURL url = GetUrl(IDS_NEW_METRICS_SERVER_URL);
  return url;
}

GURL GetInsecureMetricsServerUrl() {
  // TODO(crbug.com/358224254): Remove "NEW" from the GRIT name.
  static const GURL url = GetUrl(IDS_NEW_METRICS_SERVER_URL_INSECURE);
  return url;
}

GURL GetCastMetricsServerUrl() {
  // TODO(crbug.com/358224254): Change "OLD" to "CAST" in the GRIT name.
  static const GURL url = GetUrl(IDS_OLD_METRICS_SERVER_URL);
  return url;
}

GURL GetUkmServerUrl() {
  // TODO(crbug.com/358224254): Remove "DEFAULT" from the GRIT name.
  static const GURL url = GetUrl(IDS_DEFAULT_UKM_SERVER_URL);
  return url;
}

GURL GetDwaServerUrl() {
  // TODO(crbug.com/358224254): Remove "DEFAULT" from the GRIT name.
  static const GURL url = GetUrl(IDS_DEFAULT_DWA_SERVER_URL);
  return url;
}

}  // namespace metrics
