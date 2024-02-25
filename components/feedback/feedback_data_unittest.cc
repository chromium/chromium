// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_data.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

namespace {

constexpr char kHistograms[] = "Histogram Data";
constexpr char kImageData[] = "Image Data";
constexpr char kFileData[] = "File Data";
constexpr char kAutofillMetadata[] = "Autofill Metadata";

class MockUploader : public FeedbackUploader {
 public:
  MockUploader(
      bool is_off_the_record,
      const base::FilePath& state_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::OnceClosure on_report_sent)
      : FeedbackUploader(is_off_the_record, state_path, url_loader_factory),
        on_report_sent_(std::move(on_report_sent)) {}

  MockUploader(const MockUploader&) = delete;
  MockUploader& operator=(const MockUploader&) = delete;

  // feedback::FeedbackUploader:
  void StartDispatchingReport() override { std::move(on_report_sent_).Run(); }
  base::WeakPtr<FeedbackUploader> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void QueueReport(std::unique_ptr<std::string> data,
                   bool has_email,
                   int product_id) override {
    report_had_email_ = has_email;
    called_queue_report_ = true;
    FeedbackUploader::QueueReport(std::move(data), has_email, product_id);
  }

  bool called_queue_report() const { return called_queue_report_; }
  bool report_had_email() const { return report_had_email_; }

 private:
  base::OnceClosure on_report_sent_;
  bool called_queue_report_ = false;
  bool report_had_email_ = false;
  base::WeakPtrFactory<MockUploader> weak_ptr_factory_{this};
};

}  // namespace

class FeedbackDataTest : public testing::Test {
 protected:
  FeedbackDataTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    uploader_ = std::make_unique<MockUploader>(
        /*is_off_the_record=*/false, scoped_temp_dir_.GetPath(),
        test_shared_loader_factory_,
        base::BindOnce(&FeedbackDataTest::set_send_report_callback,
                       base::Unretained(this)));
    data_ = base::MakeRefCounted<FeedbackData>(uploader_->AsWeakPtr(), nullptr);
  }

  void Send() {
    bool attached_file_completed = data_->attached_file_uuid().empty();
    bool screenshot_completed = data_->screenshot_uuid().empty();

    if (screenshot_completed && attached_file_completed) {
      data_->OnFeedbackPageDataComplete();
    }
  }

  void RunMessageLoop() {
    run_loop_ = std::make_unique<base::RunLoop>();
    quit_closure_ = run_loop_->QuitClosure();
    Send();
    run_loop_->Run();
  }

  void set_send_report_callback() { std::move(quit_closure_).Run(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::OnceClosure quit_closure_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::ScopedTempDir scoped_temp_dir_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<MockUploader> uploader_;
  scoped_refptr<FeedbackData> data_;
};

TEST_F(FeedbackDataTest, ReportSending) {
  data_->SetAndCompressHistograms(kHistograms);
  data_->set_autofill_metadata(kAutofillMetadata);
  data_->CompressAutofillMetadata();
  data_->set_image(kImageData);
  data_->AttachAndCompressFileData(kFileData);
  RunMessageLoop();
  EXPECT_EQ(data_->user_email(), "");
  EXPECT_TRUE(data_->IsDataComplete());
  EXPECT_TRUE(uploader_->called_queue_report());
  EXPECT_FALSE(uploader_->report_had_email());
}

TEST_F(FeedbackDataTest, ReportSendingWithEmail) {
  data_->SetAndCompressHistograms(kHistograms);
  data_->set_autofill_metadata(kAutofillMetadata);
  data_->CompressAutofillMetadata();
  data_->set_image(kImageData);
  data_->AttachAndCompressFileData(kFileData);
  data_->set_user_email("foo@bar.com");
  RunMessageLoop();
  EXPECT_EQ(data_->user_email(), "foo@bar.com");
  EXPECT_TRUE(data_->IsDataComplete());
  EXPECT_TRUE(uploader_->called_queue_report());
  EXPECT_TRUE(uploader_->report_had_email());
}

TEST_F(FeedbackDataTest, ReportSendingAutofillMetadata) {
  data_->set_autofill_metadata(kAutofillMetadata);
  data_->CompressAutofillMetadata();
  RunMessageLoop();
  EXPECT_EQ(data_->user_email(), "");
  EXPECT_TRUE(data_->IsDataComplete());
  EXPECT_TRUE(uploader_->called_queue_report());
  EXPECT_FALSE(uploader_->report_had_email());
  ASSERT_EQ(data_->attachments(), 1UL);
  EXPECT_EQ(data_->attachment(0)->name, "autofill_metadata.zip");
}

}  // namespace feedback
