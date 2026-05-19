// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"

#include <memory>
#include <optional>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnRef;

namespace autofill {

class OmniboxAutofillPageActionControllerTest : public testing::Test {
 protected:
  OmniboxAutofillPageActionControllerTest() {
    ON_CALL(tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_interface_));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    user_education_.emplace(&mock_browser_window_interface_);
    omnibox_autofill_page_action_controller_ =
        std::make_unique<OmniboxAutofillPageActionController>(
            tab_interface_, page_action_controller_);
  }

  OmniboxAutofillPageActionControllerTest(
      const OmniboxAutofillPageActionControllerTest&) = delete;
  OmniboxAutofillPageActionControllerTest& operator=(
      const OmniboxAutofillPageActionControllerTest&) = delete;

  ~OmniboxAutofillPageActionControllerTest() override = default;

  tabs::MockTabInterface& tab() { return tab_interface_; }

  page_actions::MockPageActionController& page_action_controller() {
    return page_action_controller_;
  }

  OmniboxAutofillPageActionController&
  omnibox_autofill_page_action_controller() {
    return *omnibox_autofill_page_action_controller_;
  }

  MockBrowserUserEducationInterface& user_education() {
    return user_education_.value();
  }

 private:
  MockBrowserWindowInterface mock_browser_window_interface_;
  tabs::MockTabInterface tab_interface_;
  ui::UnownedUserDataHost user_data_host_;
  std::optional<MockBrowserUserEducationInterface> user_education_;
  page_actions::MockPageActionController page_action_controller_;
  std::unique_ptr<OmniboxAutofillPageActionController>
      omnibox_autofill_page_action_controller_;
};

TEST_F(OmniboxAutofillPageActionControllerTest, FromReturnsController) {
  EXPECT_EQ(&omnibox_autofill_page_action_controller(),
            OmniboxAutofillPageActionController::From(tab()));
}

TEST_F(OmniboxAutofillPageActionControllerTest, ShowCallsPageActionController) {
  InSequence s;
  EXPECT_CALL(page_action_controller(), Show(kActionAutofillPayment)).Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionAutofillPayment, _))
      .Times(1);
  EXPECT_CALL(user_education(), MaybeShowFeaturePromo(_))
      .WillOnce(Return(true));

  omnibox_autofill_page_action_controller().Show();
}

TEST_F(OmniboxAutofillPageActionControllerTest, HideCallsPageActionController) {
  InSequence s;
  EXPECT_CALL(page_action_controller(),
              HideSuggestionChip(kActionAutofillPayment))
      .Times(1);
  EXPECT_CALL(page_action_controller(), Hide(kActionAutofillPayment)).Times(1);

  omnibox_autofill_page_action_controller().Hide();
}

}  // namespace autofill
