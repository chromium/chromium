// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {

using ::testing::_;

}  // namespace

class ContentAutofillAssistantDriverTest : public testing::Test {
 public:
  ContentAutofillAssistantDriverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                         .AppendASCII("test")
                                         .AppendASCII("data")
                                         .AppendASCII("autofill_assistant")
                                         .AppendASCII("model")
                                         .AppendASCII("model.tflite");
    model_file_ = base::File(model_file_path,
                             (base::File::FLAG_OPEN | base::File::FLAG_READ));

    annotate_dom_model_service_ = std::make_unique<AnnotateDomModelService>(
        /* opt_guide= */ nullptr, /* background_task_runner= */ nullptr);

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    // Constructor of ContentAutofillAssistantDriver is private, cannot use
    // std::make_unique.
    driver_ = base::WrapUnique(new ContentAutofillAssistantDriver(
        web_contents_->GetPrimaryMainFrame()));
  }

  void SetUp() override {
    driver_->SetAnnotateDomModelService(annotate_dom_model_service_.get());
  }

  bool HasPendingCallbacks() { return !driver_->pending_calls_.empty(); }

 protected:
  // The task_environment_ must be first to guarantee other field creations run
  // in that environment.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<ContentAutofillAssistantDriver> driver_;
  std::unique_ptr<AnnotateDomModelService> annotate_dom_model_service_;
  base::File model_file_;

  base::MockCallback<base::OnceCallback<
      void(mojom::ModelStatus, base::File, const std::string&)>>
      callback_;
};

TEST_F(ContentAutofillAssistantDriverTest, GetLoadedModelFromService) {
  // Model has been loaded before.
  annotate_dom_model_service_->SetModelFileForTest(model_file_.Duplicate());

  EXPECT_CALL(callback_, Run(mojom::ModelStatus::kSuccess, _, _));

  driver_->GetAnnotateDomModel(/* timeout= */ base::Milliseconds(1000),
                               callback_.Get());

  EXPECT_FALSE(HasPendingCallbacks());
}

TEST_F(ContentAutofillAssistantDriverTest, GetModelFromServiceAfterLoading) {
  EXPECT_CALL(callback_, Run(mojom::ModelStatus::kSuccess, _, _));

  driver_->GetAnnotateDomModel(/* timeout= */ base::Milliseconds(1000),
                               callback_.Get());

  // Model loaded after being requested.
  annotate_dom_model_service_->SetModelFileForTest(model_file_.Duplicate());

  EXPECT_FALSE(HasPendingCallbacks());
}

TEST_F(ContentAutofillAssistantDriverTest, GetModelTimesOut) {
  EXPECT_CALL(callback_, Run(mojom::ModelStatus::kTimeout, _, _));

  driver_->GetAnnotateDomModel(/* timeout= */ base::Milliseconds(1000),
                               callback_.Get());

  // Model does not get loaded.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_FALSE(HasPendingCallbacks());
}

TEST_F(ContentAutofillAssistantDriverTest, MultipleParallelCalls) {
  EXPECT_CALL(callback_, Run(mojom::ModelStatus::kTimeout, _, _)).Times(3);

  driver_->GetAnnotateDomModel(/* timeout= */ base::Milliseconds(1000),
                               callback_.Get());
  driver_->GetAnnotateDomModel(/* timeout= */ base::Milliseconds(1000),
                               callback_.Get());
  driver_->GetAnnotateDomModel(/* timeout= */ base::Milliseconds(1000),
                               callback_.Get());

  // Model does not get loaded.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_FALSE(HasPendingCallbacks());
}

TEST_F(ContentAutofillAssistantDriverTest, EmptyOverrides) {
  EXPECT_CALL(callback_, Run(mojom::ModelStatus::kSuccess, _, std::string()));

  driver_->GetAnnotateDomModel(/* timeout= */ base::Milliseconds(1000),
                               callback_.Get());

  // Model loaded after being requested.
  annotate_dom_model_service_->SetModelFileForTest(model_file_.Duplicate());

  EXPECT_FALSE(HasPendingCallbacks());
}

class ContentAutofillAssistantDriverMissingDomServiceTest
    : public ContentAutofillAssistantDriverTest {
  // Do not set the dom model service.
  void SetUp() override {}
};

TEST_F(ContentAutofillAssistantDriverMissingDomServiceTest,
       MissingDomModelService) {
  EXPECT_CALL(callback_,
              Run(mojom::ModelStatus::kUnexpectedError, _, std::string()))
      .Times(1);

  driver_->GetAnnotateDomModel(/* timeout= */ base::Milliseconds(1000),
                               callback_.Get());

  EXPECT_FALSE(HasPendingCallbacks());
}

}  // namespace autofill_assistant
