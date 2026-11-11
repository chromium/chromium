// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/autofill/bubble_manager_impl.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

class FakeAutofillBubble : public AutofillBubbleBase {
 public:
  void set_on_hide(base::RepeatingClosure on_hide) {
    on_hide_ = std::move(on_hide);
  }

  void Hide() override {
    if (on_hide_) {
      on_hide_.Run();
    }
  }
  bool IsMouseHovered() const override { return false; }

 private:
  base::RepeatingClosure on_hide_;
};

class TestBubbleController : public AutofillBubbleControllerBase {
 public:
  TestBubbleController(tabs::TabInterface* tab,
                       FakeAutofillBubble* bubble,
                       BubbleType bubble_type,
                       PageActionConfig page_action_config)
      : AutofillBubbleControllerBase(tab, page_action_config),
        bubble_(bubble),
        bubble_type_(bubble_type) {
    bubble_->set_on_hide(base::BindRepeating(&TestBubbleController::CloseBubble,
                                             weak_ptr_factory_.GetWeakPtr()));
  }

  TestBubbleController(content::WebContents* web_contents,
                       FakeAutofillBubble* bubble,
                       BubbleType bubble_type,
                       PageActionConfig page_action_config)
      : AutofillBubbleControllerBase(web_contents, page_action_config),
        bubble_(bubble),
        bubble_type_(bubble_type) {
    bubble_->set_on_hide(base::BindRepeating(&TestBubbleController::CloseBubble,
                                             weak_ptr_factory_.GetWeakPtr()));
  }

  ~TestBubbleController() override = default;

  void CloseBubble() {
    ResetBubbleViewAndInformBubbleManager();
    UpdatePageActionIcon();
  }

  void ClearObservedWebContents() { Observe(nullptr); }

  // BubbleControllerBase:
  void OnBubbleDiscarded() override {}
  BubbleType GetBubbleType() const override { return bubble_type_; }
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr()
      override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // AutofillBubbleControllerBase:
  void DoShowBubble() override { SetBubbleView(*bubble_); }

  const raw_ptr<FakeAutofillBubble> bubble_;
  const BubbleType bubble_type_;
  base::WeakPtrFactory<TestBubbleController> weak_ptr_factory_{this};
};

class AutofillBubbleControllerBaseTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ON_CALL(tab_interface_, GetContents())
        .WillByDefault(Return(web_contents()));
    ON_CALL(tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(&browser_window_interface_));
    ON_CALL(tab_interface_, IsActivated()).WillByDefault(Return(true));
    ON_CALL(tab_interface_, RegisterWillDiscardContents)
        .WillByDefault(
            [this](tabs::TabInterface::WillDiscardContentsCallback cb) {
              return will_discard_callbacks_.Add(std::move(cb));
            });
    ON_CALL(tab_interface_, RegisterDidActivate)
        .WillByDefault([this](tabs::TabInterface::DidActivateCallback cb) {
          return did_activate_callbacks_.Add(std::move(cb));
        });
    ON_CALL(tab_interface_, RegisterWillDeactivate)
        .WillByDefault([this](tabs::TabInterface::WillDeactivateCallback cb) {
          return will_deactivate_callbacks_.Add(std::move(cb));
        });

    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(Return(profile()));

    browser_actions_ =
        std::make_unique<BrowserActions>(&browser_window_interface_);
    actions::ActionItem* root_action_item = nullptr;
    actions::ActionManager::GetForTesting().AddAction(
        actions::ActionItem::Builder()
            .CopyAddressTo(&root_action_item)
            .AddChildren(actions::ActionItem::Builder()
                             .CopyAddressTo(&managed_action_item_)
                             .SetActionId(kActionGmailOtpOptIn),
                         actions::ActionItem::Builder()
                             .CopyAddressTo(&synced_action_item_)
                             .SetActionId(kActionWalletReminderNotice))
            .Build());
    browser_actions_->set_root_action_item_for_testing(root_action_item);

    page_action_registration_ = std::make_unique<
        ui::ScopedUnownedUserData<page_actions::PageActionController>>(
        tab_interface_.GetUnownedUserDataHost(), page_action_controller_);
    bubble_manager_ = std::make_unique<BubbleManagerImpl>(&tab_interface_);
  }

  void TearDown() override {
    bubble_manager_.reset();
    page_action_registration_.reset();
    managed_action_item_ = nullptr;
    synced_action_item_ = nullptr;
    browser_actions_.reset();
    actions::ActionManager::ResetForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  tabs::MockTabInterface& tab_interface() { return tab_interface_; }

  page_actions::MockPageActionController& page_action_controller() {
    return page_action_controller_;
  }

  const actions::ActionItem& managed_action_item() const {
    return CHECK_DEREF(managed_action_item_.get());
  }

  const actions::ActionItem& synced_action_item() const {
    return CHECK_DEREF(synced_action_item_.get());
  }

  void DeactivateTab() { will_deactivate_callbacks_.Notify(&tab_interface_); }

 private:
  tabs::MockTabInterface tab_interface_;
  NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  base::RepeatingCallbackList<
      void(tabs::TabInterface*, content::WebContents*, content::WebContents*)>
      will_discard_callbacks_;
  base::RepeatingCallbackList<void(tabs::TabInterface*)>
      did_activate_callbacks_;
  base::RepeatingCallbackList<void(tabs::TabInterface*)>
      will_deactivate_callbacks_;

  std::unique_ptr<BrowserActions> browser_actions_;
  raw_ptr<actions::ActionItem> managed_action_item_ = nullptr;
  raw_ptr<actions::ActionItem> synced_action_item_ = nullptr;
  NiceMock<page_actions::MockPageActionController> page_action_controller_;
  std::unique_ptr<ui::ScopedUnownedUserData<page_actions::PageActionController>>
      page_action_registration_;
  std::unique_ptr<BubbleManagerImpl> bubble_manager_;
};

