// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/test_form_activity_observer.h"

#import "components/autofill/core/common/form_data.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestSubmitDocumentInfo::TestSubmitDocumentInfo() = default;

TestFormActivityObserver::TestFormActivityObserver(web::WebState* web_state)
    : web_state_(web_state) {}
TestFormActivityObserver::~TestFormActivityObserver() = default;

TestSubmitDocumentInfo* TestFormActivityObserver::submit_document_info() {
  return submit_document_info_.get();
}

TestFormActivityInfo* TestFormActivityObserver::form_activity_info() {
  return form_activity_info_.get();
}

TestFormRemovalInfo* TestFormActivityObserver::form_removal_info() {
  return form_removal_info_.get();
}

int TestFormActivityObserver::number_of_events_received() {
  return number_of_events_received_;
}

void TestFormActivityObserver::DocumentSubmitted(web::WebState* web_state,
                                                 web::WebFrame* sender_frame,
                                                 const FormData& form_data,
                                                 bool has_user_gesture) {
  ASSERT_EQ(web_state_, web_state);
  number_of_events_received_++;
  submit_document_info_ = std::make_unique<TestSubmitDocumentInfo>();
  submit_document_info_->web_state = web_state;
  submit_document_info_->sender_frame = sender_frame;
  submit_document_info_->form_data = form_data;
  submit_document_info_->has_user_gesture = has_user_gesture;
}

void TestFormActivityObserver::FormActivityRegistered(
    web::WebState* web_state,
    web::WebFrame* sender_frame,
    const FormActivityParams& params) {
  ASSERT_EQ(web_state_, web_state);
  number_of_events_received_++;
  form_activity_info_ = std::make_unique<TestFormActivityInfo>();
  form_activity_info_->web_state = web_state;
  form_activity_info_->sender_frame = sender_frame;
  form_activity_info_->form_activity = params;
}

void TestFormActivityObserver::FormRemoved(web::WebState* web_state,
                                           web::WebFrame* sender_frame,
                                           const FormRemovalParams& params) {
  ASSERT_EQ(web_state_, web_state);
  number_of_events_received_++;
  form_removal_info_ = std::make_unique<TestFormRemovalInfo>();
  form_removal_info_->web_state = web_state;
  form_removal_info_->sender_frame = sender_frame;
  form_removal_info_->form_removal_params = params;
}

}  // namespace autofill
