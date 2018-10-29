// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/test_form_activity_observer.h"

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

TestSubmitDocumentInfo::TestSubmitDocumentInfo() {}

TestFormActivityObserver::TestFormActivityObserver(web::WebState* web_state)
    : web_state_(web_state) {}
TestFormActivityObserver::~TestFormActivityObserver() {}

TestSubmitDocumentInfo* TestFormActivityObserver::submit_document_info() {
  return submit_document_info_.get();
}

TestFormActivityInfo* TestFormActivityObserver::form_activity_info() {
  return form_activity_info_.get();
}

void TestFormActivityObserver::DocumentSubmitted(web::WebState* web_state,
                                                 web::WebFrame* sender_frame,
                                                 const std::string& form_name,
                                                 const std::string& form_data,
                                                 bool has_user_gesture,
                                                 bool form_in_main_frame) {
  ASSERT_EQ(web_state_, web_state);
  submit_document_info_ = std::make_unique<TestSubmitDocumentInfo>();
  submit_document_info_->web_state = web_state;
  submit_document_info_->sender_frame = sender_frame;
  submit_document_info_->form_name = form_name;
  submit_document_info_->form_data = form_data;
  submit_document_info_->has_user_gesture = has_user_gesture;
  submit_document_info_->form_in_main_frame = form_in_main_frame;
}

void TestFormActivityObserver::FormActivityRegistered(
    web::WebState* web_state,
    web::WebFrame* sender_frame,
    const FormActivityParams& params) {
  ASSERT_EQ(web_state_, web_state);
  form_activity_info_ = std::make_unique<TestFormActivityInfo>();
  form_activity_info_->web_state = web_state;
  form_activity_info_->sender_frame = sender_frame;
  form_activity_info_->form_activity = params;
}

}  // namespace autofill
