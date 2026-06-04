// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_TEST_UTILS_H_

#include <ostream>

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/icon_table_fetcher.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect_f.h"

// Mock implementation of the
// toolbar_ui_api::mojom::ToolbarUIObserver interface.
class MockToolbarUIObserver : public toolbar_ui_api::mojom::ToolbarUIObserver {
 public:
  MockToolbarUIObserver();
  ~MockToolbarUIObserver() override;

  MockToolbarUIObserver(const MockToolbarUIObserver&) = delete;
  MockToolbarUIObserver& operator=(const MockToolbarUIObserver&) = delete;

  // Returns a PendingRemote to this mock implementation.
  mojo::PendingRemote<toolbar_ui_api::mojom::ToolbarUIObserver>
  BindAndGetRemote();

  void Bind(
      mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIObserver> receiver);

  void FlushForTesting();

  // toolbar_ui_api::mojom::ToolbarUIObserver:
  MOCK_METHOD(void,
              OnNavigationControlsStateChanged,
              (std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
               toolbar_ui_api::mojom::NavigationControlsStatePtr state),
              (override));

  MOCK_METHOD(void,
              OnFocusRequested,
              (toolbar_ui_api::mojom::FocusRequestTarget target),
              (override));

 private:
  mojo::Receiver<toolbar_ui_api::mojom::ToolbarUIObserver> receiver_{this};
};

class MockToolbarUIServiceDelegate
    : public toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate {
 public:
  MockToolbarUIServiceDelegate();
  ~MockToolbarUIServiceDelegate() override;

  // ToolbarUIService::ToolbarUIServiceDelegate:
  MOCK_METHOD(void,
              HandleContextMenu,
              (toolbar_ui_api::mojom::ContextMenuType,
               const gfx::RectF&,
               ui::mojom::MenuSourceType),
              (override));
  MOCK_METHOD(void,
              ShowContentSettingsBubble,
              (::toolbar_ui_api::mojom::ContentSettingImageType type,
               ::toolbar_ui_api::mojom::ToolbarUIService::
                   ShowContentSettingsBubbleCallback callback),
              (override));
  MOCK_METHOD(void, OnPageInitialized, (), (override));
  MOCK_METHOD(void,
              InvokePinnedToolbarAction,
              (toolbar_ui_api::mojom::PinnedToolbarAction action_id),
              (override));
  MOCK_METHOD(void,
              OnLhsChipMousePressed,
              (toolbar_ui_api::mojom::LhsChipIdentifier),
              (override));
  MOCK_METHOD(void,
              OnLhsChipClicked,
              (toolbar_ui_api::mojom::LhsChipIdentifier, bool),
              (override));
  MOCK_METHOD(void,
              OnLhsChipPointerEntered,
              (toolbar_ui_api::mojom::LhsChipIdentifier),
              (override));
  MOCK_METHOD(void,
              OnLhsChipPointerExited,
              (toolbar_ui_api::mojom::LhsChipIdentifier),
              (override));
  MOCK_METHOD(void,
              OnLhsChipExpandAnimationEnded,
              (toolbar_ui_api::mojom::LhsChipIdentifier),
              (override));
  MOCK_METHOD(void,
              OnLhsChipCollapseAnimationEnded,
              (toolbar_ui_api::mojom::LhsChipIdentifier),
              (override));
  MOCK_METHOD(void,
              OnLhsChipDrag,
              (toolbar_ui_api::mojom::LhsChipIdentifier,
               ui::mojom::DragEventSource),
              (override));
  MOCK_METHOD(void, OnHomeButtonDropUrl, (const GURL&), (override));
  MOCK_METHOD(void, OnHomeButtonDropFile, (const gfx::PointF&), (override));
  MOCK_METHOD(void, OnToolbarDropFile, (const gfx::PointF&), (override));

  MOCK_METHOD((base::expected<std::monostate, mojo_base::mojom::ErrorPtr>),
              OnOmniboxAction,
              (toolbar_ui_api::mojom::OmniboxActionPtr action_ptr),
              (override));
  MOCK_METHOD(void, ShowAvatarMenu, (), (override));
};

