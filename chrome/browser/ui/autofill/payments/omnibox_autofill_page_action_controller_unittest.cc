// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"

#include <memory>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using ::testing::_;
using ::testing::InSequence;

namespace autofill {

class OmniboxAutofillPageActionControllerTest : public testing::Test {
 protected:
  OmniboxAutofillPageActionControllerTest() {
    ON_CALL(tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));
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

 private:
  tabs::MockTabInterface tab_interface_;
  ui::UnownedUserDataHost user_data_host_;
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
