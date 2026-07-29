// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include <memory>
#include <vector>

#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace {

using testing::_;
using testing::SaveArg;

class MockPage
    : public side_panel::customize_chrome::mojom::CustomizeToolbarClient {
 public:
  MockPage() = default;

  mojo::PendingRemote<
      side_panel::customize_chrome::mojom::CustomizeToolbarClient>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              SetActionPinned,
              (side_panel::customize_chrome::mojom::ActionId action_id,
               bool pinned),
              (override));
  MOCK_METHOD(void, NotifyActionsUpdated, (), (override));

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<side_panel::customize_chrome::mojom::CustomizeToolbarClient>
      receiver_{this};
};

}  // namespace

class CustomizeToolbarHandlerBrowserTest : public InProcessBrowserTest {
 public:
  CustomizeToolbarHandlerBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    handler_ = std::make_unique<CustomizeToolbarHandler>(
        mojo::PendingReceiver<
            side_panel::customize_chrome::mojom::CustomizeToolbarHandler>(),
        mock_page_.BindAndGetRemote(),
        browser()->tab_strip_model()->GetActiveWebContents());
    mock_page_.FlushForTesting();

    auto* const template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  }

  void TearDownOnMainThread() override {
    handler_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  CustomizeToolbarHandler& handler() { return *handler_; }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  std::unique_ptr<CustomizeToolbarHandler> handler_;
};

IN_PROC_BROWSER_TEST_F(CustomizeToolbarHandlerBrowserTest, ListActions) {
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;
  base::MockCallback<CustomizeToolbarHandler::ListActionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&actions));
  handler().ListActions(callback.Get());

  const auto contains_action =
      [&actions](side_panel::customize_chrome::mojom::ActionId id) -> bool {
    return std::find_if(
               actions.begin(), actions.end(),
               [id](side_panel::customize_chrome::mojom::ActionPtr& action) {
                 return action->id == id;
               }) != actions.end();
  };

  // 11 actions are currently pinnable; more should be pinnable in the future.
  EXPECT_GE(actions.size(), 11u);

  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowBookmarks));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowReadAnything));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowReadingList));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowLensOverlay));
  EXPECT_TRUE(
      contains_action(side_panel::customize_chrome::mojom::ActionId::kHome));
  EXPECT_TRUE(
      contains_action(side_panel::customize_chrome::mojom::ActionId::kForward));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kNewIncognitoWindow));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowPasswordManager));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowPaymentMethods));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowAddresses));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kClearBrowsingData));
  EXPECT_TRUE(
      contains_action(side_panel::customize_chrome::mojom::ActionId::kPrint));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowTranslate));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kSendTabToSelf));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kQrCodeGenerator));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kTaskManager));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kDevTools));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kSplitTab));
}

IN_PROC_BROWSER_TEST_F(CustomizeToolbarHandlerBrowserTest,
                       ActionsUpdatedOnVisibilityChange) {
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;
  base::MockCallback<CustomizeToolbarHandler::ListActionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&actions));
  handler().ListActions(callback.Get());

  const auto contains_action =
      [&actions](side_panel::customize_chrome::mojom::ActionId id) -> bool {
    return std::find_if(
               actions.begin(), actions.end(),
               [id](side_panel::customize_chrome::mojom::ActionPtr& action) {
                 return action->id == id;
               }) != actions.end();
  };

  // Devtools is initially present in the actions list.
  ASSERT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kDevTools));

  // Set visibility of devtools to false, and...
  actions::ActionItem* const scope_action =
      BrowserActions::From(browser())->root_action_item();
  actions::ActionItem* const devtools_action_item =
      actions::ActionManager::Get().FindAction(kActionDevTools, scope_action);

  // The WebUI client is notified, and...
  EXPECT_CALL(mock_page_, NotifyActionsUpdated).Times(1);
  devtools_action_item->SetVisible(false);

  // Devtools is now absent from the actions list.
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&actions));
  handler().ListActions(callback.Get());
  EXPECT_FALSE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kDevTools));
}
