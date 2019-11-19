// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/ios/security_state_utils.h"

#include "components/security_state/core/security_state.h"
#import "components/security_state/ios/insecure_input_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/web_test_with_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This test fixture creates an IOSSecurityStateTabHelper and an
// InsecureInputTabHelper for the WebState, then loads a non-secure
// HTML document.
class SecurityStateUtilsTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    InsecureInputTabHelper::CreateForWebState(web_state());
    insecure_input_ = InsecureInputTabHelper::FromWebState(web_state());
    ASSERT_TRUE(insecure_input_);

    LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  }

  // Returns the InsecureInputEventData for current WebState().
  security_state::InsecureInputEventData GetInsecureInputEventData() const {
    return security_state::GetVisibleSecurityStateForWebState(web_state())
        ->insecure_input_events;
  }

  InsecureInputTabHelper* insecure_input() { return insecure_input_; }

 private:
  InsecureInputTabHelper* insecure_input_;
};

// Ensures that |insecure_field_edited| is set only when an editing event has
// been reported.
TEST_F(SecurityStateUtilsTest, SecurityInfoAfterEditing) {
  // Verify |insecure_field_edited| is not set prematurely.
  security_state::InsecureInputEventData events = GetInsecureInputEventData();
  EXPECT_FALSE(events.insecure_field_edited);

  // Simulate an edit and verify |insecure_field_edited| is noted in the
  // insecure_input_events.
  insecure_input()->DidEditFieldInInsecureContext();
  events = GetInsecureInputEventData();
  EXPECT_TRUE(events.insecure_field_edited);
}

// Ensures that re-navigating to the same page does not keep
// |insecure_field_set| set.
TEST_F(SecurityStateUtilsTest, InsecureInputClearedOnRenavigation) {
  // Simulate an edit and verify |insecure_field_edited| is noted in the
  // insecure_input_events.
  insecure_input()->DidEditFieldInInsecureContext();
  security_state::InsecureInputEventData events = GetInsecureInputEventData();
  EXPECT_TRUE(events.insecure_field_edited);

  // Navigate to the same page again.
  LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  events = GetInsecureInputEventData();
  EXPECT_FALSE(events.insecure_field_edited);
}