class MockBrowserControlsServiceDelegate
    : public browser_controls_api::BrowserControlsService::
          BrowserControlsServiceDelegate {
 public:
  MockBrowserControlsServiceDelegate();
  ~MockBrowserControlsServiceDelegate() override;

  // BrowserControlsService::BrowserControlsServiceDelegate:
  MOCK_METHOD(void, PermitLaunchUrl, (), (override));
};

// Helper to create a valid NavigationControlsState with initialized fields.
toolbar_ui_api::mojom::NavigationControlsStatePtr
CreateValidNavigationControlsState();

// Mock implementation of CommandUpdater for testing.
class MockCommandUpdater : public CommandUpdater {
 public:
  MockCommandUpdater();
  ~MockCommandUpdater() override;

  MOCK_METHOD(bool, SupportsCommand, (int id), (const, override));
  MOCK_METHOD(bool, IsCommandEnabled, (int id), (const, override));
  MOCK_METHOD(bool,
              ExecuteCommand,
              (int id, base::TimeTicks time_stamp),
              (override));
  MOCK_METHOD(bool,
              ExecuteCommandWithDisposition,
              (int id,
               WindowOpenDisposition disposition,
               base::TimeTicks time_stamp),
              (override));
  MOCK_METHOD(void,
              AddCommandObserver,
              (int id, CommandObserver* observer),
              (override));
  MOCK_METHOD(void,
              RemoveCommandObserver,
              (int id, CommandObserver* observer),
              (override));
  MOCK_METHOD(void,
              RemoveCommandObserver,
              (CommandObserver * observer),
              (override));
  MOCK_METHOD(bool, UpdateCommandEnabled, (int id, bool state), (override));
};

MATCHER_P(MatchesIconUpdate, expected, "") {
  const toolbar_ui_api::mojom::IconUpdatePtr& update = arg;
  return testing::ExplainMatchResult(expected.get()->handle_id,
                                     update->handle_id, result_listener) &&
         testing::ExplainMatchResult(expected.get()->icon_url_or_name,
                                     update->icon_url_or_name,
                                     result_listener) &&
         testing::ExplainMatchResult(expected.get()->icon_type,
                                     update->icon_type, result_listener) &&
         testing::ExplainMatchResult(expected.get()->color, update->color,
                                     result_listener);
}

// Tests that an update is a PNG data URL for given handle id.
MATCHER_P(MatchesBitmapIconUpdate, handle_id, "") {
  const toolbar_ui_api::mojom::IconUpdatePtr& update = arg;
  return testing::ExplainMatchResult(handle_id, update->handle_id,
                                     result_listener) &&
         testing::ExplainMatchResult(
             testing::Optional(testing::StartsWith("data:image/png;base64")),
             update->icon_url_or_name, result_listener) &&
         testing::ExplainMatchResult(
             toolbar_ui_api::mojom::IconType::kFullColorUrl, update->icon_type,
             result_listener) &&
         testing::ExplainMatchResult(update->color, std::nullopt,
                                     result_listener);
}

// Pretty printer for gTest. It needs to be in mojo namespace to be found;
// note that this is secretly a mojo::InlineStructPtr.
namespace mojo {

std::ostream& operator<<(
    std::ostream& out,
    const toolbar_ui_api::mojom::IconUpdatePtr& icon_update);

}  // namespace mojo

class FakeIconTableFetcher : public toolbar_ui_api::IconTableFetcher {
 public:
  FakeIconTableFetcher();
  ~FakeIconTableFetcher() override;

  void AddUpdate(toolbar_ui_api::mojom::IconUpdatePtr update);

  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> GetFullState() override;
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> TakePendingUpdates()
      override;

 private:
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> all_updates_;
  std::vector<toolbar_ui_api::mojom::IconUpdatePtr> pending_updates_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_TEST_UTILS_H_
