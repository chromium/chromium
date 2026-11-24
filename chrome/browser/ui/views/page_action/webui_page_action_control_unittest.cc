// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_view.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/browser/ui/webui/webui_toolbar/icon_table.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"
#include "components/browser_apis/ui_controllers/toolbar/icon_handle.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/animation/widget_fade_animator.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace page_actions {

namespace {

using testing::_;
using testing::NiceMock;

class TestBrowserElements : public BrowserElements {
 public:
  DECLARE_SAFE_CAST_TARGET()
  TestBrowserElements(BrowserWindowInterface& browser,
                      ui::ElementContext context)
      : BrowserElements(browser), context_(context) {}
  ui::ElementContext GetContext() override { return context_; }

 private:
  ui::ElementContext context_;
};

DEFINE_SAFE_CAST_TARGET(TestBrowserElements)

class MockPageActionModelObserver
    : public page_actions::PageActionModelObserver {
 public:
  MOCK_METHOD(void,
              OnPageActionModelChanged,
              (const page_actions::PageActionModelInterface&),
              (override));
  MOCK_METHOD(void,
              OnPageActionModelWillBeDeleted,
              (const page_actions::PageActionModelInterface&),
              (override));
};

class WebUIPageActionControlTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUIPageActionControlTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Create mock tab.
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));
    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_tab_, GetProfile()).WillByDefault(testing::Return(profile()));
    ON_CALL(*mock_tab_, RegisterDidActivate(_))
        .WillByDefault([this](tabs::TabInterface::DidActivateCallback cb) {
          return did_activate_callbacks_.Add(std::move(cb));
        });
    ON_CALL(*mock_tab_, RegisterWillDeactivate(_))
        .WillByDefault([](tabs::TabInterface::WillDeactivateCallback cb) {
          return base::CallbackListSubscription();
        });
    ON_CALL(*mock_tab_, IsActivated()).WillByDefault(testing::Return(true));

    // Register tab lookup.
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         mock_tab_.get());

    // Create TabFeatures.
    tab_features_ = std::make_unique<tabs::TabFeatures>();
    ON_CALL(*mock_tab_, GetTabFeatures())
        .WillByDefault(testing::Return(tab_features_.get()));

    // Instantiate and inject PageActionController.
    auto* pinned_actions_model = PinnedToolbarActionsModel::Get(profile());
    page_action_controller_ =
        std::make_unique<page_actions::PageActionControllerImpl>(
            *mock_tab_,
            std::vector<actions::ActionId>(page_actions::kActionIds.begin(),
                                           page_actions::kActionIds.end()),
            page_actions::PageActionPropertiesProvider(), pinned_actions_model);

    did_activate_callbacks_.Notify(mock_tab_.get());

    // Create dummy ActionItem tree.
    root_action_item_ =
        actions::ActionItem::Builder()
            .AddChild(actions::ActionItem::Builder()
                          .SetActionId(kActionAiMode)
                          .SetText(u"AI Mode"))
            .AddChild(actions::ActionItem::Builder()
                          .SetActionId(kActionShowTranslate)
                          .SetText(u"Translate"))
            .AddChild(actions::ActionItem::Builder()
                          .SetActionId(kActionSidePanelShowLensOverlayResults)
                          .SetText(u"Lens"))
            .Build();

    control_ =
        std::make_unique<WebUIPageActionControl>(root_action_item_.get());
    control_->Init(&webui_delegate_);
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    control_.reset();
    page_action_controller_.reset();
    tab_features_.reset();
    DeleteContents();
    mock_tab_.reset();
    root_action_item_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  struct TabContext {
    std::unique_ptr<content::WebContents> web_contents;
    ui::UnownedUserDataHost user_data_host;
    tabs::MockTabInterface mock_tab;
    tabs::TabFeatures tab_features;
    std::unique_ptr<page_actions::PageActionControllerImpl> controller;
  };

  std::unique_ptr<TabContext> CreateTestTabContext() {
    auto context = std::make_unique<TabContext>();
    context->web_contents = CreateTestWebContents();
    ON_CALL(context->mock_tab, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(context->user_data_host));
    ON_CALL(context->mock_tab, GetContents())
        .WillByDefault(testing::Return(context->web_contents.get()));
    ON_CALL(context->mock_tab, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(context->mock_tab, RegisterDidActivate(_))
        .WillByDefault([](tabs::TabInterface::DidActivateCallback) {
          return base::CallbackListSubscription();
        });
    ON_CALL(context->mock_tab, RegisterWillDeactivate(_))
        .WillByDefault([](tabs::TabInterface::WillDeactivateCallback) {
          return base::CallbackListSubscription();
        });
    ON_CALL(context->mock_tab, IsActivated())
        .WillByDefault(testing::Return(true));

    tabs::TabLookupFromWebContents::CreateForWebContents(
        context->web_contents.get(), &context->mock_tab);
    ON_CALL(context->mock_tab, GetTabFeatures())
        .WillByDefault(testing::Return(&context->tab_features));

    auto* pinned_actions_model = PinnedToolbarActionsModel::Get(profile());
    context->controller =
        std::make_unique<page_actions::PageActionControllerImpl>(
            context->mock_tab,
            std::vector<actions::ActionId>(page_actions::kActionIds.begin(),
                                           page_actions::kActionIds.end()),
            page_actions::PageActionPropertiesProvider(), pinned_actions_model);

    return context;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  NiceMock<MockWebUIToolbarControlDelegate> webui_delegate_;
  std::unique_ptr<actions::ActionItem> root_action_item_;
  base::RepeatingCallbackList<void(tabs::TabInterface*)>
      did_activate_callbacks_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<tabs::TabFeatures> tab_features_;
  std::unique_ptr<WebUIPageActionControl> control_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<page_actions::PageActionControllerImpl>
      page_action_controller_;
};

TEST_F(WebUIPageActionControlTest, InitialStateEmpty) {
  auto states = control_->GetPageActionStates();
  EXPECT_TRUE(states.empty());
}

TEST_F(WebUIPageActionControlTest, StateMapping) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab);
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  ASSERT_TRUE(controller);

  actions::ActionId target_action_id = kActionAiMode;
  toolbar_ui_api::mojom::PageActionId target_mojom_id =
      toolbar_ui_api::mojom::PageActionId::kActionAiMode;

  EXPECT_TRUE(control_->GetPageActionStates().empty());

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  controller->Show(target_action_id);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(target_mojom_id, states[0]->page_action_id);
  ASSERT_TRUE(states[0]->identifier);
  EXPECT_EQ("kAiModePageActionIconElementId",
            states[0]->identifier->native_identifier);

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  controller->OverrideTooltip(target_action_id, u"New Tooltip");

  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(u"New Tooltip", states[0]->tooltip_text);

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  controller->OverrideImage(target_action_id,
                            ui::ImageModel::FromVectorIcon(kZoomInIcon));

  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_FALSE(states[0]->icon.is_null());
  EXPECT_FALSE(states[0]->is_active);

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  std::optional<page_actions::ScopedPageActionActivity> activity =
      controller->AddActivity(target_action_id);
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_TRUE(states[0]->is_active);

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  activity.reset();
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_FALSE(states[0]->is_active);

  controller->OverrideText(target_action_id, u"Chip Text");
  controller->ShowSuggestionChip(target_action_id);

  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_TRUE(states[0]->should_show_chip);
  EXPECT_EQ(u"Chip Text", states[0]->text);

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  controller->Hide(target_action_id);

  states = control_->GetPageActionStates();
  EXPECT_TRUE(states.empty());
}

TEST_F(WebUIPageActionControlTest, ClickForwarding) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);

  actions::ActionId target_action_id = kActionAiMode;
  toolbar_ui_api::mojom::PageActionId target_mojom_id =
      toolbar_ui_api::mojom::PageActionId::kActionAiMode;

  controller->Show(target_action_id);

  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      target_action_id, root_action_item_.get());
  ASSERT_TRUE(action_item);

  bool action_invoked = false;
  action_item->SetInvokeActionCallback(base::BindRepeating(
      [](bool* invoked, actions::ActionItem*,
         actions::ActionInvocationContext) { *invoked = true; },
      &action_invoked));

  base::RunLoop run_loop;
  control_->OnPageActionClick(
      target_mojom_id, page_actions::PageActionTrigger::kMouse,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
                 result) {
            EXPECT_OK(result);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  EXPECT_TRUE(action_invoked);
}

