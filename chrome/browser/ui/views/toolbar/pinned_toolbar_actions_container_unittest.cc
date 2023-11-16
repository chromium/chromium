// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/views/frame/browser_actions.h"

#include <vector>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

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
  GetChildToolbarButtons() {
    std::vector<PinnedToolbarActionsContainer::PinnedActionToolbarButton*>
        result;
    for (views::View* child : browser_view()
                                  ->toolbar()
                                  ->pinned_toolbar_actions_container()
                                  ->children()) {
      if (views::Button::AsButton(child)) {
        PinnedToolbarActionsContainer::PinnedActionToolbarButton* button =
            static_cast<
                PinnedToolbarActionsContainer::PinnedActionToolbarButton*>(
                child);
        result.push_back(button);
      }
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
  auto pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Verify pinning an action adds a button.
  model->UpdatePinnedState(actions::kActionCut, true);
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  // Check the context menu
  EXPECT_EQ(
      pinned_buttons[0]->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN));
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
  pinned_buttons = GetChildToolbarButtons();
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
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify activating a button does not pin and adds to popped out buttons.
  container->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  // Check the context menu
  EXPECT_EQ(
      toolbar_buttons[0]->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN));
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Verify deactivating a button removes it from popped out buttons.
  container->UpdateActionState(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
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
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify activating a button does not pin and adds to popped out buttons.
  container->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Pin active button and verify state.
  model->UpdatePinnedState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Unpin active button and verify state.
  model->UpdatePinnedState(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
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
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Pin both and verify order matches the order they were added.
  model->UpdatePinnedState(actions::kActionCut, true);
  model->UpdatePinnedState(actions::kActionCopy, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);
  // Make kActionCut popped out instead of pinned and verify order.
  container->UpdateActionState(actions::kActionCut, true);
  model->UpdatePinnedState(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCut);
}

TEST_F(PinnedToolbarActionsContainerTest, DividerVisibleWhileButtonPoppedOut) {
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

  browser_action_item->AddChild(std::move(cut_action));

  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);
  auto* container =
      browser_view()->toolbar()->pinned_toolbar_actions_container();
  // Verify there are no child views visible. Note the divider still exists but
  // should not be visible.
  auto child_views =
      browser_view()->toolbar()->pinned_toolbar_actions_container()->children();
  ASSERT_EQ(child_views.size(), 1u);
  ASSERT_FALSE(child_views[0]->GetVisible());
  // Make kActionCut popped out and verify order and visibility of the divider.
  container->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  child_views =
      browser_view()->toolbar()->pinned_toolbar_actions_container()->children();
  ASSERT_EQ(child_views.size(), 2u);
  ASSERT_EQ(
      static_cast<PinnedToolbarActionsContainer::PinnedActionToolbarButton*>(
          child_views[0])
          ->GetActionId(),
      actions::kActionCut);
  ASSERT_EQ(child_views[1]->GetProperty(views::kElementIdentifierKey),
            kPinnedToolbarActionsContainerDividerElementId);
  ASSERT_FALSE(child_views[1]->GetVisible());
  // Pin kActionCut and verify the pinned button is there and the divider is
  // visible.
  model->UpdatePinnedState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, true);
  child_views =
      browser_view()->toolbar()->pinned_toolbar_actions_container()->children();
  ASSERT_EQ(child_views.size(), 2u);
  ASSERT_EQ(
      static_cast<PinnedToolbarActionsContainer::PinnedActionToolbarButton*>(
          child_views[0])
          ->GetActionId(),
      actions::kActionCut);
  ASSERT_EQ(child_views[1]->GetProperty(views::kElementIdentifierKey),
            kPinnedToolbarActionsContainerDividerElementId);
  ASSERT_TRUE(child_views[1]->GetVisible());
}

TEST_F(PinnedToolbarActionsContainerTest, MovingActionsUpdateOrder) {
  actions::ActionItem* browser_action_item =
      BrowserActions::FromBrowser(browser_view()->browser())
          ->root_action_item();
  auto cut_action =
      actions::ActionItem::Builder()
          .SetText(u"Test Action")
          .SetTooltipText(u"Test Action")
          .SetActionId(actions::kActionCut)
          .SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon))
          .SetVisible(true)
          .SetEnabled(true)
          .SetInvokeActionCallback(base::DoNothing())
          .Build();
  auto copy_action =
      actions::ActionItem::Builder()
          .SetText(u"Test Action")
          .SetTooltipText(u"Test Action")
          .SetActionId(actions::kActionCopy)
          .SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon))
          .SetVisible(true)
          .SetEnabled(true)
          .SetInvokeActionCallback(base::DoNothing())
          .Build();

  browser_action_item->AddChild(std::move(cut_action));
  browser_action_item->AddChild(std::move(copy_action));

  auto* model = PinnedToolbarActionsModel::Get(profile());
  auto* container =
      browser_view()->toolbar()->pinned_toolbar_actions_container();
  ASSERT_TRUE(model);
  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Pin both and verify order matches the order they were added.
  model->UpdatePinnedState(actions::kActionCut, true);
  model->UpdatePinnedState(actions::kActionCopy, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);
  // Drag to reorder the two actions.
  auto* drag_view = toolbar_buttons[1];
  EXPECT_TRUE(
      container->CanStartDragForView(drag_view, gfx::Point(), gfx::Point()));
  ui::OSExchangeData drag_data;
  container->WriteDragDataForView(drag_view, gfx::Point(), &drag_data);
  gfx::Point drag_location = toolbar_buttons[0]->bounds().CenterPoint();
  ui::DropTargetEvent drop_event(drag_data, gfx::PointF(drag_location),
                                 gfx::PointF(drag_location),
                                 ui::DragDropTypes::DRAG_MOVE);
  container->OnDragUpdated(drop_event);
  auto drop_cb = container->GetDropCallback(drop_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(drop_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/1045212): we avoid using animations on Mac due to the lack
  // of support in unit tests. Therefore this is a no-op.
#else
  views::test::WaitForAnimatingLayoutManager(container);
#endif
  // Verify the order gets updated in the ui.
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCut);
}

TEST_F(PinnedToolbarActionsContainerTest, ContextMenuTest) {
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
  auto* container =
      browser_view()->toolbar()->pinned_toolbar_actions_container();

  ASSERT_TRUE(model);
  // Verify there are no pinned buttons.
  auto pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Verify pinning an action adds a button.
  model->UpdatePinnedState(actions::kActionCut, true);
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  // Check the context menu. Callback should unpin the button.
  EXPECT_EQ(
      pinned_buttons[0]->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN));
  pinned_buttons[0]->ExecuteCommand(IDC_UPDATE_SIDE_PANEL_PIN_STATE, 0);
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Callback for pop out button should pin the action.
  container->UpdateActionState(actions::kActionCut, true);
  auto child_views =
      browser_view()->toolbar()->pinned_toolbar_actions_container()->children();
  auto* pop_out_button =
      static_cast<PinnedToolbarActionsContainer::PinnedActionToolbarButton*>(
          child_views[0]);
  EXPECT_EQ(
      pop_out_button->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN));
  pop_out_button->ExecuteCommand(IDC_UPDATE_SIDE_PANEL_PIN_STATE, 0);
  CheckIsPinned(actions::kActionCut, true);
}
