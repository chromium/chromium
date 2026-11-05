// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/data_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/actions.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"

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
  CustomizeToolbarHandlerBrowserTest() {
    // ChromeOS creates `TtcKeyedService` only where Glic's system requirements
    // are met.
    glic::GlicEnabling::SetSystemRequirementMetForTesting(true);
  }
  ~CustomizeToolbarHandlerBrowserTest() override {
    glic::GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    handler_ = std::make_unique<CustomizeToolbarHandler>(
        mojo::PendingReceiver<
            side_panel::customize_chrome::mojom::CustomizeToolbarHandler>(),
        mock_page_.BindAndGetRemote(),
        browser()->GetActiveTabInterface()->GetContents());
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_{ttc::kTtc};
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
  EXPECT_TRUE(
      contains_action(side_panel::customize_chrome::mojom::ActionId::kTtc));
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

IN_PROC_BROWSER_TEST_F(CustomizeToolbarHandlerBrowserTest,
                       IconSizeDeltaPadsListedIcon) {
  constexpr auto kMojoActionId =
      side_panel::customize_chrome::mojom::ActionId::kPrint;
  constexpr int kToolbarIconSize = 20;
  constexpr int kIconSizeDelta = -4;

  const auto list_icon_bitmap = [this, kMojoActionId]() -> SkBitmap {
    std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;
    base::MockCallback<CustomizeToolbarHandler::ListActionsCallback> callback;
    EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&actions));
    handler().ListActions(callback.Get());
    const auto it =
        std::ranges::find(actions, kMojoActionId,
                          &side_panel::customize_chrome::mojom::Action::id);
    if (it == actions.end()) {
      ADD_FAILURE() << "Action not listed";
      return SkBitmap();
    }
    std::string mime_type;
    std::string charset;
    std::string data;
    if (!net::DataURL::Parse((*it)->icon_url, &mime_type, &charset, &data)) {
      ADD_FAILURE() << "Invalid icon data URL";
      return SkBitmap();
    }
    return gfx::PNGCodec::Decode(base::as_byte_span(data));
  };

  // Returns true if every pixel in the outer `border` ring of `bitmap` is fully
  // transparent.
  const auto has_transparent_border = [](const SkBitmap& bitmap, int border) {
    for (int y = 0; y < bitmap.height(); ++y) {
      for (int x = 0; x < bitmap.width(); ++x) {
        const bool in_border = x < border || y < border ||
                               x >= bitmap.width() - border ||
                               y >= bitmap.height() - border;
        if (in_border && SkColorGetA(bitmap.getColor(x, y)) != 0) {
          return false;
        }
      }
    }
    return true;
  };

  // The test runs at a 1x scale factor, so bitmap dimensions are in dp.
  ASSERT_EQ(display::Screen::Get()->GetPrimaryDisplay().device_scale_factor(),
            1.0f);

  // Without a delta, the icon is rasterized at its intrinsic vector icon size
  // rather than padded out to the toolbar icon size.
  const SkBitmap default_bitmap = list_icon_bitmap();
  ASSERT_FALSE(default_bitmap.isNull());
  EXPECT_LT(default_bitmap.width(), kToolbarIconSize);
  EXPECT_LT(default_bitmap.height(), kToolbarIconSize);

  // With a negative delta, the icon is drawn smaller and centered on a
  // transparent canvas of the full toolbar icon size, so the WebUI (which
  // scales every icon to the same box) shows it at the same relative size as
  // the toolbar.
  actions::ActionItem* const print_action_item =
      actions::ActionManager::Get().FindAction(
          kActionPrint, BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(print_action_item);
  print_action_item->SetProperty(chrome::kActionItemIconSizeDeltaKey,
                                 kIconSizeDelta);

  const SkBitmap padded_bitmap = list_icon_bitmap();
  ASSERT_FALSE(padded_bitmap.isNull());
  EXPECT_EQ(padded_bitmap.width(), kToolbarIconSize);
  EXPECT_EQ(padded_bitmap.height(), kToolbarIconSize);
  EXPECT_TRUE(has_transparent_border(padded_bitmap, -kIconSizeDelta / 2));
}