// Tests that should_manage_visibility shows and hides the page action via
// PageActionController without modifying ActionItem::GetIsShowingBubble() when
// should_sync_bubble_showing_state is false.
TEST_F(AutofillBubbleControllerBaseTest,
       ManageVisibilityOnly_ShowsAndHidesPageAction) {
  FakeAutofillBubble bubble;
  TestBubbleController controller(
      &tab_interface(), &bubble, BubbleType::kGmailOtpOptIn,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionGmailOtpOptIn, .should_manage_visibility = true});

  EXPECT_CALL(page_action_controller(), Show(kActionGmailOtpOptIn));
  controller.QueueOrShowBubble();
  EXPECT_TRUE(controller.IsShowingBubble());
  EXPECT_FALSE(managed_action_item().GetIsShowingBubble());

  EXPECT_CALL(page_action_controller(), Hide(kActionGmailOtpOptIn));
  controller.CloseBubble();
  EXPECT_FALSE(controller.IsShowingBubble());
  EXPECT_FALSE(managed_action_item().GetIsShowingBubble());
}

// Tests that should_sync_bubble_showing_state updates
// ActionItem::GetIsShowingBubble() without calling PageActionController when
// should_manage_visibility is false.
TEST_F(AutofillBubbleControllerBaseTest,
       SyncBubbleShowingStateOnly_UpdatesActionItemWithoutPageActionCalls) {
  FakeAutofillBubble bubble;
  TestBubbleController controller(
      &tab_interface(), &bubble, BubbleType::kWalletReminderNotice,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionWalletReminderNotice,
          .should_sync_bubble_showing_state = true});

  EXPECT_CALL(page_action_controller(), Show).Times(0);
  EXPECT_CALL(page_action_controller(), Hide).Times(0);

  controller.QueueOrShowBubble();
  EXPECT_TRUE(controller.IsShowingBubble());
  EXPECT_TRUE(synced_action_item().GetIsShowingBubble());

  controller.CloseBubble();
  EXPECT_FALSE(controller.IsShowingBubble());
  EXPECT_FALSE(synced_action_item().GetIsShowingBubble());
}

// Tests that enabling both should_manage_visibility and
// should_sync_bubble_showing_state updates both PageActionController and
// ActionItem::GetIsShowingBubble().
TEST_F(AutofillBubbleControllerBaseTest,
       ManageVisibilityAndSyncBubbleShowingState_UpdatesBoth) {
  FakeAutofillBubble bubble;
  TestBubbleController controller(
      &tab_interface(), &bubble, BubbleType::kGmailOtpOptIn,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionGmailOtpOptIn,
          .should_manage_visibility = true,
          .should_sync_bubble_showing_state = true});

  EXPECT_CALL(page_action_controller(), Show(kActionGmailOtpOptIn));
  controller.QueueOrShowBubble();
  EXPECT_TRUE(controller.IsShowingBubble());
  EXPECT_TRUE(managed_action_item().GetIsShowingBubble());

  EXPECT_CALL(page_action_controller(), Hide(kActionGmailOtpOptIn));
  controller.CloseBubble();
  EXPECT_FALSE(controller.IsShowingBubble());
  EXPECT_FALSE(managed_action_item().GetIsShowingBubble());
}

