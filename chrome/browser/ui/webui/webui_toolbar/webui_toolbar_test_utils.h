// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_TEST_UTILS_H_

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"

// Mock implementation of the
// toolbar_ui_api::mojom::ToolbarUIObserver interface.
class MockReloadButtonPage : public toolbar_ui_api::mojom::ToolbarUIObserver {
 public:
  MockReloadButtonPage();
  ~MockReloadButtonPage() override;

  MockReloadButtonPage(const MockReloadButtonPage&) = delete;
  MockReloadButtonPage& operator=(const MockReloadButtonPage&) = delete;

  // Returns a PendingRemote to this mock implementation.
  mojo::PendingRemote<toolbar_ui_api::mojom::ToolbarUIObserver>
  BindAndGetRemote();

  void Bind(
      mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIObserver> receiver);

  void FlushForTesting();

  // toolbar_ui_api::mojom::ToolbarUIObserver:
  MOCK_METHOD(void,
              OnNavigationControlsStateChanged,
              (toolbar_ui_api::mojom::NavigationControlsStatePtr state),
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
               gfx::Point,
               ui::mojom::MenuSourceType),
              (override));
  MOCK_METHOD(void, OnPageInitialized, (), (override));
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

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_TEST_UTILS_H_
