// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/scheme_logger.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "url/url_constants.h"

namespace safe_browsing::scheme_logger {

void LogScheme(const GURL& url, const std::string& enum_histogram_name) {
  static constexpr auto kSchemeOptions =
      base::MakeFixedFlatMap<std::string_view, UrlScheme>({
          // From url/url_constants.cc
          {url::kAboutScheme, UrlScheme::kAbout},
          {url::kBlobScheme, UrlScheme::kBlob},
          {url::kContentScheme, UrlScheme::kContent},
          {url::kContentIDScheme, UrlScheme::kCid},
          {url::kDataScheme, UrlScheme::kData},
          {url::kFileScheme, UrlScheme::kFile},
          {url::kFileSystemScheme, UrlScheme::kFileSystem},
          {url::kFtpScheme, UrlScheme::kFtp},
          {url::kHttpScheme, UrlScheme::kHttp},
          {url::kHttpsScheme, UrlScheme::kHttps},
          {url::kJavaScriptScheme, UrlScheme::kJavascript},
          {url::kMailToScheme, UrlScheme::kMailTo},
          {url::kTelScheme, UrlScheme::kTel},
          {url::kUrnScheme, UrlScheme::kUrn},
          {url::kUuidInPackageScheme, UrlScheme::kUuidInPackage},
          {url::kWebcalScheme, UrlScheme::kWebcal},
          {url::kWsScheme, UrlScheme::kWs},
          {url::kWssScheme, UrlScheme::kWss},
          // From chrome/common/url_constants.cc
          {"isolated-app", UrlScheme::kIsolatedApp},
          {"chrome-native", UrlScheme::kChromeNative},
          {"chrome-search", UrlScheme::kChromeSearch},
          // From content/public/common/url_constants.cc
          {"devtools", UrlScheme::kDevTools},
          {"chrome-error", UrlScheme::kChromeError},
          {"chrome", UrlScheme::kChrome},
          {"chrome-untrusted", UrlScheme::kChromeUntrusted},
          {"view-source", UrlScheme::kViewSource},
          {"externalfile", UrlScheme::kExternalFile},
          {"android-app", UrlScheme::kAndroidApp},
          {"googlechrome", UrlScheme::kGoogleChrome},
          // From android_webview/common/url_constants.cc
          {"android-webview-video-poster",
           UrlScheme::kAndroidWebviewVideoPoster},
          // From components/dom_distiller/core/url_constants.cc
          {"chrome-distiller", UrlScheme::kChromeDistiller},
          // From extensions/common/constants.cc
          {"chrome-extension", UrlScheme::kChromeExtension},
      });

  UrlScheme scheme = UrlScheme::kUnknown;
  if (auto it = kSchemeOptions.find(url.scheme()); it != kSchemeOptions.end()) {
    scheme = it->second;
  }
  base::UmaHistogramEnumeration(enum_histogram_name, scheme);
}

}  // namespace safe_browsing::scheme_logger
