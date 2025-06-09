// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_CONTROLLER_H_

#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/action_id.h"

namespace page_actions {

class MockPageActionController : public PageActionController {
 public:
  MockPageActionController();
  ~MockPageActionController() override;

  MOCK_METHOD(void, Show, (actions::ActionId), (override));
  MOCK_METHOD(void, Hide, (actions::ActionId), (override));
  void ShowSuggestionChip(actions::ActionId action_id) override {
    ShowSuggestionChip(action_id, SuggestionChipConfig());
  }
  MOCK_METHOD(void,
              ShowSuggestionChip,
              (actions::ActionId, SuggestionChipConfig),
              (override));
  MOCK_METHOD(void, HideSuggestionChip, (actions::ActionId), (override));
  MOCK_METHOD(void,
              OverrideText,
              (actions::ActionId, const std::u16string&),
              (override));
  MOCK_METHOD(void, ClearOverrideText, (actions::ActionId), (override));
  MOCK_METHOD(void,
              OverrideAccessibleName,
              (actions::ActionId, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              ClearOverrideAccessibleName,
              (actions::ActionId),
              (override));
  MOCK_METHOD(void,
              OverrideImage,
              (actions::ActionId, const ui::ImageModel&),
              (override));
  MOCK_METHOD(void, ClearOverrideImage, (actions::ActionId), (override));
  MOCK_METHOD(void,
              OverrideTooltip,
              (actions::ActionId, const std::u16string&),
              (override));
  MOCK_METHOD(void, ClearOverrideTooltip, (actions::ActionId), (override));
  MOCK_METHOD(void,
              AddObserver,
              (actions::ActionId,
               (base::ScopedObservation<PageActionModelInterface,
                                        PageActionModelObserver>&)),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              CreateActionItemSubscription,
              (actions::ActionItem*),
              (override));
  MOCK_METHOD(void, SetShouldHidePageActions, (bool), (override));
  MOCK_METHOD(base::RepeatingCallback<void(PageActionTrigger)>,
              GetClickCallback,
              (actions::ActionId),
              (override));
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_CONTROLLER_H_
