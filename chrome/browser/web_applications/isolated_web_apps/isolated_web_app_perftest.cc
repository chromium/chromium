// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry_factory.h"
#include "components/webapps/isolated_web_apps/reading/signed_web_bundle_reader.h"
#include "components/webapps/isolated_web_apps/test_support/signed_web_bundle_utils.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

// Performance benchmarks for Isolated Web App installation and page load, and
// for the `SignedWebBundleReader` operations underneath them. All of them run
// in a full browser, so bundle parsing happens wherever production parses.
//
// Each benchmark runs in one of two modes:
//   * By default, and therefore on the CQ, it performs a single lap and
//     reports nothing. Timings taken on a shared bot are meaningless, but
//     running the benchmarks keeps them from rotting.
//   * With `--full-performance-run` it performs the lap counts below and
//     emits `*RESULT` lines for `testing/scripts/run_performance_tests.py`.
//     Only these numbers are worth reading, and only on an idle machine:
//
//       out/Default/browser_tests --full-performance-run \
//           --test-launcher-jobs=1 \
//           --gtest_filter=IsolatedWebAppPerfTest.*
namespace web_app {
namespace {

// The convention shared with the other `browser_tests` benchmarks that
// Pinpoint drives through the `performance_browser_tests` isolate.
constexpr char kFullPerformanceRunSwitch[] = "full-performance-run";

constexpr char kMetricPrefixIWA[] = "IsolatedWebApp.";
constexpr char kMetricPrefixReader[] = "SignedWebBundleReader.";
constexpr char kMetricOperationTime[] = "operation_time";
constexpr char kMetricThroughputOps[] = "throughput";

constexpr int kNumBundleEntries = 250;
constexpr int kNumSubresources = 50;
constexpr size_t kSubresourcePayloadSize = 4096;

// Lap counts of a full performance run. A default run does a single measured
// lap and no warmup.
constexpr int kInstallWarmupLaps = 1;
constexpr int kInstallMeasuredLaps = 5;

constexpr int kColdLoadWarmupLaps = 2;
constexpr int kColdLoadMeasuredLaps = 10;

constexpr int kWarmLoadWarmupLaps = 5;
constexpr int kWarmLoadMeasuredLaps = 20;

constexpr int kCreateReaderWarmupLaps = 3;
constexpr int kCreateReaderMeasuredLaps = 50;

// The read benchmarks cycle through the first `kNumSubresources` subresources,
// so a warmup of exactly one full cycle guarantees that every measured lap
// reads a URL that has already been read once.
constexpr int kReadWarmupLaps = kNumSubresources;
constexpr int kReadMeasuredLaps = 500;

constexpr char kIndexHtml[] = R"(<!DOCTYPE html>
<html>
<head><title>Loading...</title></head>
<body><script src="/loader.js"></script></body>
</html>
)";

// `absl::StrFormat()` template taking the number of subresources to fetch.
// Once they are all fetched, sets the title to "LOADED" followed by the page's
// query string.
constexpr char kLoaderJsFormat[] = R"js(const total = %d;
const search = window.location.search;
const promises = [];
for (let i = 0; i < total; ++i) {
  promises.push(
    fetch('/subresource_' + i + '.js' + search, { cache: 'no-store' })
      .then(r => {
        if (!r.ok) {
          throw new Error('HTTP ' + r.status + ' for ' + r.url);
        }
        return r.text();
      })
  );
}
Promise.all(promises).then(() => {
  document.title = 'LOADED' + search;
});
)js";

// The body of every `/subresource_<i>.js` entry: a JS comment wrapping
// `kSubresourcePayloadSize` bytes.
std::string SubresourcePayload() {
  return base::StrCat(
      {"/* ", std::string(kSubresourcePayloadSize, 'X'), " */\n"});
}

