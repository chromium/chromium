// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/email_verification_handler.h"

#include <optional>
#include <string>

#include "base/test/test_future.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace autofill {

namespace {

using ::testing::_;

class EmailVerificationHandlerTest : public test::AutofillRendererTest {
 public:
  EmailVerificationHandlerTest() = default;
};

// Tests that the verification token is injected into the token field if the
// email field's current value still matches the verified email address.
TEST_F(EmailVerificationHandlerTest,
       EmailVerificationHandlerSharesTokenIfEmailMatches) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body>
    <form id="form">
      <input type="email" id="email" value="a@example.com">
      <input type="hidden" id="verification" autocomplete="email-verification-token">
    </form>
  </body>)");
  WaitForFormsSeen();

  blink::WebFormElement form_element =
      GetWebElementById("form").DynamicTo<blink::WebFormElement>();
  blink::WebFormControlElement email_element =
      GetFormControlElementById("email");
  blink::WebFormControlElement verification_element =
      GetFormControlElementById("verification");

  EXPECT_CALL(autofill_driver(),
              FormWithEmailVerificationTokenSubmitted(
                  _, form_util::GetFieldRendererId(email_element)));

  autofill_agent().SendEmailVerificationToken(
      form_util::GetFieldRendererId(email_element), "a@example.com",
      "evt_token_123");

  test_api(autofill_agent())
      .email_verification_handler()
      .WillSendSubmitEvent(form_element);

  EXPECT_EQ(verification_element.Value().Utf16(), u"evt_token_123");
}

// Tests that the verification token is NOT injected if the email field's
// current value has changed since verification.
TEST_F(EmailVerificationHandlerTest,
       EmailVerificationHandlerDoesNotShareTokenIfEmailChanges) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body>
    <form id="form">
      <input type="email" id="email" value="a@example.com">
      <input type="hidden" id="verification" autocomplete="email-verification-token">
    </form>
  </body>)");
  WaitForFormsSeen();

  blink::WebFormElement form_element =
      GetWebElementById("form").DynamicTo<blink::WebFormElement>();
  blink::WebFormControlElement email_element =
      GetFormControlElementById("email");
  blink::WebFormControlElement verification_element =
      GetFormControlElementById("verification");

  EXPECT_CALL(autofill_driver(), FormWithEmailVerificationTokenSubmitted(_, _))
      .Times(0);

  autofill_agent().SendEmailVerificationToken(
      form_util::GetFieldRendererId(email_element), "a@example.com",
      "evt_token_123");

  email_element.SetValue(blink::WebString::FromUtf16(u"b@example.com"));

  test_api(autofill_agent())
      .email_verification_handler()
      .WillSendSubmitEvent(form_element);

  EXPECT_EQ(verification_element.Value().Utf16(), u"");
}

// Tests that the verification token is NOT injected if the email field has
// been cleared since verification.
TEST_F(EmailVerificationHandlerTest,
       EmailVerificationHandlerDoesNotShareTokenIfEmailIsCleared) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body>
    <form id="form">
      <input type="email" id="email" value="a@example.com">
      <input type="hidden" id="verification" autocomplete="email-verification-token">
    </form>
  </body>)");
  WaitForFormsSeen();

  blink::WebFormElement form_element =
      GetWebElementById("form").DynamicTo<blink::WebFormElement>();
  blink::WebFormControlElement email_element =
      GetFormControlElementById("email");
  blink::WebFormControlElement verification_element =
      GetFormControlElementById("verification");

  EXPECT_CALL(autofill_driver(), FormWithEmailVerificationTokenSubmitted(_, _))
      .Times(0);

  autofill_agent().SendEmailVerificationToken(
      form_util::GetFieldRendererId(email_element), "a@example.com",
      "evt_token_123");

  email_element.SetValue(blink::WebString::FromUtf16(u""));

  test_api(autofill_agent())
      .email_verification_handler()
      .WillSendSubmitEvent(form_element);

  EXPECT_EQ(verification_element.Value().Utf16(), u"");
}