TEST_F(WebUIPageActionControlTest, ChipShowingChangedForwarding) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);

  actions::ActionId target_action_id = kActionAiMode;
  toolbar_ui_api::mojom::PageActionId target_mojom_id =
      toolbar_ui_api::mojom::PageActionId::kActionAiMode;

  controller->Show(target_action_id);
  controller->ShowSuggestionChip(target_action_id);

  MockPageActionModelObserver observer;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation(&observer);
  controller->AddObserver(target_action_id, observation);

  // Before WebUI notifies chip showing changed, IsChipShowing is false.
  const auto* model = control_->GetObservedModel(target_action_id);
  ASSERT_TRUE(model);
  EXPECT_FALSE(model->IsChipShowing());

  EXPECT_CALL(observer, OnPageActionModelChanged(_))
      .WillOnce([](const page_actions::PageActionModelInterface& model) {
        EXPECT_TRUE(model.IsChipShowing());
      });

  base::RunLoop run_loop;
  control_->OnPageActionChipShowingChanged(
      target_mojom_id, /*is_showing=*/true,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
                 result) {
            EXPECT_OK(result);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  EXPECT_TRUE(model->IsChipShowing());

  // Verify the expectation for showing was satisfied and clear before
  // testing hide.
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Hiding the chip updates ShouldShowSuggestionChip(), but IsChipShowing()
  // remains true until WebUI completes the transition and reports
  // OnPageActionChipShowingChanged().
  controller->HideSuggestionChip(target_action_id);
  EXPECT_TRUE(model->IsChipShowing());

  EXPECT_CALL(observer, OnPageActionModelChanged(_))
      .WillOnce([](const page_actions::PageActionModelInterface& model) {
        EXPECT_FALSE(model.IsChipShowing());
      });

  base::RunLoop hide_run_loop;
  control_->OnPageActionChipShowingChanged(
      target_mojom_id, /*is_showing=*/false,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
                 result) {
            EXPECT_OK(result);
            run_loop->Quit();
          },
          &hide_run_loop));
  hide_run_loop.Run();

  EXPECT_FALSE(model->IsChipShowing());
}

TEST_F(WebUIPageActionControlTest,
       SwitchingControllerDoesNotForceChipShowingStatus) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab1 =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab1);
  page_actions::PageActionController* controller1 =
      page_actions::PageActionController::From(tab1);
  ASSERT_TRUE(controller1);

  actions::ActionId target_action_id = kActionAiMode;
  toolbar_ui_api::mojom::PageActionId target_mojom_id =
      toolbar_ui_api::mojom::PageActionId::kActionAiMode;

  controller1->Show(target_action_id);
  controller1->ShowSuggestionChip(target_action_id);

  // Simulate WebUI notifying chip showing for tab 1.
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionChipShowingChanged(target_mojom_id,
                                             /*is_showing=*/true,
                                             future.GetCallback());
    EXPECT_OK(future.Get());
  }
  EXPECT_TRUE(control_->GetObservedModel(target_action_id)->IsChipShowing());

  // Create a second tab context where the action chip should also be shown,
  // but has not yet received confirmation from WebUI.
  auto tab2 = CreateTestTabContext();
  tab2->controller->Show(target_action_id);
  tab2->controller->ShowSuggestionChip(target_action_id);

  // Switch to tab 2. Switching controller should NOT force IsChipShowing on
  // the new controller's model.
  control_->UpdateController(tab2->web_contents.get());
  EXPECT_FALSE(control_->GetObservedModel(target_action_id)->IsChipShowing());

  // Once WebUI signals OnPageActionChipShowingChanged, the new controller's
  // model is updated.
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionChipShowingChanged(target_mojom_id,
                                             /*is_showing=*/true,
                                             future.GetCallback());
    EXPECT_OK(future.Get());
  }
  EXPECT_TRUE(control_->GetObservedModel(target_action_id)->IsChipShowing());
}

TEST_F(WebUIPageActionControlTest, SlideAndCrossfadeState) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);

  actions::ActionId target_action_id = kActionAiMode;
  controller->Show(target_action_id);
  controller->SetAnimationStyle(
      target_action_id,
      page_actions::PageActionAnimationStyle::kSlideAndCrossfade);
  controller->SetTrailingImage(
      target_action_id,
      ui::ImageModel::FromVectorIcon(vector_icons::kArrowForwardIcon));
  controller->SetShowTrailingIcon(target_action_id, true);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(states.size(), 1u);
  EXPECT_EQ(
      states[0]->animation_style,
      toolbar_ui_api::mojom::PageActionAnimationStyle::kSlideAndCrossfade);
  EXPECT_TRUE(states[0]->show_trailing_icon);
  EXPECT_TRUE(states[0]->trailing_icon.has_value());
}

TEST_F(WebUIPageActionControlTest, AccessibilityAnnouncement) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);

  actions::ActionId target_action_id = kActionAiMode;

  controller->OverrideText(target_action_id, u"Test Announcement");
  controller->Show(target_action_id);

  // Showing chip with should_announce_chip = true should trigger AnnounceAlert.
  EXPECT_CALL(webui_delegate_,
              AnnounceAlert(std::u16string(u"Test Announcement")))
      .Times(1);
  controller->ShowSuggestionChip(
      target_action_id,
      page_actions::SuggestionChipConfig{.should_announce_chip = true});

  // Calling ShowSuggestionChip again while already showing shouldn't trigger
  // AnnounceAlert again.
  EXPECT_CALL(webui_delegate_, AnnounceAlert(_)).Times(0);
  controller->ShowSuggestionChip(
      target_action_id,
      page_actions::SuggestionChipConfig{.should_announce_chip = true});

  // Hiding chip and showing again should trigger AnnounceAlert again.
  controller->HideSuggestionChip(target_action_id);
  EXPECT_CALL(webui_delegate_,
              AnnounceAlert(std::u16string(u"Test Announcement")))
      .Times(1);
  controller->ShowSuggestionChip(
      target_action_id,
      page_actions::SuggestionChipConfig{.should_announce_chip = true});
}

TEST_F(WebUIPageActionControlTest, TabDestroyedBeforeControl) {
  control_->UpdateController(web_contents());
  tab_features_.reset();
  control_.reset();
}

TEST_F(WebUIPageActionControlTest, UpdateControllerToNullResetsState) {
  control_->UpdateController(web_contents());

  actions::ActionId target_action_id = kActionAiMode;
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);

  controller->Show(target_action_id);
  EXPECT_FALSE(control_->GetPageActionStates().empty());

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  control_->UpdateController(nullptr);
  EXPECT_TRUE(control_->GetPageActionStates().empty());
  EXPECT_FALSE(control_->IsAnchoredMessageShowing(target_action_id));
}

// Only exercises the "state unchanged -> no notify" path: this harness has no
// ColorProvider (the mock delegate's GetView() returns null), so GetState()
// resolves no colors. Verifying that colors actually re-resolve on theme change
// would require a real Widget.
TEST_F(WebUIPageActionControlTest, OnThemeChangedOnlyNotifiesWhenStateChanges) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);

  controller->Show(kActionAiMode);
  ASSERT_EQ(1u, control_->GetPageActionStates().size());
  testing::Mock::VerifyAndClearExpectations(&webui_delegate_);

  // Without a ColorProvider in this harness, re-resolving yields an identical
  // state, so the WebUI must not be notified.
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_)).Times(0);
  control_->OnThemeChanged();
  ASSERT_EQ(1u, control_->GetPageActionStates().size());
}

TEST_F(WebUIPageActionControlTest, GetPageActionViewInterfaceAndMethods) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab);
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  ASSERT_TRUE(controller);

  actions::ActionId target_action_id = kActionAiMode;

  page_actions::PageActionViewInterface* view_interface =
      control_->GetPageActionViewInterface(target_action_id);
  ASSERT_TRUE(view_interface);

  controller->OverrideTooltip(target_action_id, u"Test Tooltip");
  controller->OverrideAccessibleName(target_action_id, u"Test Accessible Name");

  EXPECT_EQ(view_interface->GetTooltipText(), u"Test Tooltip");
  EXPECT_EQ(view_interface->GetAccessibleName(), u"Test Accessible Name");

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  view_interface->SetVisible(true);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(states.size(), 1u);

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  view_interface->SetVisible(false);
  EXPECT_TRUE(control_->GetPageActionStates().empty());
}

