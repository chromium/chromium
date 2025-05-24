// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_TAB_HELPER_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

struct FormActivityParams;
struct FormRemovalParams;
class FormData;

class TestFormActivityTabHelper {
 public:
  explicit TestFormActivityTabHelper(web::WebState* web_state);

  TestFormActivityTabHelper(const TestFormActivityTabHelper&) = delete;
  TestFormActivityTabHelper& operator=(const TestFormActivityTabHelper&) =
      delete;

  ~TestFormActivityTabHelper();

  void FormActivityRegistered(web::WebFrame* sender_frame,
                              const FormActivityParams& params);
  void FormRemovalRegistered(web::WebFrame* sender_frame,
                             const FormRemovalParams& params);
  void DocumentSubmitted(web::WebFrame* sender_frame,
                         const FormData& form_data,
                         bool has_user_gesture);

 private:
  raw_ptr<web::WebState> web_state_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_TEST_FORM_ACTIVITY_TAB_HELPER_H_