std::unique_ptr<ScopedBundledIsolatedWebApp> BuildSubresourceHeavyBundle() {
  IsolatedWebAppBuilder builder(
      ManifestBuilder().SetName("IWA Perf Benchmark App").SetVersion("1.0.0"));

  const IsolatedWebAppBuilder::Headers no_cache_html_headers = {
      {"content-type", "text/html; charset=utf-8"},
      {"cache-control", "no-store, no-cache, must-revalidate"}};
  const IsolatedWebAppBuilder::Headers no_cache_js_headers = {
      {"content-type", "application/javascript; charset=utf-8"},
      {"cache-control", "no-store, no-cache, must-revalidate"}};

  builder.AddResource("/index.html", kIndexHtml, no_cache_html_headers);

  builder.AddResource("/loader.js",
                      absl::StrFormat(kLoaderJsFormat, kNumSubresources),
                      no_cache_js_headers);

  const std::string js_payload = SubresourcePayload();
  for (int i = 0; i < kNumBundleEntries; ++i) {
    builder.AddResource(
        base::StrCat({"/subresource_", base::NumberToString(i), ".js"}),
        js_payload, no_cache_js_headers);
  }

  return builder.BuildBundle();
}

base::FilePath GetInstalledBundlePath(Profile* profile,
                                      const webapps::AppId& app_id) {
  const WebApp* app =
      WebAppProvider::GetForTest(profile)->registrar_unsafe().GetAppById(
          app_id);
  CHECK(app);
  CHECK(app->isolation_data().has_value());
  const auto& location = app->isolation_data()->location().variant();
  if (const auto* owned = std::get_if<IwaStorageOwnedBundle>(&location)) {
    return owned->GetPath(profile->GetPath());
  }
  if (const auto* unowned = std::get_if<IwaStorageUnownedBundle>(&location)) {
    return unowned->path();
  }
  NOTREACHED();
}

void ClearReaderCache(Profile* profile, const base::FilePath& bundle_path) {
  base::test::TestFuture<void> future;
  IsolatedWebAppReaderRegistryFactory::Get(profile)->ClearCacheForPath(
      bundle_path, future.GetCallback());
  CHECK(future.Wait());
}

std::unique_ptr<SignedWebBundleReader> CreateReaderAndWait(
    const base::FilePath& bundle_path,
    const GURL& base_url,
    bool verify_signatures) {
  base::test::TestFuture<SignedWebBundleReader::Result> future;
  SignedWebBundleReader::Create(bundle_path, base_url, verify_signatures,
                                future.GetCallback());
  SignedWebBundleReader::Result result = future.Take();
  CHECK(result.has_value()) << result.error().message();
  return std::move(*result);
}

void CloseReaderAndWait(SignedWebBundleReader& reader) {
  base::test::TestFuture<void> future;
  reader.Close(future.GetCallback());
  CHECK(future.Wait());
}

web_package::mojom::BundleResponsePtr ReadResponseAndWait(
    SignedWebBundleReader& reader,
    const GURL& url) {
  network::ResourceRequest request;
  request.url = url;
  base::test::TestFuture<
      base::expected<web_package::mojom::BundleResponsePtr,
                     SignedWebBundleReader::ReadResponseError>>
      future;
  reader.ReadResponse(request, future.GetCallback());
  auto response = future.Take();
  CHECK(response.has_value()) << response.error().message;
  CHECK_EQ((*response)->response_code, 200);
  return std::move(*response);
}

GURL GetBaseUrl(const BundledIsolatedWebApp& bundle) {
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
             bundle.web_bundle_id())
      .origin()
      .GetURL();
}

std::vector<GURL> GetSubresourceUrls(const GURL& base_url) {
  std::vector<GURL> urls;
  urls.reserve(kNumSubresources);
  for (int i = 0; i < kNumSubresources; ++i) {
    urls.push_back(base_url.Resolve(
        base::StrCat({"/subresource_", base::NumberToString(i), ".js"})));
  }
  return urls;
}