TEST_F(WebUIPageActionControlTest, GetBubbleAnchor) {
  constexpr ui::ElementContext kTestContext =
      ui::ElementContext::CreateFakeContextForTesting(1);
  NiceMock<MockBrowserWindowInterface> mock_browser;
  TestBrowserElements browser_elements(mock_browser, kTestContext);

  page_actions::PageActionViewInterface* ai_mode_view =
      control_->GetPageActionViewInterface(kActionAiMode);
  ASSERT_TRUE(ai_mode_view);

  // 1. When browser is null, BubbleAnchor is null.
  EXPECT_CALL(webui_delegate_, GetBrowser())
      .WillRepeatedly(testing::Return(nullptr));
  EXPECT_TRUE(ai_mode_view->GetBubbleAnchor().IsNull());

  // 2. When browser is present, but neither the page action element nor the
  // location bar element is tracked, BubbleAnchor is null.
  EXPECT_CALL(webui_delegate_, GetBrowser())
      .WillRepeatedly(testing::Return(&mock_browser));
  EXPECT_TRUE(ai_mode_view->GetBubbleAnchor().IsNull());

  // 3. When the specific page action element is not tracked, but the location
  // bar element is tracked, GetBubbleAnchor falls back to the location bar.
  ui::test::TestElement location_bar_element(kLocationBarElementId,
                                             kTestContext);
  location_bar_element.Show();

  views::BubbleAnchor fallback_anchor = ai_mode_view->GetBubbleAnchor();
  EXPECT_FALSE(fallback_anchor.IsNull());
  EXPECT_EQ(fallback_anchor.GetIfElement(), &location_bar_element);

  // 4. When the specific page action element is tracked, GetBubbleAnchor
  // anchors to that specific element instead of the location bar.
  ui::test::TestElement ai_mode_element(kAiModePageActionIconElementId,
                                        kTestContext);
  ai_mode_element.Show();

  views::BubbleAnchor specific_anchor = ai_mode_view->GetBubbleAnchor();
  EXPECT_FALSE(specific_anchor.IsNull());
  EXPECT_EQ(specific_anchor.GetIfElement(), &ai_mode_element);

  ai_mode_element.Hide();
  location_bar_element.Hide();
}

TEST_F(WebUIPageActionControlTest, AnchoredMessageState) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab);
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  ASSERT_TRUE(controller);

  actions::ActionId target_action_id = kActionAiMode;

  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .Times(testing::AtLeast(1));
  controller->Show(target_action_id);

  page_actions::AnchoredMessageConfig config{};
  controller->ShowAnchoredMessage(target_action_id, config);
  // Even though controller->ShowAnchoredMessage(...) is called just before this
  // line, EXPECT_FALSE(control_->IsAnchoredMessageShowing(target_action_id))
  // correctly expects the result to be false.
  //
  // This happens because WebUIPageActionControl requires a physical anchor
  // view or a visible UI element to attach the anchored message bubble to.
  // When CreateAndShowAnchoredMessage handles the model change, it attempts to
  // find this anchor.
  //
  // Since this is a unit test environment without an actual rendered WebUI or
  // View hierarchy, a valid anchor does not exist. The control falls back to
  // subscribing to ui::ElementTracker to wait for the UI element to become
  // visible on the screen. Because the bubble widget cannot be created
  // immediately, IsAnchoredMessageShowing() returns false.
  EXPECT_FALSE(control_->IsAnchoredMessageShowing(target_action_id));

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_FALSE(states[0]->should_show_chip);
  EXPECT_TRUE(states[0]->tooltip_text.empty());

  controller->OverrideTooltip(target_action_id, u"Tooltip Text");
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(states[0]->tooltip_text,
            l10n_util::GetStringFUTF16(IDS_PAGE_ACTION_ANCHORED_MESSAGE_SHOWING,
                                       u"Tooltip Text"));

  controller->HideAnchoredMessage(target_action_id);
  EXPECT_FALSE(control_->IsAnchoredMessageShowing(target_action_id));
}

TEST_F(WebUIPageActionControlTest,
       ClickWhileAnchoredMessageActiveDowngradesToChipWithoutInvokingAction) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab);
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  ASSERT_TRUE(controller);

  actions::ActionItem* translate_action_item =
      actions::ActionManager::Get().FindAction(kActionShowTranslate,
                                               root_action_item_.get());
  ASSERT_TRUE(translate_action_item);

  int translate_invoked_count = 0;
  translate_action_item->SetInvokeActionCallback(
      base::BindRepeating([](int* count, actions::ActionItem*,
                             actions::ActionInvocationContext) { ++(*count); },
                          &translate_invoked_count));

  controller->Show(kActionShowTranslate);
  controller->ShowAnchoredMessage(kActionShowTranslate,
                                  page_actions::AnchoredMessageConfig{});
  EXPECT_EQ(controller->GetActiveAnchoredMessage(), kActionShowTranslate);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_FALSE(states[0]->should_show_chip);

  // Clicking the page action while the anchored message is active should
  // downgrade it to a suggestion chip without invoking the underlying action.
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionClick(
        toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
        PageActionTrigger::kMouse, future.GetCallback());
    EXPECT_OK(future.Get());
  }

  EXPECT_EQ(controller->GetActiveAnchoredMessage(), std::nullopt);
  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  EXPECT_EQ(0, translate_invoked_count);

  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_TRUE(states[0]->should_show_chip);
}

TEST_F(WebUIPageActionControlTest, TabSwitchTokenUpdatesOnTabSwitch) {
  control_->UpdateController(web_contents());

  // Show an action so we can get states.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  controller->Show(kActionAiMode);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  const uint32_t initial_token = states[0]->tab_switch_token;

  // Update controller with the same web contents. Token should not change.
  control_->UpdateController(web_contents());
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(initial_token, states[0]->tab_switch_token);

  // Set up a second tab.
  std::unique_ptr<content::WebContents> web_contents2 = CreateTestWebContents();
  tabs::MockTabInterface mock_tab2;
  ui::UnownedUserDataHost user_data_host2;
  ON_CALL(mock_tab2, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host2));
  ON_CALL(mock_tab2, GetContents())
      .WillByDefault(testing::Return(web_contents2.get()));
  ON_CALL(mock_tab2, GetProfile()).WillByDefault(testing::Return(profile()));
  ON_CALL(mock_tab2, RegisterDidActivate(_))
      .WillByDefault([](tabs::TabInterface::DidActivateCallback cb) {
        return base::CallbackListSubscription();
      });
  ON_CALL(mock_tab2, RegisterWillDeactivate(_))
      .WillByDefault([](tabs::TabInterface::WillDeactivateCallback cb) {
        return base::CallbackListSubscription();
      });
  ON_CALL(mock_tab2, IsActivated()).WillByDefault(testing::Return(true));

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents2.get(),
                                                       &mock_tab2);

  auto tab_features2 = std::make_unique<tabs::TabFeatures>();
  ON_CALL(mock_tab2, GetTabFeatures())
      .WillByDefault(testing::Return(tab_features2.get()));

  auto page_action_controller2 =
      std::make_unique<page_actions::PageActionControllerImpl>(
          mock_tab2,
          std::vector<actions::ActionId>(page_actions::kActionIds.begin(),
                                         page_actions::kActionIds.end()),
          page_actions::PageActionPropertiesProvider(),
          PinnedToolbarActionsModel::Get(profile()));

  // Show action on second tab too.
  page_action_controller2->Show(kActionAiMode);

  // Switch to second tab. Token should increment and delegates should be
  // notified with the new token.
  uint32_t notified_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce([&notified_token](
                    std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                        action_states) {
        ASSERT_FALSE(action_states.empty());
        notified_token = action_states[0]->tab_switch_token;
      });
  control_->UpdateController(web_contents2.get());
  EXPECT_NE(initial_token, notified_token);
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(notified_token, states[0]->tab_switch_token);
  const uint32_t second_token = states[0]->tab_switch_token;

  // Switch back to first tab. Token should increment again and delegates should
  // be notified with the new token.
  uint32_t third_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce(
          [&third_token](std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                             action_states) {
            ASSERT_FALSE(action_states.empty());
            third_token = action_states[0]->tab_switch_token;
          });
  control_->UpdateController(web_contents());
  EXPECT_NE(second_token, third_token);
  EXPECT_NE(initial_token, third_token);
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(third_token, states[0]->tab_switch_token);
}

