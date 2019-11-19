// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/test_form_activity_tab_helper.h"

#include "base/observer_list.h"
#include "components/autofill/ios/form_util/form_activity_observer.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
TestFormActivityTabHelper::TestFormActivityTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

TestFormActivityTabHelper::~TestFormActivityTabHelper() {}

void TestFormActivityTabHelper::FormActivityRegistered(
    web::WebFrame* sender_frame,
    FormActivityParams const& params) {
  autofill::FormActivityTabHelper* form_activity_tab_helper =
      autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state_);
  for (auto& observer : form_activity_tab_helper->observers_) {
    observer.FormActivityRegistered(web_state_, sender_frame, params);
  }
}

void TestFormActivityTabHelper::DocumentSubmitted(web::WebFrame* sender_frame,
                                                  const std::string& form_name,
                                                  const std::string& form_data,
                                                  bool has_user_gesture,
                                                  bool form_in_main_frame) {
  autofill::FormActivityTabHelper* form_activity_tab_helper =
      autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state_);
  for (auto& observer : form_activity_tab_helper->observers_) {
    observer.DocumentSubmitted(web_state_, sender_frame, form_name, form_data,
                               has_user_gesture, form_in_main_frame);
  }
}
}  // namespace autofill
