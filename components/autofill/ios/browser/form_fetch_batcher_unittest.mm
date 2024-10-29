// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_fetch_batcher.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

constexpr base::TimeDelta kBatchPeriodMs = base::Milliseconds(100);

// Generic form fetch completion callback that flips a bool to true when called.
void FormFetchCompletionCallback(
    bool* complete_ptr,
    std::optional<std::vector<autofill::FormData>> result) {
  *complete_ptr = true;
}

autofill::FormData MakeTestFormData(const std::u16string& name) {
  autofill::FormData form_data;
  form_data.set_name(name);
  return form_data;
}

}  // namespace

// AutofillDriverIosBridge used for testing. Provides a simple implementation of
// the methods that are used during testing, e.g. call the completion block upon
// calling -fetchFormsFiltered.
@interface TestAutofillDriverIOSBridge : NSObject <AutofillDriverIOSBridge>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithForms:(std::vector<autofill::FormData>)forms;

@end

@implementation TestAutofillDriverIOSBridge {
  std::vector<autofill::FormData> _forms;
}

- (instancetype)initWithForms:(std::vector<autofill::FormData>)forms {
  if ((self = [super init])) {
    _forms = std::move(forms);
  }
  return self;
}

- (void)fillData:(const std::vector<autofill::FormFieldData::FillData>&)form
         inFrame:(web::WebFrame*)frame {
}
- (void)fillSpecificFormField:(const autofill::FieldRendererId&)field
                    withValue:(const std::u16string)value
                      inFrame:(web::WebFrame*)frame {
}
- (void)handleParsedForms:
            (const std::vector<
                raw_ptr<autofill::FormStructure, VectorExperimental>>&)forms
                  inFrame:(web::WebFrame*)frame {
}
- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame {
}
- (void)scanFormsInWebState:(web::WebState*)webState
                    inFrame:(web::WebFrame*)webFrame {
}
- (void)notifyFormsSeen:(const std::vector<autofill::FormData>&)updatedForms
                inFrame:(web::WebFrame*)frame {
}
- (void)fetchFormsFiltered:(BOOL)filtered
                  withName:(const std::u16string&)formName
                   inFrame:(web::WebFrame*)frame
         completionHandler:(FormFetchCompletion)completionHandler {
  std::move(completionHandler).Run(_forms);
}

@end

class FormFetchBatcherTest : public PlatformTest {
 protected:
  FormFetchBatcherTest()
      : test_bridge_([[TestAutofillDriverIOSBridge alloc]
            initWithForms:{MakeTestFormData(u"form1"),
                           MakeTestFormData(u"form2")}]),
        fake_web_frame_(
            web::FakeWebFrame::Create("main_frame_id", true, GURL())),
        batcher_(test_bridge_,
                 fake_web_frame_.get()->AsWeakPtr(),
                 kBatchPeriodMs) {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestAutofillDriverIOSBridge* test_bridge_;
  std::unique_ptr<web::WebFrame> fake_web_frame_;
  autofill::FormFetchBatcher batcher_;
  base::HistogramTester histogram_tester_;
};

// Tests that the requests pushed to the scheduled batch are indeed completed
// once the batch is done. Tests with a batch of 2 requests, namely r1 and r2.
TEST_F(FormFetchBatcherTest, Batch) {
  // Completion trackers, true when the request is completed.
  bool r1_completed = false;
  bool r2_completed = false;

  // Verify that there is not any scheduled batch at this point, not until the
  // first request push.
  ASSERT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());

  // Push request #1 (r1).
  {
    batcher_.PushRequest(
        base::BindOnce(&FormFetchCompletionCallback, &r1_completed));
  }

  // Verify that the batch was scheduled by the first push.
  ASSERT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  // Advance time but not enough to hit the schedule.
  base::TimeDelta remaining_time = base::Milliseconds(50);
  task_environment_.AdvanceClock(kBatchPeriodMs - remaining_time);

  EXPECT_FALSE(r1_completed);
  EXPECT_FALSE(r2_completed);