TEST_F(WebUIPageActionControlTest, TabSwitchTokenUpdatesOnNavigation) {
  control_->UpdateController(web_contents());

  // Show an action so we can get states.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  controller->Show(kActionAiMode);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  const uint32_t initial_token = states[0]->tab_switch_token;

  // Navigate to a new page in the same tab.
  NavigateAndCommit(GURL("https://example.com/new_page"));
  uint32_t notified_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce([&notified_token](
                    std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                        action_states) {
        ASSERT_FALSE(action_states.empty());
        notified_token = action_states[0]->tab_switch_token;
      });
  control_->UpdateController(web_contents());
  EXPECT_NE(initial_token, notified_token);

  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  // Token should have incremented.
  EXPECT_EQ(notified_token, states[0]->tab_switch_token);
  EXPECT_NE(initial_token, states[0]->tab_switch_token);
}

TEST_F(WebUIPageActionControlTest,
       TabSwitchTokenUpdatesWhenModelChangesBeforeUpdateController) {
  NavigateAndCommit(GURL("https://example.test/initial"));
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  controller->Show(kActionAiMode);
  controller->Show(kActionShowTranslate);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(2u, states.size());
  const uint32_t initial_token = states[0]->tab_switch_token;

  // Commit a same-tab navigation, then mutate a page action model *before*
  // UpdateController() is called (matching WebContentsObserver ordering).
  NavigateAndCommit(GURL("https://example.test/new_page"));

  uint32_t notified_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce([&notified_token](
                    std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                        action_states) {
        ASSERT_EQ(2u, action_states.size());
        notified_token = action_states[0]->tab_switch_token;
        EXPECT_EQ(notified_token, action_states[1]->tab_switch_token);
      });
  controller->OverrideImage(kActionAiMode,
                            ui::ImageModel::FromVectorIcon(kZoomInIcon));
  EXPECT_NE(initial_token, notified_token);

  // Subsequent UpdateController() for the same committed URL should not emit a
  // redundant notification because all visible delegates already pushed the
  // updated token.
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_)).Times(0);
  control_->UpdateController(web_contents());
}

TEST_F(WebUIPageActionControlTest,
       TabSwitchTokenUpdatesOnPendingNavigationVisibleUrlChange) {
  NavigateAndCommit(GURL("https://example.test/initial"));
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  controller->Show(kActionAiMode);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  const uint32_t initial_token = states[0]->tab_switch_token;

  // Start a browser-initiated navigation without committing yet. GetVisibleURL
  // updates immediately (which BookmarkTabHelper::DidStartNavigation reads),
  // while GetLastCommittedURL remains at the initial URL.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.test/pending"), web_contents());
  navigation->Start();
  ASSERT_EQ(GURL("https://example.test/pending"),
            web_contents()->GetVisibleURL());
  ASSERT_EQ(GURL("https://example.test/initial"),
            web_contents()->GetLastCommittedURL());

  uint32_t pending_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce([&pending_token](
                    std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                        action_states) {
        ASSERT_EQ(1u, action_states.size());
        pending_token = action_states[0]->tab_switch_token;
      });
  controller->OverrideImage(kActionAiMode,
                            ui::ImageModel::FromVectorIcon(kZoomInIcon));
  EXPECT_NE(initial_token, pending_token);

  // Committing the pending navigation updates GetLastCommittedURL() and must
  // advance the token once more.
  navigation->Commit();
  uint32_t committed_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce([&committed_token](
                    std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                        action_states) {
        ASSERT_EQ(1u, action_states.size());
        committed_token = action_states[0]->tab_switch_token;
      });
  control_->UpdateController(web_contents());
  EXPECT_NE(pending_token, committed_token);

  // A subsequent in-page icon change (neither visible nor committed URL
  // changed) must preserve the token so user interactions still animate.
  uint32_t in_page_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce([&in_page_token](
                    std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                        action_states) {
        ASSERT_EQ(1u, action_states.size());
        in_page_token = action_states[0]->tab_switch_token;
      });
  controller->ClearOverrideImage(kActionAiMode);
  EXPECT_EQ(committed_token, in_page_token);
}

TEST_F(WebUIPageActionControlTest,
       SwitchingControllerClosesActiveAnchoredMessage) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab1 =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab1);
  page_actions::PageActionController* controller1 =
      page_actions::PageActionController::From(tab1);
  ASSERT_TRUE(controller1);

  actions::ActionId target_action_id = kActionAiMode;
  controller1->Show(target_action_id);
  controller1->ShowAnchoredMessage(target_action_id,
                                   page_actions::AnchoredMessageConfig{});
  EXPECT_EQ(controller1->GetActiveAnchoredMessage(), target_action_id);

  // Switching controller to nullptr should close the active anchored message on
  // the previous controller.
  control_->UpdateController(nullptr);
  EXPECT_EQ(controller1->GetActiveAnchoredMessage(), std::nullopt);
}

TEST_F(WebUIPageActionControlTest,
       SwitchingToNewControllerClosesActiveAnchoredMessage) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab1 =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab1);
  page_actions::PageActionController* controller1 =
      page_actions::PageActionController::From(tab1);
  ASSERT_TRUE(controller1);

  actions::ActionId target_action_id = kActionAiMode;
  controller1->Show(target_action_id);
  controller1->ShowAnchoredMessage(target_action_id,
                                   page_actions::AnchoredMessageConfig{});
  EXPECT_EQ(controller1->GetActiveAnchoredMessage(), target_action_id);

  // Set up a second WebContents and Tab with its own PageActionController.
  auto tab2 = CreateTestTabContext();

  // Switching controller to web_contents2 should close the active anchored
  // message on controller1 without notifying again through controller1's model.
  testing::Mock::VerifyAndClearExpectations(&webui_delegate_);
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_)).Times(1);
  control_->UpdateController(tab2->web_contents.get());
  testing::Mock::VerifyAndClearExpectations(&webui_delegate_);
  EXPECT_EQ(controller1->GetActiveAnchoredMessage(), std::nullopt);
  EXPECT_FALSE(control_->IsAnchoredMessageShowing(target_action_id));
  EXPECT_EQ(tab2->controller->GetActiveAnchoredMessage(), std::nullopt);
}

TEST_F(WebUIPageActionControlTest,
       SwitchingToNewControllerWithActiveAnchoredMessage) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab1 =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab1);
  page_actions::PageActionController* controller1 =
      page_actions::PageActionController::From(tab1);
  ASSERT_TRUE(controller1);

  actions::ActionId target_action_id = kActionAiMode;
  controller1->Show(target_action_id);
  controller1->ShowAnchoredMessage(target_action_id,
                                   page_actions::AnchoredMessageConfig{});
  EXPECT_EQ(controller1->GetActiveAnchoredMessage(), target_action_id);

  // Set up a second WebContents and Tab with its own PageActionController.
  auto tab2 = CreateTestTabContext();

  // Show an active anchored message on controller2 before switching.
  tab2->controller->Show(target_action_id);
  tab2->controller->ShowAnchoredMessage(target_action_id,
                                        page_actions::AnchoredMessageConfig{});
  EXPECT_EQ(tab2->controller->GetActiveAnchoredMessage(), target_action_id);

  // Switching controller to web_contents2 should close the active anchored
  // message on controller1 while retaining controller2's active anchored
  // message.
  control_->UpdateController(tab2->web_contents.get());
  EXPECT_EQ(controller1->GetActiveAnchoredMessage(), std::nullopt);
}

