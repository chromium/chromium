// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_TEST_WITH_WEB_STATE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_TEST_WITH_WEB_STATE_H_

#import "ios/web/public/test/web_test_with_web_state.h"

namespace web {
class WebClient;
class WebFrame;
}  // namespace web

namespace autofill::test {
// Test delay used for throttling form mutation messages.
inline constexpr int kTrackFormMutationsDelayInMs = 10;

struct AutofillPlaceholderConfig {
  bool autofill_across_iframes_enabled = true;
  bool autofill_across_iframes_throttling = true;
  bool autofill_disallow_more_hyphen_like_labels = false;
  bool autofill_support_date_input = false;
  bool autofill_correct_user_edited_bit_in_parsed_field = true;
  bool autofill_allow_default_prevented_submission = true;
  bool autofill_dedupe_form_submission = true;
  bool autofill_email_verification = false;
  bool autofill_report_form_submission_errors = false;
  bool autofill_count_form_submission_in_renderer = true;
};

// Returns JavaScript script to inject default feature flag placeholders in
// tests.
NSString* GetAutofillTestPlaceholders(
    const AutofillPlaceholderConfig& config = {});
}  // namespace autofill::test

// A fixture to set up testing of Autofill methods.
class AutofillTestWithWebState : public web::WebTestWithWebState {
 protected:
  AutofillTestWithWebState(std::unique_ptr<web::WebClient> web_client);

  // Toggles tracking form mutations in a |frame| and waits for completion.
  void TrackFormMutations(web::WebFrame* frame);

  // web::WebTestWithWebState:
  id ExecuteJavaScript(NSString* script) override;
};

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_TEST_WITH_WEB_STATE_H_
