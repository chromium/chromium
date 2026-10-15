// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_download.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_engine.h"
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace update_client {

namespace {

// The outcome of one downloader in a fake chain: the content it produces on
// success or the error it fails with, and the metrics it reports.
struct FakeDownloadStep {
  base::expected<std::string, int> result;
  CrxDownloader::DownloadMetrics metrics;
};

class FakeDownloader : public CrxDownloader {
 public:
  FakeDownloader(scoped_refptr<CrxDownloader> successor,
                 const base::FilePath& dest,
                 const FakeDownloadStep& step)
      : CrxDownloader(std::move(successor)), dest_(dest), step_(step) {}

  base::OnceClosure DoStartDownload(const GURL& url) override {
    if (step_.result.has_value()) {
      base::WriteFile(dest_, step_.result.value());
      OnDownloadComplete(true, {.response = dest_}, step_.metrics);
    } else if (step_.result.error() ==
               std::to_underlying(CrxDownloaderError::CANCELLED)) {
      return base::BindOnce(&FakeDownloader::OnDownloadComplete, this, true,
                            Result{.error = step_.result.error()},
                            step_.metrics);
    } else {
      // A failure which is not handled falls through to the next url, or to
      // the successor in the chain, if any.
      OnDownloadComplete(false, {.error = step_.result.error()}, step_.metrics);
    }
    return base::DoNothing();
  }

 protected:
  ~FakeDownloader() override = default;

 private:
  const base::FilePath dest_;
  const FakeDownloadStep step_;
};

// Builds a chain of fake downloaders, one per step, in the order given.
class FakeFactory : public CrxDownloaderFactory {
 public:
  FakeFactory(const base::FilePath& dest, std::vector<FakeDownloadStep> steps)
      : dest_(dest), steps_(std::move(steps)) {}

  scoped_refptr<CrxDownloader> MakeCrxDownloader(
      const std::string& prod_id,
      bool background_download_enabled) const override {
    scoped_refptr<CrxDownloader> downloader;
    for (auto it = steps_.rbegin(); it != steps_.rend(); ++it) {
      downloader = base::MakeRefCounted<FakeDownloader>(std::move(downloader),
                                                        dest_, *it);
    }
    return downloader;
  }

 protected:
  ~FakeFactory() override = default;

 private:
  const base::FilePath dest_;
  const std::vector<FakeDownloadStep> steps_;
};

}  // namespace

class OpDownloadTest : public testing::Test {
 public:
  OpDownloadTest() { RegisterPersistedDataPrefs(pref_->registry()); }

  // Overrides from testing::Test.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  scoped_refptr<TestConfigurator> MakeConfigurator(
      base::expected<std::string, int> result,
      const CrxDownloader::DownloadMetrics& metrics) {
    return MakeConfigurator({{result, metrics}});
  }

  scoped_refptr<TestConfigurator> MakeConfigurator(
      std::vector<FakeDownloadStep> steps) {
    scoped_refptr<TestConfigurator> config =
        base::MakeRefCounted<TestConfigurator>(pref_.get());
    config->SetCrxDownloaderFactory(base::MakeRefCounted<FakeFactory>(
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("OpDownloadTest_File")),
        std::move(steps)));
    return config;
  }

  CrxDownloader::ProgressCallback MakeProgressCallback() {
    return base::DoNothing();
  }

  base::RepeatingCallback<void(base::DictValue)> MakePingCallback() {
    return base::BindLambdaForTesting(
        [&](base::DictValue ping) { pings_.push_back(std::move(ping)); });
  }

  base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
  MakeDoneCallback() {
    return base::BindLambdaForTesting(
        [&](base::expected<base::FilePath, CategorizedError> outcome) {
          outcome_ = outcome;
          runloop_.Quit();
        });
  }

  void Download(scoped_refptr<Configurator> config,
                int64_t length,
                const std::string& hash) {
    DownloadOperation(config, "appid",
                      /*is_foreground=*/false, {GURL("http://localhost:111")},
                      length, hash, MakePingCallback(), base::DoNothing(),
                      MakeProgressCallback(), {}, MakeDoneCallback());
    runloop_.Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_ =
      std::make_unique<TestingPrefServiceSimple>();
  base::ScopedTempDir temp_dir_;
  base::RunLoop runloop_;

  std::vector<base::DictValue> pings_;
  base::expected<base::FilePath, CategorizedError> outcome_;
};

