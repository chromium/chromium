// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/scheme_logger.h"

#include <map>
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"

namespace {

using UrlScheme = safe_browsing::scheme_logger::UrlScheme;

const std::map<std::string, UrlScheme>& GetSchemeOptions() {
  static const base::NoDestructor<std::map<std::string, UrlScheme>>
      scheme_options({
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
  return *scheme_options;
}

}  // namespace

namespace safe_browsing::scheme_logger {

void LogScheme(const GURL& url, const std::string& enum_histogram_name) {
  UrlScheme scheme = UrlScheme::kUnknown;
  for (const auto& scheme_option : GetSchemeOptions()) {
    if (url.SchemeIs(scheme_option.first)) {
      scheme = scheme_option.second;
      break;
    }
  }
  base::UmaHistogramEnumeration(enum_histogram_name, scheme);
}

}  // namespace safe_browsing::scheme_logger
