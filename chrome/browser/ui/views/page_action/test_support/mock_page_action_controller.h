// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_CONTROLLER_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
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
  MOCK_METHOD(void,
              OverrideImage,
              (actions::ActionId, const ui::ImageModel&, PageActionColorSource),
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
  MOCK_METHOD(ScopedPageActionActivity,
              AddActivity,
              (actions::ActionId),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              CreateActionItemSubscription,
              (actions::ActionItem*),
              (override));
  MOCK_METHOD(void, SetShouldHidePageActions, (bool), (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterOnWillDestroyCallback,
              (base::OnceCallback<void(PageActionController&)>),
              (override));
  MOCK_METHOD(base::RepeatingCallback<void(PageActionTrigger)>,
              GetClickCallback,
              (base::PassKey<PageActionView>, actions::ActionId),
              (override));
  MOCK_METHOD(void,
              RegisterIsChipShowingChangedCallback,
              (base::PassKey<PageActionView>,
               actions::ActionId,
               PageActionView*),
              (override));
  MOCK_METHOD(void, DecrementActivityCounter, (actions::ActionId), (override));

 private:
  MockPageActionModel model_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_CONTROLLER_H_
