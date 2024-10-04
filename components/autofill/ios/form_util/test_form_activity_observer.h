// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_OBSERVER_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_OBSERVER_H_

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/form_util/form_activity_observer.h"
#import "components/autofill/ios/form_util/form_activity_params.h"

namespace web {
class WebState;
}

namespace autofill {
// Arguments passed to |DocumentSubmitted|.
struct TestSubmitDocumentInfo {
  TestSubmitDocumentInfo();
  raw_ptr<web::WebState> web_state = nullptr;
  raw_ptr<web::WebFrame> sender_frame = nullptr;
  FormData form_data;
  bool has_user_gesture;
};

// Arguments passed to |FormActivityRegistered|.
struct TestFormActivityInfo {
  raw_ptr<web::WebState> web_state = nullptr;
  raw_ptr<web::WebFrame> sender_frame = nullptr;
  FormActivityParams form_activity;
};

// Arguments passed to |FormRemovalRegistered|.
struct TestFormRemovalInfo {
  raw_ptr<web::WebState> web_state = nullptr;
  raw_ptr<web::WebFrame> sender_frame = nullptr;
  FormRemovalParams form_removal_params;
};

class TestFormActivityObserver : public autofill::FormActivityObserver {
 public:
  explicit TestFormActivityObserver(web::WebState* web_state);

  TestFormActivityObserver(const TestFormActivityObserver&) = delete;
  TestFormActivityObserver& operator=(const TestFormActivityObserver&) = delete;

  ~TestFormActivityObserver() override;

  // Arguments passed to |DocumentSubmitted|.
  TestSubmitDocumentInfo* submit_document_info();

  // Arguments passed to |FormActivityRegistered|.
  TestFormActivityInfo* form_activity_info();

  // Arguments passed to |FormRemoved|.
  TestFormRemovalInfo* form_removal_info();

  // How many times |DocumentSubmitted|, |FormActivityRegistered| and
  // |FormRemoved| were called.
  int number_of_events_received();

  void DocumentSubmitted(web::WebState* web_state,
                         web::WebFrame* sender_frame,
                         const FormData& form_data,
                         bool has_user_gesture) override;

  void FormActivityRegistered(web::WebState* web_state,
                              web::WebFrame* sender_frame,
                              const FormActivityParams& params) override;

  void FormRemoved(web::WebState* web_state,
                   web::WebFrame* sender_frame,
                   const FormRemovalParams& params) override;

 private:
  raw_ptr<web::WebState> web_state_ = nullptr;
  std::unique_ptr<TestSubmitDocumentInfo> submit_document_info_;
  std::unique_ptr<TestFormActivityInfo> form_activity_info_;
  std::unique_ptr<TestFormRemovalInfo> form_removal_info_;
  int number_of_events_received_ = 0;
};
}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_OBSERVER_H_
