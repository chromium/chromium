// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_view.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

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
            .AddChild(actions::ActionItem::Builder().SetActionId(kActionAiMode))
            .AddChild(actions::ActionItem::Builder().SetActionId(
                kActionShowTranslate))
            .AddChild(actions::ActionItem::Builder().SetActionId(
                kActionSidePanelShowLensOverlayResults))
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

  EXPECT_CALL(observer, OnPageActionModelChanged(_))
      .WillOnce([](const page_actions::PageActionModelInterface& model) {
        EXPECT_TRUE(model.IsChipShowing());
      });

  base::RunLoop run_loop;
  control_->OnPageActionChipShowingChanged(
      target_mojom_id,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
                 result) {
            EXPECT_OK(result);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
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

  EXPECT_DEATH(view_interface->GetIconLabelBubbleViewNotMigrated(), "");

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

  page_actions::PageActionViewInterface* lens_view =
      control_->GetPageActionViewInterface(
          kActionSidePanelShowLensOverlayResults);
  ASSERT_TRUE(lens_view);

  // 1. When browser is null, BubbleAnchor is null.
  EXPECT_CALL(webui_delegate_, GetBrowser())
      .WillRepeatedly(testing::Return(nullptr));
  EXPECT_TRUE(ai_mode_view->GetBubbleAnchor().IsNull());

  // 2. When browser is present, but neither the page action element nor the
  // location bar element is tracked, BubbleAnchor is null.
  EXPECT_CALL(webui_delegate_, GetBrowser())
      .WillRepeatedly(testing::Return(&mock_browser));
  EXPECT_TRUE(ai_mode_view->GetBubbleAnchor().IsNull());
  EXPECT_TRUE(lens_view->GetBubbleAnchor().IsNull());

  // 3. When the specific page action element is not tracked, but the location
  // bar element is tracked, GetBubbleAnchor falls back to the location bar.
  ui::test::TestElement location_bar_element(kLocationBarElementId,
                                             kTestContext);
  location_bar_element.Show();

  views::BubbleAnchor fallback_anchor = ai_mode_view->GetBubbleAnchor();
  EXPECT_FALSE(fallback_anchor.IsNull());
  EXPECT_EQ(fallback_anchor.GetIfElement(), &location_bar_element);

  // An action without a specific element identifier (e.g. LensOverlay) also
  // anchors to the location bar.
  views::BubbleAnchor lens_anchor = lens_view->GetBubbleAnchor();
  EXPECT_FALSE(lens_anchor.IsNull());
  EXPECT_EQ(lens_anchor.GetIfElement(), &location_bar_element);

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

TEST_F(WebUIPageActionControlTest, MouseClickSuppression) {
  control_->UpdateController(web_contents());

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  ASSERT_TRUE(tab);
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  ASSERT_TRUE(controller);

  actions::ActionItem* ai_mode_action_item =
      actions::ActionManager::Get().FindAction(kActionAiMode,
                                               root_action_item_.get());
  ASSERT_TRUE(ai_mode_action_item);
  actions::ActionItem* translate_action_item =
      actions::ActionManager::Get().FindAction(kActionShowTranslate,
                                               root_action_item_.get());
  ASSERT_TRUE(translate_action_item);

  int ai_mode_invoked_count = 0;
  ai_mode_action_item->SetInvokeActionCallback(
      base::BindRepeating([](int* count, actions::ActionItem*,
                             actions::ActionInvocationContext) { ++(*count); },
                          &ai_mode_invoked_count));

  int translate_invoked_count = 0;
  translate_action_item->SetInvokeActionCallback(
      base::BindRepeating([](int* count, actions::ActionItem*,
                             actions::ActionInvocationContext) { ++(*count); },
                          &translate_invoked_count));

  controller->Show(kActionAiMode);
  controller->Show(kActionShowTranslate);

  control_->SetSuppressionThresholdForTesting(base::Seconds(1));

  // 1. Initial click without bubble being shown is NOT suppressed.
  control_->OnPageActionPointerDown(
      toolbar_ui_api::mojom::PageActionId::kActionAiMode);
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionClick(
        toolbar_ui_api::mojom::PageActionId::kActionAiMode,
        PageActionTrigger::kMouse, future.GetCallback());
    EXPECT_OK(future.Get());
    EXPECT_EQ(1, ai_mode_invoked_count);
  }

  // 2. Simulate bubble showing and then closing.
  ai_mode_action_item->SetIsShowingBubble(true);
  ai_mode_action_item->SetIsShowingBubble(false);

  // Pointer down occurs immediately within the suppression threshold.
  control_->OnPageActionPointerDown(
      toolbar_ui_api::mojom::PageActionId::kActionAiMode);

  // 3. A non-pointer click (e.g. keyboard) should NOT be suppressed.
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionClick(
        toolbar_ui_api::mojom::PageActionId::kActionAiMode,
        PageActionTrigger::kKeyboard, future.GetCallback());
    EXPECT_OK(future.Get());
    EXPECT_EQ(2, ai_mode_invoked_count);
  }

  // 4. A mouse click immediately after bubble close + pointerdown IS
  // suppressed.
  ai_mode_action_item->SetIsShowingBubble(true);
  ai_mode_action_item->SetIsShowingBubble(false);
  control_->OnPageActionPointerDown(
      toolbar_ui_api::mojom::PageActionId::kActionAiMode);
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionClick(
        toolbar_ui_api::mojom::PageActionId::kActionAiMode,
        PageActionTrigger::kMouse, future.GetCallback());
    EXPECT_OK(future.Get());
    // Action should NOT have been invoked.
    EXPECT_EQ(2, ai_mode_invoked_count);
  }

  // 5. A gesture/touch click immediately after bubble close + pointerdown IS
  // suppressed.
  ai_mode_action_item->SetIsShowingBubble(true);
  ai_mode_action_item->SetIsShowingBubble(false);
  control_->OnPageActionPointerDown(
      toolbar_ui_api::mojom::PageActionId::kActionAiMode);
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionClick(
        toolbar_ui_api::mojom::PageActionId::kActionAiMode,
        PageActionTrigger::kGesture, future.GetCallback());
    EXPECT_OK(future.Get());
    // Action should NOT have been invoked.
    EXPECT_EQ(2, ai_mode_invoked_count);
  }

  // 6. Clicking a different page action chip is NOT suppressed.
  ai_mode_action_item->SetIsShowingBubble(true);
  ai_mode_action_item->SetIsShowingBubble(false);
  control_->OnPageActionPointerDown(
      toolbar_ui_api::mojom::PageActionId::kActionShowTranslate);
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionClick(
        toolbar_ui_api::mojom::PageActionId::kActionShowTranslate,
        PageActionTrigger::kMouse, future.GetCallback());
    EXPECT_OK(future.Get());
    EXPECT_EQ(1, translate_invoked_count);
  }

  // 7. Pointer down while bubble is still open suppresses the subsequent
  // mouse click.
  ai_mode_action_item->SetIsShowingBubble(true);
  control_->OnPageActionPointerDown(
      toolbar_ui_api::mojom::PageActionId::kActionAiMode);
  ai_mode_action_item->SetIsShowingBubble(false);
  {
    base::test::TestFuture<
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>>
        future;
    control_->OnPageActionClick(
        toolbar_ui_api::mojom::PageActionId::kActionAiMode,
        PageActionTrigger::kMouse, future.GetCallback());
    EXPECT_OK(future.Get());
    // Action should NOT have been invoked.
    EXPECT_EQ(2, ai_mode_invoked_count);
  }
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