TEST_F(WebUIPageActionControlTest,
       AnchoredMessagesPromotedAheadOfChipsAndIcons) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab);
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  ASSERT_TRUE(controller);

  // In kActionIds, initial order is:
  // 1. kActionAiMode
  // 2. kActionSidePanelShowLensOverlayResults
  // 3. kActionShowTranslate
  controller->Show(kActionAiMode);
  controller->Show(kActionSidePanelShowLensOverlayResults);
  controller->Show(kActionShowTranslate);

  // Initially all are icons, preserving initial order.
  auto states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[0]->page_action_id);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[1]->page_action_id);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[2]->page_action_id);

  const page_actions::SuggestionChipConfig chip_config{
      .priority = page_actions::PageActionPriorityCategory::
          kSecurityTrustAndPermissions,
  };
  const page_actions::AnchoredMessageConfig anchored_message_config(
      page_actions::PageActionPriorityCategory::kSecurityTrustAndPermissions);

  // Promote kActionShowTranslate to a suggestion chip. Chips precede regular
  // icons.
  controller->ShowSuggestionChip(kActionShowTranslate, chip_config);
  states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[0]->page_action_id);
  EXPECT_TRUE(states[0]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[1]->page_action_id);
  EXPECT_FALSE(states[1]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[2]->page_action_id);
  EXPECT_FALSE(states[2]->should_show_chip);

  // Show an anchored message on kActionSidePanelShowLensOverlayResults.
  // Page actions with anchored messages are placed before chips and icons.
  controller->ShowAnchoredMessage(kActionSidePanelShowLensOverlayResults,
                                  anchored_message_config);
  states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[0]->page_action_id);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[1]->page_action_id);
  EXPECT_TRUE(states[1]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[2]->page_action_id);
  EXPECT_FALSE(states[2]->should_show_chip);

  // Promote kActionAiMode to a chip as well. The anchored message remains
  // first, followed by chips in their initial order (AiMode before
  // ShowTranslate).
  controller->ShowSuggestionChip(kActionAiMode, chip_config);
  states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[0]->page_action_id);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[1]->page_action_id);
  EXPECT_TRUE(states[1]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[2]->page_action_id);
  EXPECT_TRUE(states[2]->should_show_chip);

  // Hide the anchored message. The two chips now lead (in initial order),
  // followed by the remaining icon.
  controller->HideAnchoredMessage(kActionSidePanelShowLensOverlayResults);
  states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[0]->page_action_id);
  EXPECT_TRUE(states[0]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[1]->page_action_id);
  EXPECT_TRUE(states[1]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[2]->page_action_id);
  EXPECT_FALSE(states[2]->should_show_chip);

  // Show an anchored message on a chip action (kActionShowTranslate). It should
  // now be placed first, ahead of the other chip (kActionAiMode).
  controller->ShowAnchoredMessage(kActionShowTranslate,
                                  anchored_message_config);
  states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[0]->page_action_id);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[1]->page_action_id);
  EXPECT_TRUE(states[1]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[2]->page_action_id);
  EXPECT_FALSE(states[2]->should_show_chip);

  // Hide the anchored message again. Showing an anchored message downgraded
  // kActionShowTranslate from a chip to an icon, so kActionAiMode is the sole
  // chip and leads, followed by the two icons in initial order
  // (LensOverlayResults before ShowTranslate).
  controller->HideAnchoredMessage(kActionShowTranslate);
  states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[0]->page_action_id);
  EXPECT_TRUE(states[0]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[1]->page_action_id);
  EXPECT_FALSE(states[1]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[2]->page_action_id);
  EXPECT_FALSE(states[2]->should_show_chip);

  // Re-promoting kActionShowTranslate to a chip brings both chips to the front.
  controller->ShowSuggestionChip(kActionShowTranslate, chip_config);
  states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[0]->page_action_id);
  EXPECT_TRUE(states[0]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[1]->page_action_id);
  EXPECT_TRUE(states[1]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[2]->page_action_id);
  EXPECT_FALSE(states[2]->should_show_chip);

  // Downgrading all actions back to icons restores initial order.
  controller->HideSuggestionChip(kActionAiMode);
  controller->HideSuggestionChip(kActionShowTranslate);
  states = control_->GetPageActionStates();
  ASSERT_EQ(3u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[0]->page_action_id);
  EXPECT_FALSE(states[0]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::
                kActionSidePanelShowLensOverlayResults,
            states[1]->page_action_id);
  EXPECT_FALSE(states[1]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[2]->page_action_id);
  EXPECT_FALSE(states[2]->should_show_chip);
}

// Subclasses `MockWebUIToolbarControlDelegate` to override `GetColorProvider()`
// (delegating to `widget_->GetColorProvider()` for `IconTable` color
// resolution) and route `GetView()` and `GetBrowser()` to the test widget and
// mock browser.
class WidgetBackedToolbarControlDelegate
    : public NiceMock<MockWebUIToolbarControlDelegate> {
 public:
  void Init(views::Widget* widget,
            views::View* view,
            BrowserWindowInterface* browser) {
    widget_ = widget;
    view_ = view;
    browser_ = browser;
  }

  void Reset() {
    widget_ = nullptr;
    view_ = nullptr;
    browser_ = nullptr;
  }

  BrowserWindowInterface* GetBrowser() override { return browser_; }
  views::View* GetView() override { return view_; }

  const ui::ColorProvider* GetColorProvider() const override {
    if (widget_) {
      return widget_->GetColorProvider();
    }
    return MockWebUIToolbarControlDelegate::GetColorProvider();
  }

 private:
  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<views::View> view_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
};

class WebUIPageActionControlAnchoredMessageTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    animation_duration_ =
        std::make_unique<gfx::ScopedAnimationDurationScaleMode>(
            gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    rich_animation_mode_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_widget_->SetBounds(
        gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/800, /*height=*/600));
    auto* contents_view =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    anchor_widget_->Show();

    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(anchor_widget_.get());
    browser_elements_ =
        std::make_unique<TestBrowserElements>(mock_browser_, context);
    webui_delegate_.Init(anchor_widget_.get(), contents_view, &mock_browser_);

    location_bar_element_ =
        std::make_unique<ui::test::TestElement>(kLocationBarElementId, context);
    location_bar_element_->SetNativeView(anchor_widget_->GetNativeView());
    location_bar_element_->SetScreenBounds(
        gfx::Rect(/*x=*/100, /*y=*/10, /*width=*/600, /*height=*/24));
    location_bar_element_->Show();

    ai_mode_element_ = std::make_unique<ui::test::TestElement>(
        kAiModePageActionIconElementId, context);
    ai_mode_element_->SetNativeView(anchor_widget_->GetNativeView());
    ai_mode_element_->SetScreenBounds(
        gfx::Rect(/*x=*/670, /*y=*/10, /*width=*/24, /*height=*/24));
    ai_mode_element_->Show();

    translate_element_ = std::make_unique<ui::test::TestElement>(
        kTranslatePageActionElementId, context);
    translate_element_->SetNativeView(anchor_widget_->GetNativeView());
    translate_element_->SetScreenBounds(
        gfx::Rect(/*x=*/700, /*y=*/10, /*width=*/24, /*height=*/24));
    translate_element_->Show();

    tab_ = std::make_unique<FakeTabInterface>(&profile_);
    tab_->Activate();
    tabs::TabLookupFromWebContents::CreateForWebContents(tab_->GetContents(),
                                                         tab_.get());

    pinned_actions_model_ =
        std::make_unique<PinnedToolbarActionsModel>(&profile_);
    controller_ = std::make_unique<PageActionControllerImpl>(
        *tab_,
        std::vector<actions::ActionId>(kActionIds.begin(), kActionIds.end()),
        PageActionPropertiesProvider(), pinned_actions_model_.get());

    const ui::ImageModel icon = ui::ImageModel::FromVectorIcon(kZoomInIcon);
    root_action_item_ =
        actions::ActionItem::Builder()
            .AddChild(actions::ActionItem::Builder()
                          .SetActionId(kActionAiMode)
                          .SetText(u"AI Mode")
                          .SetImage(icon))
            .AddChild(actions::ActionItem::Builder()
                          .SetActionId(kActionShowTranslate)
                          .SetText(u"Translate")
                          .SetImage(icon))
            .AddChild(actions::ActionItem::Builder()
                          .SetActionId(kActionAnchoredContextualCue)
                          .SetText(u"Contextual Cue")
                          .SetImage(icon))
            .Build();

    control_ =
        std::make_unique<WebUIPageActionControl>(root_action_item_.get());
    control_->Init(&webui_delegate_);
    control_->UpdateController(tab_->GetContents());
  }

  void TearDown() override {
    rich_animation_mode_.reset();
    animation_duration_.reset();
    control_.reset();
    controller_.reset();
    pinned_actions_model_.reset();
    tab_.reset();
    translate_element_.reset();
    ai_mode_element_.reset();
    location_bar_element_.reset();
    browser_elements_.reset();
    webui_delegate_.Reset();
    anchor_widget_.reset();
    root_action_item_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  AnchoredMessageBubbleView* ShowAnchoredMessage(
      actions::ActionId action_id,
      const AnchoredMessageConfig& config = {}) {
    controller_->Show(action_id);
    controller_->ShowAnchoredMessage(action_id, config);
    return GetAnchoredMessage(action_id);
  }

  AnchoredMessageBubbleView* GetAnchoredMessage(actions::ActionId action_id) {
    return control_->GetAnchoredMessageForTesting(action_id);
  }

  class ExitAnimationWaiter {
   public:
    explicit ExitAnimationWaiter(AnchoredMessageBubbleView* bubble)
        : bubble_tracker_(bubble), destroyed_waiter_(bubble->GetWidget()) {}
    ExitAnimationWaiter(const ExitAnimationWaiter&) = delete;
    ExitAnimationWaiter& operator=(const ExitAnimationWaiter&) = delete;

    void Wait() {
      if (views::View* view = bubble_tracker_.view()) {
        auto* bubble = static_cast<AnchoredMessageBubbleView*>(view);
        if (views::WidgetFadeAnimator* fade_animator =
                bubble->fade_animator_for_testing();
            fade_animator && fade_animator->IsFadingOut()) {
          gfx::AnimationContainerTestApi(fade_animator->container())
              .IncrementTime(base::Milliseconds(150));
        }
      }
      destroyed_waiter_.Wait();
    }

   private:
    views::ViewTracker bubble_tracker_;
    views::test::WidgetDestroyedWaiter destroyed_waiter_;
  };

  SkColor GetColor(ui::ColorId color_id) const {
    const ui::ColorProvider* color_provider =
        anchor_widget_->GetColorProvider();
    CHECK(color_provider);
    return color_provider->GetColor(color_id);
  }

  template <typename Fn>
  std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
  CaptureNextPageActionChange(Fn&& trigger) {
    std::vector<toolbar_ui_api::mojom::PageActionStatePtr> notified_states;
    EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
        .WillOnce(
            [&](std::vector<toolbar_ui_api::mojom::PageActionStatePtr> s) {
              notified_states = std::move(s);
            });
    std::forward<Fn>(trigger)();
    testing::Mock::VerifyAndClearExpectations(&webui_delegate_);
    return notified_states;
  }

  std::optional<SkColor> ResolveRegisteredIconColor(
      const toolbar_ui_api::IconHandle& handle) {
    if (handle.is_null()) {
      return std::nullopt;
    }
    auto full_state = webui_delegate_.GetIconTable().GetFullState();
    auto it = std::ranges::find_if(
        full_state, [&](const toolbar_ui_api::mojom::IconUpdatePtr& update) {
          return update->handle_id == handle.HandleId().value();
        });
    if (it == full_state.end()) {
      return std::nullopt;
    }
    return (*it)->color;
  }

  std::unique_ptr<gfx::ScopedAnimationDurationScaleMode> animation_duration_;
  gfx::AnimationTestApi::RenderModeResetter rich_animation_mode_;
  TestingProfile profile_;
  std::unique_ptr<views::Widget> anchor_widget_;
  NiceMock<MockBrowserWindowInterface> mock_browser_;
  std::unique_ptr<TestBrowserElements> browser_elements_;
  std::unique_ptr<ui::test::TestElement> location_bar_element_;
  std::unique_ptr<ui::test::TestElement> ai_mode_element_;
  std::unique_ptr<ui::test::TestElement> translate_element_;
  WidgetBackedToolbarControlDelegate webui_delegate_;
  std::unique_ptr<FakeTabInterface> tab_;
  std::unique_ptr<PinnedToolbarActionsModel> pinned_actions_model_;
  std::unique_ptr<PageActionControllerImpl> controller_;
  std::unique_ptr<actions::ActionItem> root_action_item_;
  std::unique_ptr<WebUIPageActionControl> control_;
};

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageExitAnimation) {
  AnchoredMessageBubbleView* bubble = ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  ExitAnimationWaiter exit_waiter(bubble);
  ASSERT_TRUE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  EXPECT_EQ(bubble->GetAnchor().GetIfElement(), translate_element_.get());
  EXPECT_FALSE(bubble->GetBubbleFrameView()->layer());
  views::WidgetFadeAnimator* fade_animator =
      bubble->fade_animator_for_testing();
  ASSERT_TRUE(fade_animator);
  EXPECT_TRUE(fade_animator->IsFadingIn());

  controller_->HideAnchoredMessage(kActionShowTranslate);

  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  ASSERT_EQ(GetAnchoredMessage(kActionShowTranslate), bubble);
  EXPECT_TRUE(bubble->is_closing());
  EXPECT_FALSE(bubble->GetWidget()->IsClosed());
  EXPECT_TRUE(fade_animator->IsFadingOut());

  exit_waiter.Wait();
  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), nullptr);
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageReordersIconsImmediatelyOnExitAnimation) {
  controller_->Show(kActionAiMode);
  AnchoredMessageBubbleView* bubble = ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  ExitAnimationWaiter exit_waiter(bubble);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(2u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[0]->page_action_id);

  testing::Mock::VerifyAndClearExpectations(&webui_delegate_);
  std::vector<toolbar_ui_api::mojom::PageActionStatePtr> notified_states;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce(
          [&notified_states](
              std::vector<toolbar_ui_api::mojom::PageActionStatePtr> states) {
            notified_states = std::move(states);
          });

  controller_->HideAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble->is_closing());

  ASSERT_EQ(2u, notified_states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            notified_states[0]->page_action_id);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            notified_states[1]->page_action_id);

  exit_waiter.Wait();
}

