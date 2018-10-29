// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_TAB_HELPER_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_TAB_HELPER_H_

#include <string>

#include "base/macros.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

struct FormActivityParams;

class TestFormActivityTabHelper {
 public:
  explicit TestFormActivityTabHelper(web::WebState* web_state);
  ~TestFormActivityTabHelper();

  void FormActivityRegistered(web::WebFrame* sender_frame,
                              const FormActivityParams& params);
  void DocumentSubmitted(web::WebFrame* sender_frame,
                         const std::string& form_name,
                         const std::string& form_data,
                         bool has_user_gesture,
                         bool form_in_main_frame);

 private:
  web::WebState* web_state_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestFormActivityTabHelper);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_TAB_HELPER_H_