TEST_F(OpDownloadTest, DownloadSuccess) {
  base::HistogramTester histograms;
  const std::string data = "data";
  Download(
      MakeConfigurator(
          data, {.url = GURL("http://test"),
                 .downloader =
                     CrxDownloader::DownloadMetrics::Downloader::kUrlFetcher,
                 .error = 0,
                 .extra_code1 = 34,
                 .downloaded_bytes = 56,
                 .total_bytes = 78,
                 .download_time_ms = 90}),
      data.length(), base::HexEncode(crypto::hash::Sha256(data)));
  EXPECT_TRUE(outcome_.has_value());
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), 14);
  EXPECT_EQ(pings_[0].FindInt("eventresult"), 1);
  EXPECT_EQ(pings_[0].Find("errorcode"), nullptr);
  EXPECT_EQ(pings_[0].FindInt("extracode1"), 34);
  EXPECT_EQ(pings_[0].FindDouble("downloaded"), 56);
  EXPECT_EQ(pings_[0].FindDouble("total"), 78);
  EXPECT_EQ(pings_[0].FindDouble("download_time_ms"), 90);
  EXPECT_EQ(CHECK_DEREF(pings_[0].FindString("downloader")), "direct");

  histograms.ExpectUniqueSample("UpdateClient.CrxDownloader.UrlFetcher.Result",
                                true, 1);
  histograms.ExpectUniqueTimeSample(
      "UpdateClient.CrxDownloader.UrlFetcher.DownloadTime",
      base::Milliseconds(90), 1);
  histograms.ExpectUniqueSample(
      "UpdateClient.CrxDownloader.FinalDownloader",
      CrxDownloader::DownloadMetrics::Downloader::kUrlFetcher, 1);
  histograms.ExpectUniqueTimeSample("UpdateClient.DownloadTime2",
                                    base::Milliseconds(90), 1);
}

TEST_F(OpDownloadTest, DownloadFailure) {
  base::HistogramTester histograms;
  const std::string data = "data";
  Download(MakeConfigurator(
               base::unexpected(404),
               {.url = GURL("http://test"),
                .downloader =
                    CrxDownloader::DownloadMetrics::Downloader::kUrlFetcher,
                .error = 404,
                .extra_code1 = 34,
                .downloaded_bytes = 56,
                .total_bytes = 78,
                .download_time_ms = 90}),
           data.length(), base::HexEncode(crypto::hash::Sha256(data)));
  EXPECT_FALSE(outcome_.has_value());
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), 14);
  EXPECT_EQ(pings_[0].FindInt("eventresult"), 0);
  EXPECT_EQ(pings_[0].FindInt("errorcode"), 404);
  EXPECT_EQ(pings_[0].FindInt("extracode1"), 34);
  EXPECT_EQ(pings_[0].FindDouble("downloaded"), 56);
  EXPECT_EQ(pings_[0].FindDouble("total"), 78);
  EXPECT_EQ(pings_[0].FindDouble("download_time_ms"), 90);
  EXPECT_EQ(CHECK_DEREF(pings_[0].FindString("downloader")), "direct");

  histograms.ExpectUniqueSample("UpdateClient.CrxDownloader.UrlFetcher.Result",
                                false, 1);
  histograms.ExpectTotalCount(
      "UpdateClient.CrxDownloader.UrlFetcher.DownloadTime", 0);
  histograms.ExpectUniqueSample(
      "UpdateClient.CrxDownloader.FinalDownloader",
      CrxDownloader::DownloadMetrics::Downloader::kNone, 1);
  histograms.ExpectTotalCount("UpdateClient.DownloadTime2", 0);
}