// HideAnchoredMessage() dismisses only the bubble while keeping the page action
// icon visible in the toolbar, so clicks during fade-out invoke the action.
TEST_F(WebUIPageActionControlAnchoredMessageTest,
       ClickDuringExitAnimationInvokesAction) {
  AnchoredMessageBubbleView* bubble = ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  ExitAnimationWaiter exit_waiter(bubble);

  actions::ActionItem* translate_action_item =
      actions::ActionManager::Get().FindAction(kActionShowTranslate,
                                               root_action_item_.get());
  ASSERT_TRUE(translate_action_item);
  int invoke_count = 0;
  translate_action_item->SetInvokeActionCallback(
      base::BindRepeating([](int* count, actions::ActionItem*,
                             actions::ActionInvocationContext) { ++(*count); },
                          &invoke_count));

  controller_->HideAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble->is_closing());

  base::test::TestFuture<
      base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
      click_future;
  control_->OnPageActionClick(
      toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
      PageActionTrigger::kMouse, click_future.GetCallback());
  EXPECT_OK(click_future.Get());
  EXPECT_EQ(1, invoke_count);

  exit_waiter.Wait();
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageAnimatesOutWhenPageActionHidden) {
  AnchoredMessageBubbleView* bubble = ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  views::WidgetFadeAnimator* fade_animator =
      bubble->fade_animator_for_testing();
  ASSERT_TRUE(fade_animator);
  ExitAnimationWaiter exit_waiter(bubble);

  controller_->Hide(kActionShowTranslate);
  translate_element_->Hide();

  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  EXPECT_EQ(controller_->GetActiveAnchoredMessage(), std::nullopt);
  EXPECT_TRUE(bubble->is_closing());
  EXPECT_FALSE(bubble->GetWidget()->IsClosed());
  EXPECT_TRUE(fade_animator->IsFadingOut());

  exit_waiter.Wait();
  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), nullptr);
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       DeferredAnchoredMessageClearedWhenPageActionHidden) {
  translate_element_->Hide();
  ASSERT_EQ(ShowAnchoredMessage(kActionShowTranslate), nullptr);
  ASSERT_EQ(controller_->GetActiveAnchoredMessage(), kActionShowTranslate);

  controller_->Hide(kActionShowTranslate);
  EXPECT_EQ(controller_->GetActiveAnchoredMessage(), std::nullopt);

  translate_element_->Show();
  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), nullptr);
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredContextualCueAnchorsToPageActionElement) {
  // The message is postponed until the corresponding anchor element exists.
  location_bar_element_->Hide();
  ASSERT_EQ(ShowAnchoredMessage(kActionAnchoredContextualCue), nullptr);
  location_bar_element_->Show();
  EXPECT_EQ(GetAnchoredMessage(kActionAnchoredContextualCue), nullptr);

  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(anchor_widget_.get());
  ui::test::TestElement cue_element(
      kAnchoredContextualCuePageActionIconElementId, context);
  cue_element.SetNativeView(anchor_widget_->GetNativeView());
  cue_element.SetScreenBounds(
      gfx::Rect(/*x=*/700, /*y=*/10, /*width=*/24, /*height=*/24));
  cue_element.Show();

  AnchoredMessageBubbleView* bubble =
      GetAnchoredMessage(kActionAnchoredContextualCue);
  ASSERT_TRUE(bubble);
  EXPECT_EQ(bubble->GetAnchor().GetIfElement(), &cue_element);
  EXPECT_TRUE(control_->IsAnchoredMessageShowing(kActionAnchoredContextualCue));
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageReshownDuringExitAnimation) {
  AnchoredMessageBubbleView* bubble = ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  views::Label* label =
      views::ElementTrackerViews::GetInstance()
          ->GetFirstMatchingViewAs<views::Label>(
              AnchoredMessageBubbleView::kAnchoredMessageLabelId,
              views::ElementTrackerViews::GetContextForView(bubble));
  ASSERT_TRUE(label);
  EXPECT_TRUE(label->GetText().empty());

  controller_->Hide(kActionShowTranslate);
  translate_element_->Hide();
  ASSERT_TRUE(bubble->is_closing());
  ASSERT_TRUE(bubble->GetAnchor().IsNull());

  controller_->SetAnchoredMessageText(kActionShowTranslate, u"Updated");
  ShowAnchoredMessage(kActionShowTranslate);

  EXPECT_TRUE(bubble->is_closing());
  EXPECT_TRUE(bubble->GetAnchor().IsNull());
  EXPECT_TRUE(label->GetText().empty());
  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionShowTranslate));

  translate_element_->Show();

  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), bubble);
  EXPECT_EQ(bubble->GetAnchor().GetIfElement(), translate_element_.get());
  EXPECT_EQ(label->GetText(), u"Updated");
  EXPECT_FALSE(bubble->is_closing());
  EXPECT_TRUE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  views::WidgetFadeAnimator* fade_animator =
      bubble->fade_animator_for_testing();
  ASSERT_TRUE(fade_animator);
  EXPECT_FALSE(fade_animator->IsFadingOut());

  EXPECT_FALSE(bubble->GetWidget()->IsClosed());
  EXPECT_TRUE(bubble->GetWidget()->IsVisible());
  EXPECT_TRUE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageExitAnimationFinishesBeforeDeferredReshow) {
  AnchoredMessageBubbleView* bubble = ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  ExitAnimationWaiter exit_waiter(bubble);

  controller_->Hide(kActionShowTranslate);
  translate_element_->Hide();
  ASSERT_TRUE(bubble->is_closing());

  ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_EQ(controller_->GetActiveAnchoredMessage(), kActionShowTranslate);

  exit_waiter.Wait();
  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), nullptr);
  EXPECT_EQ(controller_->GetActiveAnchoredMessage(), kActionShowTranslate);

  translate_element_->Show();

  AnchoredMessageBubbleView* reshown_bubble =
      GetAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(reshown_bubble);
  EXPECT_EQ(reshown_bubble->GetAnchor().GetIfElement(),
            translate_element_.get());
  EXPECT_FALSE(reshown_bubble->is_closing());
  EXPECT_TRUE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageCloseOnDeactivateNotifiesController) {
  AnchoredMessageBubbleView* bubble = ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  ASSERT_EQ(controller_->GetActiveAnchoredMessage(), kActionShowTranslate);
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble->GetWidget());

  bubble->GetWidget()->CloseWithReason(views::Widget::ClosedReason::kLostFocus);

  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), nullptr);
  EXPECT_EQ(controller_->GetActiveAnchoredMessage(), std::nullopt);
  destroyed_waiter.Wait();
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageClosesImmediatelyWithReducedMotion) {
  rich_animation_mode_.reset();
  auto reduced_motion = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);

  AnchoredMessageBubbleView* bubble = ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble->GetWidget());

  controller_->HideAnchoredMessage(kActionShowTranslate);

  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), nullptr);
  destroyed_waiter.Wait();
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       UpdateControllerDestroysAnchoredMessageSynchronously) {
  AnchoredMessageBubbleView* live_bubble =
      ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(live_bubble);
  ASSERT_EQ(controller_->GetActiveAnchoredMessage(), kActionShowTranslate);
  base::WeakPtr<views::Widget> live_widget =
      live_bubble->GetWidget()->GetWeakPtr();

  testing::Mock::VerifyAndClearExpectations(&webui_delegate_);
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_)).Times(1);
  control_->UpdateController(/*web_contents=*/nullptr);
  testing::Mock::VerifyAndClearExpectations(&webui_delegate_);
  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), nullptr);
  EXPECT_FALSE(live_widget);
  EXPECT_EQ(controller_->GetActiveAnchoredMessage(), std::nullopt);

  control_->UpdateController(tab_->GetContents());
  AnchoredMessageBubbleView* closing_bubble =
      ShowAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(closing_bubble);
  base::WeakPtr<views::Widget> closing_widget =
      closing_bubble->GetWidget()->GetWeakPtr();

  controller_->HideAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(closing_bubble->is_closing());

  control_->UpdateController(/*web_contents=*/nullptr);
  EXPECT_EQ(GetAnchoredMessage(kActionShowTranslate), nullptr);
  EXPECT_FALSE(closing_widget);
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageTonalStylingOnShowAndHide) {
  const SkColor action_icon_color = GetColor(kColorOmniboxActionIcon);
  const SkColor tonal_fg_color = GetColor(kColorOmniboxIconForegroundTonal);
  ASSERT_NE(action_icon_color, tonal_fg_color);

  controller_->SetTrailingImage(
      kActionAiMode,
      ui::ImageModel::FromVectorIcon(vector_icons::kArrowForwardIcon));
  controller_->SetShowTrailingIcon(kActionAiMode, /*show=*/true);
  controller_->Show(kActionAiMode);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_FALSE(states[0]->is_anchored_message_visible);
  EXPECT_FALSE(states[0]->should_show_chip);
  EXPECT_EQ(action_icon_color, ResolveRegisteredIconColor(states[0]->icon));
  ASSERT_TRUE(states[0]->trailing_icon.has_value());
  EXPECT_EQ(action_icon_color,
            ResolveRegisteredIconColor(*states[0]->trailing_icon));

  // Showing an anchored message notifies WebUI with tonal icon colors while
  // keeping `should_show_chip` false.
  auto notified_states = CaptureNextPageActionChange([&] {
    controller_->ShowAnchoredMessage(kActionAiMode,
                                     page_actions::AnchoredMessageConfig{});
  });

  AnchoredMessageBubbleView* bubble = GetAnchoredMessage(kActionAiMode);
  ASSERT_TRUE(bubble);
  EXPECT_TRUE(control_->IsAnchoredMessageShowing(kActionAiMode));
  ASSERT_EQ(1u, notified_states.size());
  EXPECT_TRUE(notified_states[0]->is_anchored_message_visible);
  EXPECT_FALSE(notified_states[0]->should_show_chip);
  EXPECT_EQ(tonal_fg_color,
            ResolveRegisteredIconColor(notified_states[0]->icon));
  ASSERT_TRUE(notified_states[0]->trailing_icon.has_value());
  EXPECT_EQ(tonal_fg_color,
            ResolveRegisteredIconColor(*notified_states[0]->trailing_icon));

  // Hiding via the controller starts `AnimateOut()` first and immediately
  // notifies WebUI with non-tonal icon colors restored.
  ExitAnimationWaiter exit_waiter(bubble);
  notified_states = CaptureNextPageActionChange(
      [&] { controller_->HideAnchoredMessage(kActionAiMode); });

  EXPECT_TRUE(bubble->is_closing());
  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionAiMode));
  ASSERT_EQ(1u, notified_states.size());
  EXPECT_FALSE(notified_states[0]->is_anchored_message_visible);
  EXPECT_EQ(action_icon_color,
            ResolveRegisteredIconColor(notified_states[0]->icon));
  ASSERT_TRUE(notified_states[0]->trailing_icon.has_value());
  EXPECT_EQ(action_icon_color,
            ResolveRegisteredIconColor(*notified_states[0]->trailing_icon));

  exit_waiter.Wait();
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageForegroundColorOverrideTakesPrecedence) {
  const SkColor tonal_fg_color = GetColor(kColorOmniboxIconForegroundTonal);
  const ui::ColorId override_fg_color_id = ui::kColorSysPrimary;
  const SkColor override_fg_color = GetColor(override_fg_color_id);
  ASSERT_NE(tonal_fg_color, override_fg_color);

  controller_->SetTrailingImage(
      kActionAiMode,
      ui::ImageModel::FromVectorIcon(vector_icons::kArrowForwardIcon));
  controller_->SetShowTrailingIcon(kActionAiMode, /*show=*/true);
  ASSERT_TRUE(ShowAnchoredMessage(kActionAiMode));

  controller_->OverrideForegroundColor(kActionAiMode, override_fg_color_id);
  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_TRUE(states[0]->is_anchored_message_visible);
  EXPECT_EQ(override_fg_color, states[0]->foreground_color_override);
  EXPECT_EQ(override_fg_color, ResolveRegisteredIconColor(states[0]->icon));
  ASSERT_TRUE(states[0]->trailing_icon.has_value());
  EXPECT_EQ(override_fg_color,
            ResolveRegisteredIconColor(*states[0]->trailing_icon));

  // Clearing the override while the anchored message remains visible restores
  // tonal icon colors.
  controller_->OverrideForegroundColor(kActionAiMode, std::nullopt);
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(tonal_fg_color, ResolveRegisteredIconColor(states[0]->icon));
  ASSERT_TRUE(states[0]->trailing_icon.has_value());
  EXPECT_EQ(tonal_fg_color,
            ResolveRegisteredIconColor(*states[0]->trailing_icon));
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessagePreservesCascadingAccentIconColor) {
  const SkColor tonal_fg_color = GetColor(kColorOmniboxIconForegroundTonal);
  const SkColor expected_accent_color =
      color_utils::BlendForMinContrast(
          GetColor(ui::kColorFocusableBorderFocused), GetColor(kColorToolbar),
          /*high_contrast_foreground=*/std::nullopt,
          color_utils::kMinimumVisibleContrastRatio)
          .color;

  ASSERT_TRUE(ShowAnchoredMessage(kActionAiMode));

  controller_->OverrideImage(
      kActionAiMode, ui::ImageModel::FromVectorIcon(kZoomInIcon),
      page_actions::PageActionColorSource::kCascadingAccent);
  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_TRUE(states[0]->is_anchored_message_visible);
  EXPECT_EQ(expected_accent_color, ResolveRegisteredIconColor(states[0]->icon));

  // Switching back to `kForeground` restores tonal icon color.
  controller_->OverrideImage(kActionAiMode,
                             ui::ImageModel::FromVectorIcon(kZoomInIcon),
                             page_actions::PageActionColorSource::kForeground);
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(tonal_fg_color, ResolveRegisteredIconColor(states[0]->icon));
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageDeferredShowAppliesTonalStylingWhenElementAppears) {
  const SkColor action_icon_color = GetColor(kColorOmniboxActionIcon);
  const SkColor tonal_fg_color = GetColor(kColorOmniboxIconForegroundTonal);
  ASSERT_NE(action_icon_color, tonal_fg_color);

  // Hide the Translate anchor element so bubble creation is deferred until
  // `ElementTracker` reports the element shown.
  translate_element_->Hide();

  controller_->Show(kActionAiMode);
  controller_->ShowSuggestionChip(
      kActionAiMode, {.priority = page_actions::PageActionPriorityCategory::
                          kSecurityTrustAndPermissions});
  controller_->Show(kActionShowTranslate);

  // Request anchored message while the anchor element is not yet tracked:
  // `kActionShowTranslate` is promoted in order because the model requests an
  // anchored message, but `is_anchored_message_visible` remains false and the
  // icon color stays `kColorOmniboxActionIcon` until the bubble actually shows.
  controller_->ShowAnchoredMessage(
      kActionShowTranslate,
      page_actions::AnchoredMessageConfig(
          page_actions::PageActionPriorityCategory::kUserInteraction));
  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionShowTranslate));

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(2u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[0]->page_action_id);
  EXPECT_FALSE(states[0]->is_anchored_message_visible);
  EXPECT_EQ(action_icon_color, ResolveRegisteredIconColor(states[0]->icon));
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            states[1]->page_action_id);
  EXPECT_TRUE(states[1]->should_show_chip);

  // Showing the anchor element triggers `OnElementShownForAnchoredMessage()`,
  // spawning the bubble and notifying WebUI with tonal styling.
  auto notified_states =
      CaptureNextPageActionChange([&] { translate_element_->Show(); });

  EXPECT_TRUE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  ASSERT_EQ(2u, notified_states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            notified_states[0]->page_action_id);
  EXPECT_TRUE(notified_states[0]->is_anchored_message_visible);
  EXPECT_EQ(tonal_fg_color,
            ResolveRegisteredIconColor(notified_states[0]->icon));
}

