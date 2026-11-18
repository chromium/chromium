// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/pipeline.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/cancellation.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace update_client {

namespace {

class DeferredActionHandler : public ActionHandler {
 public:
  void Handle(const base::FilePath& /*action*/,
              const std::string& /*session_id*/,
              Callback callback) override {
    callback_ = std::move(callback);
    if (on_started_) {
      std::move(on_started_).Run();
    }
  }

  void WaitForStart() {
    if (callback_) {
      return;
    }
    base::RunLoop loop;
    on_started_ = loop.QuitClosure();
    loop.Run();
  }

  void Complete() {
    ASSERT_TRUE(callback_);
    std::move(callback_).Run(/*succeeded=*/true, /*error_code=*/0,
                             /*extra_code1=*/0);
  }

 private:
  ~DeferredActionHandler() override = default;

  base::OnceClosure on_started_;
  Callback callback_;
};

class RecordingDownloader : public CrxDownloader {
 public:
  bool started() const { return started_; }
  std::optional<CancelMode> cancelled_mode() const { return cancelled_mode_; }

 private:
  ~RecordingDownloader() override = default;

  CancelCallback DoStartDownload(const GURL& /*url*/) override {
    started_ = true;
    return base::BindOnce(&RecordingDownloader::CancelAttempt, this);
  }

  void CancelAttempt(CancelMode mode) {
    cancelled_mode_ = mode;
    DownloadMetrics metrics;
    metrics.url = url();
    metrics.error = std::to_underlying(CrxDownloaderError::CANCELLED);
    OnDownloadComplete(/*is_handled=*/true, {.error = metrics.error}, metrics);
  }

  bool started_ = false;
  std::optional<CancelMode> cancelled_mode_;
};

class RecordingDownloaderFactory : public CrxDownloaderFactory {
 public:
  explicit RecordingDownloaderFactory(
      scoped_refptr<RecordingDownloader> downloader)
      : downloader_(std::move(downloader)) {}

  scoped_refptr<CrxDownloader> MakeCrxDownloader(
      const std::string& /*prod_id*/,
      bool /*background_download_enabled*/) const override {
    return downloader_;
  }

 private:
  ~RecordingDownloaderFactory() override = default;

  scoped_refptr<RecordingDownloader> downloader_;
};

}  // namespace

class PipelineTest : public testing::Test {
 public:
  PipelineTest() { RegisterPersistedDataPrefs(pref_->registry()); }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    config_ = base::MakeRefCounted<TestConfigurator>(pref_.get());
    downloader_ = base::MakeRefCounted<RecordingDownloader>();
    config_->SetCrxDownloaderFactory(
        base::MakeRefCounted<RecordingDownloaderFactory>(downloader_));
    action_handler_ = base::MakeRefCounted<DeferredActionHandler>();
  }

  CancelCallback StartPipeline(
      base::OnceCallback<void(const CategorizedError&)> done_callback) {
    ProtocolParser::Pipeline pipeline_def;
    pipeline_def.pipeline_id = "p1";
    pipeline_def.operations = {
        {.type = "run", .path = "action"},
        {.type = "download",
         .sha256_out = std::string(64, 'a'),
         .size = 10,
         .urls = {GURL("http://localhost/download/file.crx")}},
        {.type = "crx3", .sha256_in = std::string(64, 'a')},
    };
    ProtocolParser::App app;
    app.status = "ok";
    app.pipelines = {std::move(pipeline_def)};

    base::test::TestFuture<
        base::expected<PipelineStartCallback, CategorizedError>>
        pipeline_future;
    MakePipeline(
        config_, base::BindRepeating([](const base::FilePath&) -> int64_t {
          return 1 << 20;
        }),
        /*is_foreground=*/true, "session_id",
        base::MakeRefCounted<CrxCache>(std::nullopt),
        crx_file::VerifierFormat::CRX3, "appid", std::vector<uint8_t>{},
        /*install_data_index=*/"",
        base::MakeRefCounted<ReadOnlyTestInstaller>(temp_dir_.GetPath()),
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing(), base::DoNothing(), action_handler_, app,
        pipeline_future.GetCallback());
    auto start_callback = pipeline_future.Take();
    EXPECT_TRUE(start_callback.has_value());
    return std::move(*start_callback).Run(std::move(done_callback));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_ =
      std::make_unique<TestingPrefServiceSimple>();
  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestConfigurator> config_;
  scoped_refptr<RecordingDownloader> downloader_;
  scoped_refptr<DeferredActionHandler> action_handler_;
};

TEST_F(PipelineTest, CancelDuringRunActionSkipsDownload) {
  base::test::TestFuture<CategorizedError> done;
  CancelCallback cancel =
      StartPipeline(done.GetCallback<const CategorizedError&>());
  action_handler_->WaitForStart();

  std::move(cancel).Run(CancelMode::kKeepPartialDownload);
  action_handler_->Complete();

  const CategorizedError error = done.Get();
  EXPECT_EQ(error.category, ErrorCategory::kService);
  EXPECT_EQ(error.code, std::to_underlying(ServiceError::CANCELLED));
  EXPECT_FALSE(downloader_->started());
}

TEST_F(PipelineTest, CancelWhileDownloadCacheCheckPending) {
  base::test::TestFuture<CategorizedError> done;
  CancelCallback cancel =
      StartPipeline(done.GetCallback<const CategorizedError&>());
  action_handler_->WaitForStart();

  // Complete() advances the pipeline to the download's async cache lookup.
  action_handler_->Complete();
  ASSERT_FALSE(downloader_->started());

  std::move(cancel).Run(CancelMode::kKeepPartialDownload);
  const CategorizedError error = done.Get();

  // SkipIfCached() currently starts the wrapped operation even when already
  // cancelled (current behavior, not a requirement).
  EXPECT_TRUE(downloader_->started());
  EXPECT_EQ(downloader_->cancelled_mode(), CancelMode::kKeepPartialDownload);
  EXPECT_EQ(error.category, ErrorCategory::kService);
  EXPECT_EQ(error.code, std::to_underlying(ServiceError::CANCELLED));
}

}  // namespace update_client
