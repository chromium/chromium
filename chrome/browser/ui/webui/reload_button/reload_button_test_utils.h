// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_TEST_UTILS_H_

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"

// Mock implementation of the reload_button::mojom::Page interface.
class MockReloadButtonPage : public reload_button::mojom::Page {
 public:
  MockReloadButtonPage();
  ~MockReloadButtonPage() override;

  MockReloadButtonPage(const MockReloadButtonPage&) = delete;
  MockReloadButtonPage& operator=(const MockReloadButtonPage&) = delete;

  // Returns a PendingRemote to this mock implementation.
  mojo::PendingRemote<reload_button::mojom::Page> BindAndGetRemote();

  void FlushForTesting();

  // reload_button::mojom::Page:
  MOCK_METHOD(void,
              SetReloadButtonState,
              (bool is_loading, bool is_menu_enabled),
              (override));

 private:
  mojo::Receiver<reload_button::mojom::Page> receiver_{this};
};

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

#endif  // CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_TEST_UTILS_H_
