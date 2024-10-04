// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/test_form_activity_tab_helper.h"

#include "base/observer_list.h"
#include "components/autofill/ios/form_util/form_activity_observer.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

namespace autofill {
TestFormActivityTabHelper::TestFormActivityTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

TestFormActivityTabHelper::~TestFormActivityTabHelper() = default;

void TestFormActivityTabHelper::FormActivityRegistered(
    web::WebFrame* sender_frame,
    FormActivityParams const& params) {
  autofill::FormActivityTabHelper* form_activity_tab_helper =
      autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state_);
  for (auto& observer : form_activity_tab_helper->observers_) {
    observer.FormActivityRegistered(web_state_, sender_frame, params);
  }
}

void TestFormActivityTabHelper::FormRemovalRegistered(
    web::WebFrame* sender_frame,
    const FormRemovalParams& params) {
  autofill::FormActivityTabHelper* form_activity_tab_helper =
      autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state_);
  for (auto& observer : form_activity_tab_helper->observers_) {
    observer.FormRemoved(web_state_, sender_frame, params);
  }
}

void TestFormActivityTabHelper::DocumentSubmitted(web::WebFrame* sender_frame,
                                                  const FormData& form_data,
                                                  bool has_user_gesture) {
  autofill::FormActivityTabHelper* form_activity_tab_helper =
      autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state_);
  for (auto& observer : form_activity_tab_helper->observers_) {
    observer.DocumentSubmitted(web_state_, sender_frame, form_data,
                               has_user_gesture);
  }
}
}  // namespace autofill