// Tests that destroying a showing controller resets
// ActionItem::GetIsShowingBubble() to false.
TEST_F(AutofillBubbleControllerBaseTest,
       SyncBubbleShowingState_ResetsActionItemOnDestruction) {
  FakeAutofillBubble bubble;
  auto controller = std::make_unique<TestBubbleController>(
      &tab_interface(), &bubble, BubbleType::kWalletReminderNotice,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionWalletReminderNotice,
          .should_sync_bubble_showing_state = true});

  controller->QueueOrShowBubble();
  ASSERT_TRUE(synced_action_item().GetIsShowingBubble());

  controller.reset();
  EXPECT_FALSE(synced_action_item().GetIsShowingBubble());
}

// Tests that deactivating a tab hides its active bubble and resets the
// original window's ActionItem::GetIsShowingBubble() before the tab detaches.
TEST_F(AutofillBubbleControllerBaseTest,
       SyncBubbleShowingState_ResetsActionItemOnTabDeactivate) {
  FakeAutofillBubble bubble;
  TestBubbleController controller(
      &tab_interface(), &bubble, BubbleType::kWalletReminderNotice,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionWalletReminderNotice,
          .should_sync_bubble_showing_state = true});

  controller.QueueOrShowBubble();
  ASSERT_TRUE(synced_action_item().GetIsShowingBubble());

  DeactivateTab();
  EXPECT_FALSE(controller.IsShowingBubble());
  EXPECT_FALSE(synced_action_item().GetIsShowingBubble());
}

// Tests that clearing observed WebContents on a WebContents-constructed
// controller before close or destruction does not crash in GetTabInterface().
TEST_F(AutofillBubbleControllerBaseTest,
       WebContentsConstructor_HandlesClearedWebContentsOnTeardown) {
  FakeAutofillBubble bubble;
  auto controller = std::make_unique<TestBubbleController>(
      web_contents(), &bubble, BubbleType::kWalletReminderNotice,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionWalletReminderNotice,
          .should_manage_visibility = true,
          .should_sync_bubble_showing_state = true});

  controller->QueueOrShowBubble();
  ASSERT_TRUE(controller->IsShowingBubble());

  controller->ClearObservedWebContents();
  controller.reset();
}

// Tests that hiding a bubble resets ActionItem::GetIsShowingBubble() before
// BubbleManager synchronously shows a queued bubble sharing the same ActionId.
TEST_F(AutofillBubbleControllerBaseTest,
       SyncBubbleShowingState_PreservesStateForQueuedNextBubble) {
  FakeAutofillBubble first_bubble;
  FakeAutofillBubble queued_bubble;
  TestBubbleController first_controller(
      &tab_interface(), &first_bubble, BubbleType::kSaveUpdateCard,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionWalletReminderNotice,
          .should_sync_bubble_showing_state = true});
  TestBubbleController queued_controller(
      &tab_interface(), &queued_bubble, BubbleType::kWalletReminderNotice,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionWalletReminderNotice,
          .should_sync_bubble_showing_state = true});

  first_controller.QueueOrShowBubble();
  queued_controller.QueueOrShowBubble();
  ASSERT_TRUE(first_controller.IsShowingBubble());
  ASSERT_FALSE(queued_controller.IsShowingBubble());
  ASSERT_TRUE(synced_action_item().GetIsShowingBubble());

  first_controller.CloseBubble();
  EXPECT_FALSE(first_controller.IsShowingBubble());
  EXPECT_TRUE(queued_controller.IsShowingBubble());
  EXPECT_TRUE(synced_action_item().GetIsShowingBubble());
}

// Tests that a PageActionConfig with both flags false touches neither
// PageActionController nor ActionItem.
TEST_F(AutofillBubbleControllerBaseTest, DefaultFlags_NoOpOnPageActions) {
  FakeAutofillBubble bubble;
  TestBubbleController controller(
      &tab_interface(), &bubble, BubbleType::kSaveUpdateAutofillAi,
      AutofillBubbleControllerBase::PageActionConfig{
          .action_id = kActionWalletReminderNotice});

  EXPECT_CALL(page_action_controller(), Show).Times(0);
  EXPECT_CALL(page_action_controller(), Hide).Times(0);

  controller.QueueOrShowBubble();
  EXPECT_TRUE(controller.IsShowingBubble());
  EXPECT_FALSE(managed_action_item().GetIsShowingBubble());
  EXPECT_FALSE(synced_action_item().GetIsShowingBubble());

  controller.CloseBubble();
  EXPECT_FALSE(controller.IsShowingBubble());
}

}  // namespace
}  // namespace autofill
