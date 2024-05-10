// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/password_autofill_agent.h"

#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/mock_password_autofill_agent_delegate.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace autofill {

using PasswordAutofillAgentTest = PlatformTest;

// Tests that DidFillField() correctly calls the delegate.
TEST_F(PasswordAutofillAgentTest, DidFillField) {
  GURL url("https://example.com");
  auto frame = web::FakeWebFrame::Create("frameID", true, url);
  autofill::FormRendererId form_id(1);
  autofill::FieldRendererId field_id(2);
  const std::u16string value(u"value");

  MockPasswordAutofillAgentDelegate delegate_mock;
  EXPECT_CALL(
      delegate_mock,
      DidFillField(frame.get(), std::make_optional<FormRendererId>(form_id),
                   field_id, value));
  web::FakeWebState fake_web_state;
  PasswordAutofillAgent::CreateForWebState(&fake_web_state, &delegate_mock);
  PasswordAutofillAgent::FromWebState(&fake_web_state)
      ->DidFillField(frame.get(), form_id, field_id, value);
}

}  // namespace autofill