// Tests that GetNonceForEmailVerification correctly queries the nonce from
// the verification token field (including both type="hidden" and boolean hidden
// attribute) and ignores non-hidden fields.
TEST_F(EmailVerificationHandlerTest, GetNonceForEmailVerification) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body>
    <form id="form">
      <input type="email" id="email" value="a@example.com">
      <input type="hidden" id="verification" autocomplete="email-verification-token" nonce="test_nonce_123">
    </form>
    <form id="form_hidden_attr">
      <input type="email" id="email_hidden_attr" value="a@example.com">
      <input id="verification_hidden_attr" autocomplete="email-verification-token" nonce="test_nonce_456" hidden>
    </form>
    <form id="form_visible_token">
      <input type="email" id="email_visible" value="a@example.com">
      <input id="verification_visible" autocomplete="email-verification-token" nonce="test_nonce_789">
    </form>
    <form id="form_without_token">
      <input type="email" id="email2" value="b@example.com">
    </form>
  </body>)");
  WaitForFormsSeen();

  blink::WebFormControlElement email_element =
      GetFormControlElementById("email");
  blink::WebFormControlElement email_hidden_attr_element =
      GetFormControlElementById("email_hidden_attr");
  blink::WebFormControlElement email_visible_element =
      GetFormControlElementById("email_visible");
  blink::WebFormControlElement email2_element =
      GetFormControlElementById("email2");

  base::test::TestFuture<const std::optional<std::string>&> future1;
  autofill_agent().GetNonceForEmailVerification(
      form_util::GetFieldRendererId(email_element), future1.GetCallback());
  EXPECT_EQ(future1.Get(), "test_nonce_123");

  base::test::TestFuture<const std::optional<std::string>&> future_hidden_attr;
  autofill_agent().GetNonceForEmailVerification(
      form_util::GetFieldRendererId(email_hidden_attr_element),
      future_hidden_attr.GetCallback());
  EXPECT_EQ(future_hidden_attr.Get(), "test_nonce_456");

  base::test::TestFuture<const std::optional<std::string>&> future_visible;
  autofill_agent().GetNonceForEmailVerification(
      form_util::GetFieldRendererId(email_visible_element),
      future_visible.GetCallback());
  EXPECT_EQ(future_visible.Get(), std::nullopt);

  base::test::TestFuture<const std::optional<std::string>&> future2;
  autofill_agent().GetNonceForEmailVerification(
      form_util::GetFieldRendererId(email2_element), future2.GetCallback());
  EXPECT_EQ(future2.Get(), std::nullopt);
}

// Tests that the verification token is injected into an input field using the
// boolean `hidden` attribute instead of type="hidden".
TEST_F(EmailVerificationHandlerTest,
       EmailVerificationHandlerSharesTokenWithHiddenAttributeField) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body>
    <form id="form">
      <input type="email" id="email" value="a@example.com">
      <input id="verification" autocomplete="email-verification-token" hidden>
    </form>
  </body>)");
  WaitForFormsSeen();

  blink::WebFormElement form_element =
      GetWebElementById("form").DynamicTo<blink::WebFormElement>();
  blink::WebFormControlElement email_element =
      GetFormControlElementById("email");
  blink::WebFormControlElement verification_element =
      GetFormControlElementById("verification");

  EXPECT_CALL(autofill_driver(),
              FormWithEmailVerificationTokenSubmitted(
                  _, form_util::GetFieldRendererId(email_element)));

  autofill_agent().SendEmailVerificationToken(
      form_util::GetFieldRendererId(email_element), "a@example.com",
      "evt_token_456");

  test_api(autofill_agent())
      .email_verification_handler()
      .WillSendSubmitEvent(form_element);

  EXPECT_EQ(verification_element.Value().Utf16(), u"evt_token_456");
}

}  // namespace

}  // namespace autofill