// A chain where the first downloader fails and the second one succeeds, as
// when Delivery Optimization is unavailable and BITS takes over: both attempts
// are reported, and the final downloader is the one which succeeded.
TEST_F(OpDownloadTest, DownloadFallsThroughChain) {
  base::HistogramTester histograms;
  const std::string data = "data";
  constexpr int kClassNotRegistered = static_cast<int>(0x80040154);
  Download(
      MakeConfigurator(
          {{.result = base::unexpected(kClassNotRegistered),
            .metrics = {.url = GURL("http://test"),
                        .downloader = CrxDownloader::DownloadMetrics::
                            Downloader::kDeliveryOptimization,
                        .error = kClassNotRegistered,
                        .download_time_ms = 10}},
           {.result = data,
            .metrics = {.url = GURL("http://test"),
                        .downloader =
                            CrxDownloader::DownloadMetrics::Downloader::kBits,
                        .error = 0,
                        .downloaded_bytes = 4,
                        .total_bytes = 4,
                        .download_time_ms = 20}}}),
      data.length(), base::HexEncode(crypto::hash::Sha256(data)));
  EXPECT_TRUE(outcome_.has_value());
  ASSERT_EQ(pings_.size(), 2u);
  EXPECT_EQ(CHECK_DEREF(pings_[0].FindString("downloader")),
            "delivery_optimization");
  EXPECT_EQ(pings_[0].FindInt("eventresult"), 0);
  EXPECT_EQ(pings_[0].FindInt("errorcode"), kClassNotRegistered);
  EXPECT_EQ(CHECK_DEREF(pings_[1].FindString("downloader")), "bits");
  EXPECT_EQ(pings_[1].FindInt("eventresult"), 1);

  histograms.ExpectUniqueSample(
      "UpdateClient.CrxDownloader.DeliveryOptimization.Result", false, 1);
  histograms.ExpectUniqueSample(
      "UpdateClient.CrxDownloader.DeliveryOptimization.Error",
      kClassNotRegistered, 1);
  histograms.ExpectTotalCount(
      "UpdateClient.CrxDownloader.DeliveryOptimization.DownloadTime", 0);
  histograms.ExpectUniqueSample("UpdateClient.CrxDownloader.Bits.Result", true,
                                1);
  histograms.ExpectTotalCount("UpdateClient.CrxDownloader.Bits.Error", 0);
  histograms.ExpectUniqueTimeSample(
      "UpdateClient.CrxDownloader.Bits.DownloadTime", base::Milliseconds(20),
      1);
  histograms.ExpectUniqueSample(
      "UpdateClient.CrxDownloader.FinalDownloader",
      CrxDownloader::DownloadMetrics::Downloader::kBits, 1);
  histograms.ExpectUniqueTimeSample("UpdateClient.DownloadTime2",
                                    base::Milliseconds(20), 1);
}

TEST_F(OpDownloadTest, DownloadCancelled) {
  base::HistogramTester histograms;
  const std::string data = "data";
  constexpr int kCancelled = std::to_underlying(CrxDownloaderError::CANCELLED);
  base::OnceClosure cancel = DownloadOperation(
      MakeConfigurator(
          base::unexpected(kCancelled),
          {.url = GURL("http://test"),
           .downloader =
               CrxDownloader::DownloadMetrics::Downloader::kUrlFetcher,
           .error = kCancelled,
           .downloaded_bytes = 12,
           .total_bytes = 78,
           .download_time_ms = 90}),
      "appid", /*is_foreground=*/false, {GURL("http://localhost:111")},
      data.length(), base::HexEncode(crypto::hash::Sha256(data)),
      MakePingCallback(), base::DoNothing(), MakeProgressCallback(), {},
      MakeDoneCallback());
  std::move(cancel).Run();
  runloop_.Run();

  ASSERT_FALSE(outcome_.has_value());
  EXPECT_EQ(outcome_.error().category, ErrorCategory::kService);
  EXPECT_EQ(outcome_.error().code, std::to_underlying(ServiceError::CANCELLED));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventresult"), 0);
  EXPECT_EQ(pings_[0].FindInt("errorcode"), kCancelled);

  histograms.ExpectTotalCount("UpdateClient.CrxDownloader.UrlFetcher.Result",
                              0);
  histograms.ExpectTotalCount("UpdateClient.CrxDownloader.FinalDownloader", 0);
  histograms.ExpectTotalCount("UpdateClient.DownloadTime2", 0);
}

}  // namespace update_client