TEST_F(WebUIPageActionControlAnchoredMessageTest,
       AnchoredMessageDirectWidgetCloseRevertsStateAndOrder) {
  const SkColor action_icon_color = GetColor(kColorOmniboxActionIcon);
  const SkColor tonal_fg_color = GetColor(kColorOmniboxIconForegroundTonal);
  ASSERT_NE(action_icon_color, tonal_fg_color);

  controller_->Show(kActionAiMode);
  controller_->ShowSuggestionChip(
      kActionAiMode, {.priority = page_actions::PageActionPriorityCategory::
                          kSecurityTrustAndPermissions});
  controller_->Show(kActionShowTranslate);
  controller_->ShowAnchoredMessage(
      kActionShowTranslate,
      page_actions::AnchoredMessageConfig(
          page_actions::PageActionPriorityCategory::kUserInteraction));

  AnchoredMessageBubbleView* bubble = GetAnchoredMessage(kActionShowTranslate);
  ASSERT_TRUE(bubble);
  ASSERT_TRUE(bubble->GetWidget());
  auto states = control_->GetPageActionStates();
  ASSERT_EQ(2u, states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            states[0]->page_action_id);
  EXPECT_TRUE(states[0]->is_anchored_message_visible);
  EXPECT_EQ(tonal_fg_color, ResolveRegisteredIconColor(states[0]->icon));

  // Closing the bubble widget directly (e.g. Escape / focus loss) notifies
  // WebUI to revert tonal styling and restore chip-before-icon ordering.
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble->GetWidget());
  auto notified_states = CaptureNextPageActionChange([&] {
    bubble->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kEscKeyPressed);
  });

  EXPECT_FALSE(control_->IsAnchoredMessageShowing(kActionShowTranslate));
  EXPECT_EQ(std::nullopt, controller_->GetActiveAnchoredMessage());
  ASSERT_EQ(2u, notified_states.size());
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionAiMode,
            notified_states[0]->page_action_id);
  EXPECT_TRUE(notified_states[0]->should_show_chip);
  EXPECT_EQ(toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
            notified_states[1]->page_action_id);
  EXPECT_FALSE(notified_states[1]->is_anchored_message_visible);
  EXPECT_FALSE(notified_states[1]->should_show_chip);
  EXPECT_EQ(action_icon_color,
            ResolveRegisteredIconColor(notified_states[1]->icon));
  destroyed_waiter.Wait();
}
}  // namespace

}  // namespace page_actions
