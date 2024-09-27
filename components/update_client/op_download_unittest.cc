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
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_engine.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace update_client {

namespace {

class FakeDownloader : public CrxDownloader {
 public:
  FakeDownloader(const base::FilePath& dest,
                 const base::expected<std::string, int>& result,
                 const CrxDownloader::DownloadMetrics& metrics)
      : CrxDownloader(nullptr),
        dest_(dest),
        result_(result),
        metrics_(metrics) {}

  base::OnceClosure DoStartDownload(const GURL& url) override {
    if (result_.has_value()) {
      base::WriteFile(dest_, result_.value());
      OnDownloadComplete(true, {.response = dest_}, metrics_);
    } else {
      OnDownloadComplete(true, {.error = result_.error()}, metrics_);
    }
    return base::DoNothing();
  }

 protected:
  ~FakeDownloader() override = default;

 private:
  const base::FilePath dest_;
  const base::expected<std::string, int> result_;
  const CrxDownloader::DownloadMetrics metrics_;
};

class FakeFactory : public CrxDownloaderFactory {
 public:
  FakeFactory(const base::FilePath dest,
              const base::expected<std::string, int>& result,
              const CrxDownloader::DownloadMetrics& metrics)
      : dest_(dest), result_(result), metrics_(metrics) {}

  scoped_refptr<CrxDownloader> MakeCrxDownloader(
      bool background_download_enabled) const override {
    return base::MakeRefCounted<FakeDownloader>(dest_, result_, metrics_);
  }

 protected:
  ~FakeFactory() override = default;

 private:
  const base::FilePath dest_;
  const base::expected<std::string, int> result_;
  const CrxDownloader::DownloadMetrics metrics_;
};

}  // namespace

class OpDownloadTest : public testing::Test {
 public:
  OpDownloadTest() { RegisterPersistedDataPrefs(pref_->registry()); }
  ~OpDownloadTest() override = default;

  // Overrides from testing::Test.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  scoped_refptr<UpdateContext> MakeUpdateContext(
      const base::expected<std::string, int>& result,
      const CrxDownloader::DownloadMetrics& metrics) {
    scoped_refptr<TestConfigurator> config =
        base::MakeRefCounted<TestConfigurator>(pref_.get());
    config->SetCrxDownloaderFactory(base::MakeRefCounted<FakeFactory>(
        temp_dir_.GetPath().AppendASCII("OpDownloadTest_File"), result,
        metrics));
    return base::MakeRefCounted<UpdateContext>(
        config,
        base::MakeRefCounted<CrxCache>(CrxCache::Options(temp_dir_.GetPath())),
        false, false, std::vector<std::string>(),
        UpdateClient::CrxStateChangeCallback(), UpdateEngine::Callback(),
        nullptr,
        /*is_update_check_only=*/false);
  }

  CrxDownloader::ProgressCallback MakeProgressCallback() {
    return base::DoNothing();
  }

  base::RepeatingCallback<void(base::Value::Dict)> MakePingCallback() {
    return base::BindLambdaForTesting(
        [&](base::Value::Dict ping) { pings_.push_back(std::move(ping)); });
  }

  base::OnceCallback<
      void(const base::expected<base::FilePath, CategorizedError>&)>
  MakeDoneCallback() {
    return base::BindLambdaForTesting(
        [&](const base::expected<base::FilePath, CategorizedError>& outcome) {
          outcome_ = outcome;
          runloop_.Quit();
        });
  }

  void Download(scoped_refptr<UpdateContext> context,
                int64_t length,
                const std::string& hash) {
    DownloadOperation(context, {GURL("http://localhost:111")}, length, hash,
                      MakePingCallback(), MakeProgressCallback(),
                      MakeDoneCallback());
    runloop_.Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_ =
      std::make_unique<TestingPrefServiceSimple>();
  base::ScopedTempDir temp_dir_;
  base::RunLoop runloop_;

  std::vector<base::Value::Dict> pings_;
  base::expected<base::FilePath, CategorizedError> outcome_;
};

TEST_F(OpDownloadTest, DownloadSuccess) {
  const std::string data = "data";
  Download(
      MakeUpdateContext(
          data, {.url = GURL("http://test"),
                 .downloader =
                     CrxDownloader::DownloadMetrics::Downloader::kUrlFetcher,
                 .error = 0,
                 .extra_code1 = 34,
                 .downloaded_bytes = 56,
                 .total_bytes = 78,
                 .download_time_ms = 90}),
      data.length(), base::HexEncode(crypto::SHA256HashString(data)));
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
}

TEST_F(OpDownloadTest, DownloadFailure) {
  const std::string data = "data";
  Download(MakeUpdateContext(
               base::unexpected(404),
               {.url = GURL("http://test"),
                .downloader =
                    CrxDownloader::DownloadMetrics::Downloader::kUrlFetcher,
                .error = 404,
                .extra_code1 = 34,
                .downloaded_bytes = 56,
                .total_bytes = 78,
                .download_time_ms = 90}),
           data.length(), base::HexEncode(crypto::SHA256HashString(data)));
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
}

}  // namespace update_client