class IsolatedWebAppPerfTest : public IsolatedWebAppBrowserTestHarness {
 protected:
  // Whether this is a real measurement run rather than the single-lap run
  // that happens by default.
  bool is_full_performance_run() const {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        kFullPerformanceRunSwitch);
  }

  int WarmupLaps(int full_run_laps) const {
    return is_full_performance_run() ? full_run_laps : 0;
  }

  int MeasuredLaps(int full_run_laps) const {
    return is_full_performance_run() ? full_run_laps : 1;
  }

  void Report(std::string_view metric_prefix,
              std::string_view time_unit,
              std::string_view story_name,
              base::TimeDelta total_time,
              int laps) {
    if (!is_full_performance_run()) {
      return;
    }
    perf_test::PerfResultReporter reporter(metric_prefix, story_name);
    reporter.RegisterImportantMetric(kMetricOperationTime, time_unit);
    reporter.RegisterImportantMetric(kMetricThroughputOps, "ops/s");
    reporter.AddResult(kMetricOperationTime, total_time / laps);
    reporter.AddResult(kMetricThroughputOps,
                       static_cast<double>(laps) / total_time.InSecondsF());
  }

  void NavigateAndAwaitLoaded(BrowserWindowInterface* app_browser,
                              const GURL& base_url,
                              int lap) {
    content::WebContents* web_contents =
        app_browser->GetTabStripModel()->GetActiveWebContents();
    std::string query = base::StrCat({"?lap=", base::NumberToString(lap)});
    std::u16string expected_title =
        base::ASCIIToUTF16(base::StrCat({"LOADED", query}));
    content::TitleWatcher title_watcher(web_contents, expected_title);
    GURL target_url = base_url.Resolve(base::StrCat({"/index.html", query}));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, target_url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  // Times `SignedWebBundleReader::Create()`, which parses the integrity block
  // and the metadata of the bundle.
  void RunCreateReaderStory(std::string_view story_name,
                            bool verify_signatures) {
    std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
        BuildSubresourceHeavyBundle();
    const GURL base_url = GetBaseUrl(*bundle);

    for (int i = 0; i < WarmupLaps(kCreateReaderWarmupLaps); ++i) {
      CloseReaderAndWait(
          *CreateReaderAndWait(bundle->path(), base_url, verify_signatures));
    }

    const int laps = MeasuredLaps(kCreateReaderMeasuredLaps);
    base::TimeDelta total_create_time;
    for (int i = 0; i < laps; ++i) {
      base::TimeTicks start = base::TimeTicks::Now();
      std::unique_ptr<SignedWebBundleReader> reader =
          CreateReaderAndWait(bundle->path(), base_url, verify_signatures);
      total_create_time += (base::TimeTicks::Now() - start);
      CloseReaderAndWait(*reader);
    }

    Report(kMetricPrefixReader, "us", story_name, total_create_time, laps);
  }

  // Times `SignedWebBundleReader::ReadResponse()` on a ready reader and, if
  // `read_body` is true, reading the whole response body as well.
  void RunReadStory(std::string_view story_name, bool read_body) {
    std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
        BuildSubresourceHeavyBundle();
    const GURL base_url = GetBaseUrl(*bundle);
    const std::vector<GURL> urls = GetSubresourceUrls(base_url);
    std::unique_ptr<SignedWebBundleReader> reader = CreateReaderAndWait(
        bundle->path(), base_url, /*verify_signatures=*/false);

    const std::string expected_body = SubresourcePayload();
    size_t url_index = 0;
    auto read_next = [&] {
      web_package::mojom::BundleResponsePtr response =
          ReadResponseAndWait(*reader, urls[url_index]);
      url_index = (url_index + 1) % urls.size();
      if (read_body) {
        CHECK_EQ(ReadAndFulfillResponseBody(*reader, std::move(response)),
                 expected_body);
      }
    };

    for (int i = 0; i < WarmupLaps(kReadWarmupLaps); ++i) {
      read_next();
    }

    const int laps = MeasuredLaps(kReadMeasuredLaps);
    base::TimeDelta total_read_time;
    for (int i = 0; i < laps; ++i) {
      base::TimeTicks start = base::TimeTicks::Now();
      read_next();
      total_read_time += (base::TimeTicks::Now() - start);
    }

    CloseReaderAndWait(*reader);
    Report(kMetricPrefixReader, "us", story_name, total_read_time, laps);
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPerfTest, Install) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      BuildSubresourceHeavyBundle();
  bundle->TrustSigningKey();

  for (int i = 0; i < WarmupLaps(kInstallWarmupLaps); ++i) {
    IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());
    test::UninstallWebApp(profile(), url_info.app_id());
  }

  const int laps = MeasuredLaps(kInstallMeasuredLaps);
  base::TimeDelta total_install_time;
  for (int i = 0; i < laps; ++i) {
    base::TimeTicks start = base::TimeTicks::Now();
    IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());
    total_install_time += (base::TimeTicks::Now() - start);
    test::UninstallWebApp(profile(), url_info.app_id());
  }

  Report(kMetricPrefixIWA, "ms", "Install", total_install_time, laps);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPerfTest, ColdPageLoad) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      BuildSubresourceHeavyBundle();
  bundle->TrustSigningKey();
  IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());

  base::FilePath installed_bundle_path =
      GetInstalledBundlePath(profile(), url_info.app_id());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  BrowserWindowInterface* app_browser = GetBrowserFromFrame(app_frame);
  GURL base_url = url_info.origin().GetURL();

  int lap_counter = 0;
  for (int i = 0; i < WarmupLaps(kColdLoadWarmupLaps); ++i) {
    ClearReaderCache(profile(), installed_bundle_path);
    NavigateAndAwaitLoaded(app_browser, base_url, ++lap_counter);
  }

  const int laps = MeasuredLaps(kColdLoadMeasuredLaps);
  base::TimeDelta total_load_time;
  for (int i = 0; i < laps; ++i) {
    ClearReaderCache(profile(), installed_bundle_path);
    base::TimeTicks start = base::TimeTicks::Now();
    NavigateAndAwaitLoaded(app_browser, base_url, ++lap_counter);
    total_load_time += (base::TimeTicks::Now() - start);
  }

  Report(kMetricPrefixIWA, "ms", "ColdPageLoad", total_load_time, laps);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPerfTest, WarmPageLoad) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      BuildSubresourceHeavyBundle();
  bundle->TrustSigningKey();
  IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  BrowserWindowInterface* app_browser = GetBrowserFromFrame(app_frame);
  GURL base_url = url_info.origin().GetURL();

  int lap_counter = 0;
  for (int i = 0; i < WarmupLaps(kWarmLoadWarmupLaps); ++i) {
    NavigateAndAwaitLoaded(app_browser, base_url, ++lap_counter);
  }

  const int laps = MeasuredLaps(kWarmLoadMeasuredLaps);
  base::TimeDelta total_load_time;
  for (int i = 0; i < laps; ++i) {
    base::TimeTicks start = base::TimeTicks::Now();
    NavigateAndAwaitLoaded(app_browser, base_url, ++lap_counter);
    total_load_time += (base::TimeTicks::Now() - start);
  }

  Report(kMetricPrefixIWA, "ms", "WarmPageLoad", total_load_time, laps);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPerfTest,
                       CreateAndParseMetadata_VerifySignaturesTrue) {
  RunCreateReaderStory("CreateAndParseMetadata_VerifySignaturesTrue",
                       /*verify_signatures=*/true);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPerfTest,
                       CreateAndParseMetadata_VerifySignaturesFalse) {
  RunCreateReaderStory("CreateAndParseMetadata_VerifySignaturesFalse",
                       /*verify_signatures=*/false);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPerfTest, ReadResponseHeaders) {
  RunReadStory("ReadResponseHeaders", /*read_body=*/false);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPerfTest, ReadResponseAndBody) {
  RunReadStory("ReadResponseAndBody", /*read_body=*/true);
}

}  // namespace
}  // namespace web_app
