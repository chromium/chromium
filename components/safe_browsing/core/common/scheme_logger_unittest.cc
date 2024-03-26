// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/scheme_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(SchemeLoggerTest, TestLogScheme) {
  using scheme_logger::UrlScheme;
  struct TestCase {
    std::string url;
    UrlScheme expected_scheme;
  } test_cases[] = {
      {"about:blank", UrlScheme::kAbout},
      {"BLOB://foo/", UrlScheme::kBlob},
      {"content://test/content/", UrlScheme::kContent},
      {"cid://test/cid/", UrlScheme::kCid},
      {"data://test/data/", UrlScheme::kData},
      {"file://test/file/", UrlScheme::kFile},
      {"filesystem://test/filesystem/", UrlScheme::kFileSystem},
      {"ftp://test/ftp/", UrlScheme::kFtp},
      {"http://www.google.com/", UrlScheme::kHttp},
      {"https://www.google.com/", UrlScheme::kHttps},
      {"javascript:undefined", UrlScheme::kJavascript},
      {"mailto:nobody", UrlScheme::kMailTo},
      {"tel:123456789", UrlScheme::kTel},
      {"urn:foo", UrlScheme::kUrn},
      {"uuid-in-package:foo", UrlScheme::kUuidInPackage},
      {"webcal:foo", UrlScheme::kWebcal},
      {"ws://foo/", UrlScheme::kWs},
      {"wss://foo/", UrlScheme::kWss},
      {"isolated-app:foo", UrlScheme::kIsolatedApp},
      {"chrome-native:foo", UrlScheme::kChromeNative},
      {"chrome-search:foo", UrlScheme::kChromeSearch},
      {"devtools://devtools", UrlScheme::kDevTools},
      {"chrome-error://", UrlScheme::kChromeError},
      {"chrome://settings", UrlScheme::kChrome},
      {"chrome-untrusted://", UrlScheme::kChromeUntrusted},
      {"view-source:foo/", UrlScheme::kViewSource},
      {"externalfile:foo/", UrlScheme::kExternalFile},
      {"android-app:foo/", UrlScheme::kAndroidApp},
      {"googlechrome:foo/", UrlScheme::kGoogleChrome},
      {"android-webview-video-poster:foo/",
       UrlScheme::kAndroidWebviewVideoPoster},
      {"chrome-distiller:foo/", UrlScheme::kChromeDistiller},
      {"chrome-extension://foo", UrlScheme::kChromeExtension},
      {"something-else:foo", UrlScheme::kUnknown},
      {"something-else://foo", UrlScheme::kUnknown},
  };
  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    std::string histogram_name = "DummyHistogram";
    scheme_logger::LogScheme(GURL(test_case.url), histogram_name);
    histogram_tester.ExpectUniqueSample(/*name=*/histogram_name,
                                        /*sample=*/test_case.expected_scheme,
                                        /*expected_bucket_count=*/1);
  }
}

}  // namespace safe_browsing
