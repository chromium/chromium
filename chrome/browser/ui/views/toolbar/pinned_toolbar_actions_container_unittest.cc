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

  std::vector<PinnedToolbarActionsContainer::PinnedActionToolbarButton*>
  GetToolbarButtons() {
    std::vector<PinnedToolbarActionsContainer::PinnedActionToolbarButton*>
        result;
    for (views::View* child : browser_view()
                                  ->toolbar()
                                  ->pinned_toolbar_actions_container()
                                  ->children()) {
      PinnedToolbarActionsContainer::PinnedActionToolbarButton* button =
          static_cast<
              PinnedToolbarActionsContainer::PinnedActionToolbarButton*>(child);
      result.push_back(button);
    }
    return result;
  }

  void CheckIsPoppedOut(actions::ActionId id, bool should_be_popped_out) {
    auto* container =
        browser_view()->toolbar()->pinned_toolbar_actions_container();
    if (should_be_popped_out) {
      ASSERT_NE(base::ranges::find(
                    container->popped_out_buttons_, id,
                    [](auto* button) { return button->GetActionId(); }),
                container->popped_out_buttons_.end());
    } else {
      ASSERT_EQ(base::ranges::find(
                    container->popped_out_buttons_, id,
                    [](auto* button) { return button->GetActionId(); }),
                container->popped_out_buttons_.end());
    }
  }

  void CheckIsPinned(actions::ActionId id, bool should_be_pinned) {
    auto* container =
        browser_view()->toolbar()->pinned_toolbar_actions_container();
    if (should_be_pinned) {
      ASSERT_NE(base::ranges::find(
                    container->pinned_buttons_, id,
                    [](auto* button) { return button->GetActionId(); }),
                container->pinned_buttons_.end());
    } else {
      ASSERT_EQ(base::ranges::find(
                    container->pinned_buttons_, id,
                    [](auto* button) { return button->GetActionId(); }),
                container->pinned_buttons_.end());
    }
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
  auto pinned_buttons = GetToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Verify pinning an action adds a button.
  model->UpdatePinnedState(actions::kActionCut, true);
  pinned_buttons = GetToolbarButtons();
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
  pinned_buttons = GetToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
}

TEST_F(PinnedToolbarActionsContainerTest,
       UnpinnedToolbarButtonsPoppedOutWhileActive) {
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

  browser_action_item->AddChild(std::move(action_item));

  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);
  auto* container =
      browser_view()->toolbar()->pinned_toolbar_actions_container();
  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify activating a button does not pin and adds to popped out buttons.
  container->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Verify deactivating a button removes it from popped out buttons.
  container->UpdateActionState(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
}

TEST_F(PinnedToolbarActionsContainerTest,
       StateChangesBetweenPinnedandUnpinnedWhileActive) {
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

  browser_action_item->AddChild(std::move(action_item));

  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);
  auto* container =
      browser_view()->toolbar()->pinned_toolbar_actions_container();
  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify activating a button does not pin and adds to popped out buttons.
  container->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Pin active button and verify state.
  model->UpdatePinnedState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, true);
  toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Unpin active button and verify state.
  model->UpdatePinnedState(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
}

TEST_F(PinnedToolbarActionsContainerTest, PoppedOutButtonsAreAfterPinned) {
  const std::u16string kActionTooltipText = u"Test Action";
  actions::ActionItem* browser_action_item =
      BrowserActions::FromBrowser(browser_view()->browser())
          ->root_action_item();
  auto cut_action = actions::ActionItem::Builder()
                        .SetText(u"Test Action")
                        .SetTooltipText(kActionTooltipText)
                        .SetActionId(actions::kActionCut)
                        .SetVisible(true)
                        .SetEnabled(true)
                        .SetInvokeActionCallback(base::DoNothing())
                        .Build();
  auto copy_action = actions::ActionItem::Builder()
                         .SetText(u"Test Action")
                         .SetTooltipText(kActionTooltipText)
                         .SetActionId(actions::kActionCopy)
                         .SetVisible(true)
                         .SetEnabled(true)
                         .SetInvokeActionCallback(base::DoNothing())
                         .Build();

  browser_action_item->AddChild(std::move(cut_action));
  browser_action_item->AddChild(std::move(copy_action));

  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);
  auto* container =
      browser_view()->toolbar()->pinned_toolbar_actions_container();
  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Pin both and verify order matches the order they were added.
  model->UpdatePinnedState(actions::kActionCut, true);
  model->UpdatePinnedState(actions::kActionCopy, true);
  toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);
  // Make kActionCut popped out instead of pinned and verify order.
  container->UpdateActionState(actions::kActionCut, true);
  model->UpdatePinnedState(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCut);
}
