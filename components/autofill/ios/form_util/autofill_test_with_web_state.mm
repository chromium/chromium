// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"

#import "base/test/ios/wait_util.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_client.h"

using autofill::test::kTrackFormMutationsDelayInMs;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

AutofillTestWithWebState::AutofillTestWithWebState(
    std::unique_ptr<web::WebClient> web_client)
    : web::WebTestWithWebState(std::move(web_client)) {}

void AutofillTestWithWebState::TrackFormMutations(web::WebFrame* frame) {
  // Override |__gCrWeb.formHandlers.trackFormMutations| to set a boolean
  // trackFormMutationsComplete after the function is called.
  ExecuteJavaScript(
      @"var trackFormMutationsComplete = false;"
      @"const formHandlersApi = __gCrWeb.getRegisteredApi('formHandlers');"
      @"var originalTrackFormMutations = "
      @"formHandlersApi.getFunction('trackFormMutations');"
      @"formHandlersApi.addFunction('trackFormMutations', function() {"
      @"  var result = originalTrackFormMutations.apply(this, arguments);"
      @"  trackFormMutationsComplete = true;"
      @"  return result;"
      @"});");

  autofill::FormHandlersJavaScriptFeature::GetInstance()->TrackFormMutations(
      frame, kTrackFormMutationsDelayInMs);

  // Wait for |TrackFormMutations| to add form listeners.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return [ExecuteJavaScript(@"trackFormMutationsComplete") boolValue];
  }));
}

id AutofillTestWithWebState::ExecuteJavaScript(NSString* script) {
  // Pass all JavaScript execution to the correct content world. Note that
  // although `FormHandlersJavaScriptFeature` is specified, all autofill
  // features must live in the same content world so any one of them could be
  // used here.
  return web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(), script,
      autofill::FormHandlersJavaScriptFeature::GetInstance());
}

namespace autofill::test {

NSString* GetAutofillTestPlaceholders(const AutofillPlaceholderConfig& config) {
  return [NSString stringWithFormat:
      @"var gCrWebPlaceholderAutofillAcrossIframesEnabled = %s;"
       "var gCrWebPlaceholderAutofillAcrossIframesThrottling = %s;"
       "var gCrWebPlaceholderAutofillDisallowMoreHyphenLikeLabels = %s;"
       "var gCrWebPlaceholderAutofillSupportDateInput = %s;"
       "var gCrWebPlaceholderAutofillCorrectUserEditedBitInParsedField = %s;"
       "var gCrWebPlaceholderAutofillAllowDefaultPreventedSubmission = %s;"
       "var gCrWebPlaceholderAutofillDedupeFormSubmission = %s;"
       "var gCrWebPlaceholderAutofillEmailVerification = %s;"
       "var gCrWebPlaceholderAutofillReportFormSubmissionErrors = %s;"
       "var gCrWebPlaceholderAutofillCountFormSubmissionInRenderer = %s;",
      config.autofill_across_iframes_enabled ? "true" : "false",
      config.autofill_across_iframes_throttling ? "true" : "false",
      config.autofill_disallow_more_hyphen_like_labels ? "true" : "false",
      config.autofill_support_date_input ? "true" : "false",
      config.autofill_correct_user_edited_bit_in_parsed_field ? "true" : "false",
      config.autofill_allow_default_prevented_submission ? "true" : "false",
      config.autofill_dedupe_form_submission ? "true" : "false",
      config.autofill_email_verification ? "true" : "false",
      config.autofill_report_form_submission_errors ? "true" : "false",
      config.autofill_count_form_submission_in_renderer ? "true" : "false"];
}

}  // namespace autofill::test
