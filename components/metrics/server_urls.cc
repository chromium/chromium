// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The actual URLs are stored in an internal version of server_urls.grd
// to prevent Chromium forks from accidentally sending metrics to Google
// servers. The URLs can be found here:
// https://chrome-internal.googlesource.com/chrome/components/metrics/internal/+/main/server_urls.grd
// Further, the reason why the URLs are provided through GRIT is for LGPL
// compliance reasons.

#include "components/metrics/server_urls.h"

#include <string>

#include "base/no_destructor.h"
#include "components/metrics/grit/metrics_server_urls.h"
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

const char kMetricsMimeType[] = "application/vnd.chrome.uma";
const char kUkmMimeType[] = "application/vnd.chrome.ukm";

GURL GetMetricsServerUrl() {
  static const base::NoDestructor<GURL> url(GetUrl(IDS_METRICS_SERVER_URL));
  return *url;
}

GURL GetInsecureMetricsServerUrl() {
  static const base::NoDestructor<GURL> url(
      GetUrl(IDS_INSECURE_METRICS_SERVER_URL));
  return *url;
}

GURL GetCastMetricsServerUrl() {
  static const base::NoDestructor<GURL> url(
      GetUrl(IDS_CAST_METRICS_SERVER_URL));
  return *url;
}

GURL GetUkmServerUrl() {
  static const base::NoDestructor<GURL> url(GetUrl(IDS_UKM_SERVER_URL));
  return *url;
}

GURL GetDwaServerUrl() {
  static const base::NoDestructor<GURL> url(GetUrl(IDS_DWA_SERVER_URL));
  return *url;
}

GURL GetPrivateMetricsServerUrl() {
  static const base::NoDestructor<GURL> url(
      GetUrl(IDS_PRIVATE_METRICS_SERVER_URL));
  return *url;
}

}  // namespace metrics
