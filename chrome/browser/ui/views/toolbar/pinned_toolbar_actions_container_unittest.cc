// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/views/frame/browser_actions.h"

#include <vector>

#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/events/base_event_utils.h"

class PinnedToolbarActionsContainerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSidePanelPinning);
    TestWithBrowserView::SetUp();
    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        PinnedToolbarActionsModelFactory::GetInstance(),
        base::BindRepeating(&PinnedToolbarActionsContainerTest::
                                BuildPinnedToolbarActionsModel));
    return factories;
  }

  static std::unique_ptr<KeyedService> BuildPinnedToolbarActionsModel(
      content::BrowserContext* context) {
    return std::make_unique<PinnedToolbarActionsModel>(
        Profile::FromBrowserContext(context));
  }

  std::vector<ToolbarButton*> GetPinnedEntryButtons() {
    std::vector<ToolbarButton*> result;
    for (views::View* child : browser_view()
                                  ->toolbar()
                                  ->pinned_toolbar_actions_container()
                                  ->children()) {
      ToolbarButton* button = static_cast<ToolbarButton*>(child);
      result.push_back(button);
    }
    return result;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PinnedToolbarActionsContainerTest, PinningAndUnpinning) {
  const std::u16string kActionTooltipText = u"Test Action";
  actions::ActionItem* browser_action_item =
      BrowserActions::FromBrowser(browser_view()->browser())
          ->root_action_item();
  auto action_item = actions::ActionItem::Builder()
                         .SetText(u"Test Action")
                         .SetTooltipText(kActionTooltipText)
                         .SetActionId(actions::kActionCut)
                         .SetVisible(true)
                         .SetEnabled(true)
                         .SetInvokeActionCallback(base::DoNothing())
                         .Build();
  // clang-format on
  browser_action_item->AddChild(std::move(action_item));

  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);
  // Verify there are no pinned buttons.
  auto pinned_buttons = GetPinnedEntryButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Verify pinning an action adds a button.
  model->UpdatePinnedState(actions::kActionCut, true);
  pinned_buttons = GetPinnedEntryButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  // Verify pressing the toolbar button invokes the action.
  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(actions::kActionCut)
                ->GetInvokeCount(),
            0);
  pinned_buttons[0]->button_controller()->NotifyClick();
  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(actions::kActionCut)
                ->GetInvokeCount(),
            1);
  // Verify unpinning an action removes a button.
  model->UpdatePinnedState(actions::kActionCut, false);
  pinned_buttons = GetPinnedEntryButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
}
