// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
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
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace page_actions {

namespace {

using testing::_;
using testing::NiceMock;

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
            .Build();

    control_ =
        std::make_unique<WebUIPageActionControl>(root_action_item_.get());
    control_->Init(&webui_delegate_);
  }

  void TearDown() override {
    control_.reset();
    page_action_controller_.reset();
    tab_features_.reset();
    DeleteContents();
    mock_tab_.reset();
    root_action_item_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
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

}  // namespace

}  // namespace page_actions