TEST_F(WebUIPageActionControlTest, IconAnimationTokenUpdatesOnTabSwitch) {
  scoped_feature_list_.InitAndEnableFeature(features::kToolbarGlowUp);
  control_->UpdateController(web_contents());

  // Show an action so we can get states.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  controller->Show(kActionAiMode);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  const uint32_t initial_token = states[0]->icon_animation_token;

  // Update controller with the same web contents. Token should not change.
  control_->UpdateController(web_contents());
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(initial_token, states[0]->icon_animation_token);

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
        notified_token = action_states[0]->icon_animation_token;
      });
  control_->UpdateController(web_contents2.get());
  EXPECT_NE(initial_token, notified_token);
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(notified_token, states[0]->icon_animation_token);
  const uint32_t second_token = states[0]->icon_animation_token;

  // Switch back to first tab. Token should increment again and delegates should
  // be notified with the new token.
  uint32_t third_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce(
          [&third_token](std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                             action_states) {
            ASSERT_FALSE(action_states.empty());
            third_token = action_states[0]->icon_animation_token;
          });
  control_->UpdateController(web_contents());
  EXPECT_NE(second_token, third_token);
  EXPECT_NE(initial_token, third_token);
  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(third_token, states[0]->icon_animation_token);
}

TEST_F(WebUIPageActionControlTest, IconAnimationTokenUpdatesOnNavigation) {
  scoped_feature_list_.InitAndEnableFeature(features::kToolbarGlowUp);
  control_->UpdateController(web_contents());

  // Show an action so we can get states.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  controller->Show(kActionAiMode);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  const uint32_t initial_token = states[0]->icon_animation_token;

  // Navigate to a new page in the same tab.
  NavigateAndCommit(GURL("https://example.com/new_page"));
  uint32_t notified_token = 0;
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_))
      .WillOnce([&notified_token](
                    std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
                        action_states) {
        ASSERT_FALSE(action_states.empty());
        notified_token = action_states[0]->icon_animation_token;
      });
  control_->UpdateController(web_contents());
  EXPECT_NE(initial_token, notified_token);

  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  // Token should have incremented.
  EXPECT_EQ(notified_token, states[0]->icon_animation_token);
  EXPECT_NE(initial_token, states[0]->icon_animation_token);
}

class WebUIPageActionControlDisabledGlowUpTest
    : public WebUIPageActionControlTest {
 public:
  WebUIPageActionControlDisabledGlowUpTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kToolbarGlowUp, features::kDesktopGlowUp});
  }
};

TEST_F(WebUIPageActionControlDisabledGlowUpTest,
       NoNavigationUpdateWhenGlowUpDisabled) {
  control_->UpdateController(web_contents());

  // Show an action so we can get states.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  page_actions::PageActionController* controller =
      page_actions::PageActionController::From(tab);
  controller->Show(kActionAiMode);

  auto states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(0u, states[0]->icon_animation_token);

  // When glow up is disabled, navigating in the same tab should NOT notify
  // delegates or update the token.
  EXPECT_CALL(webui_delegate_, OnPageActionChanged(_)).Times(0);

  NavigateAndCommit(GURL("https://example.com/new_page"));
  control_->UpdateController(web_contents());

  states = control_->GetPageActionStates();
  ASSERT_EQ(1u, states.size());
  EXPECT_EQ(0u, states[0]->icon_animation_token);
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
  // message on controller1.
  control_->UpdateController(tab2->web_contents.get());
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
  EXPECT_EQ(tab2->controller->GetActiveAnchoredMessage(), target_action_id);
}
}  // namespace

}  // namespace page_actions