  // Verif that the batch is still scheduled as the delay wasn't reached.
  ASSERT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  // Push request #2 (r2).
  {
    batcher_.PushRequest(
        base::BindOnce(&FormFetchCompletionCallback, &r2_completed));
  }

  // Verify that the new request was included in the same batch as r1, where
  // there is still only one pending batch but now with 2 requests in it.
  ASSERT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  // Run the scheduled batch. Advance the time so the scheduled task is run.
  task_environment_.FastForwardBy(remaining_time + base::Milliseconds(10));

  // Verify that the batch is done and no other batch was rescheduled since
  // there are no more requests in the queue.
  ASSERT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());

  // Verify that the batch of requests was completed by the batcher.
  EXPECT_TRUE(r1_completed);
  EXPECT_TRUE(r2_completed);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.iOS.FormExtraction.ForScan.BatchSize",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
}

// Tests that once a batch is done, another one can be scheduled.
TEST_F(FormFetchBatcherTest, Batch_Reschedule) {
  // Schedule an initial batch with request #1 in it (r1).

  // Completion tracker, true when the request is completed.
  auto r1_completed = std::make_unique<bool>(false);

  // Push request #1 (r1).
  {
    batcher_.PushRequest(
        base::BindOnce(&FormFetchCompletionCallback, r1_completed.get()));
  }

  // Advance time enough to trigger the first batch.
  task_environment_.FastForwardBy(kBatchPeriodMs + base::Milliseconds(50));

  EXPECT_TRUE(*r1_completed);
  *r1_completed = false;

  ASSERT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());

  // Schedule a new batch with request 2 in it (r2).

  // Completion trackers, true when the request is completed.
  auto r2_completed = std::make_unique<bool>(false);

  // Push request #2 (r2).
  {
    batcher_.PushRequest(
        base::BindOnce(&FormFetchCompletionCallback, r2_completed.get()));
  }

  // Verify that a new batch was scheduled.
  ASSERT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  // Advance time enough to trigger the first batch.
  task_environment_.FastForwardBy(kBatchPeriodMs + base::Milliseconds(50));

  ASSERT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());

  // Verify that the batch of requests was completed by the batcher.
  EXPECT_TRUE(*r2_completed);

  // As request #1 was already completed, it should not had been part of the
  // second batch.
  EXPECT_FALSE(*r1_completed);

  // Verify that each batch was recorded.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.iOS.FormExtraction.ForScan.BatchSize",
      /*sample=*/1,
      /*expected_bucket_count=*/2);
}

// Tests that a batch isn't scheduled if not needed (i.e. there are requests to
// be completed).
TEST_F(FormFetchBatcherTest, Batch_OnlyWhenNeeded) {
  // Advance time enough to trigger the first batch if there was one needed.
  task_environment_.AdvanceClock(kBatchPeriodMs + base::Milliseconds(50));

  // Verify that no batch was scheduled.
  ASSERT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());

  histogram_tester_.ExpectTotalCount(
      "Autofill.iOS.FormExtraction.ForScan.BatchSize",
      /*exprected_count=*/0);
}

// Tests fetch filtered requests.
TEST_F(FormFetchBatcherTest, Filtered) {
  // Hold the fetched forms for each request.
  std::vector<autofill::FormData> r1_forms;
  std::vector<autofill::FormData> r2_forms;

  auto callback = [](std::vector<autofill::FormData>* captured_forms,
                     std::optional<std::vector<autofill::FormData>> result) {
    CHECK(result);
    *captured_forms = *result;
  };

  // Push request #1 (r1).
  { batcher_.PushRequest(base::BindOnce(callback, &r1_forms), u"form1"); }
  // Push request #2 (r2).
  { batcher_.PushRequest(base::BindOnce(callback, &r2_forms), u"form2"); }

  task_environment_.FastForwardBy(kBatchPeriodMs + base::Milliseconds(50));

  // Verify that only the forms matching the name specified in the request are
  // returned for each request.
  EXPECT_THAT(r1_forms, testing::ElementsAre(testing::Property(
                            &autofill::FormData::name, u"form1")));
  EXPECT_THAT(r2_forms, testing::ElementsAre(testing::Property(
                            &autofill::FormData::name, u"form2")));
}
