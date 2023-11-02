// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/media_router/app_menu_test_api.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

namespace {

class AppMenuTestApiViews : public test::AppMenuTestApi {
 public:
  explicit AppMenuTestApiViews(Browser* browser);

  AppMenuTestApiViews(const AppMenuTestApiViews&) = delete;
  AppMenuTestApiViews& operator=(const AppMenuTestApiViews&) = delete;

  ~AppMenuTestApiViews() override;

  // AppMenuTestApi:
  bool IsMenuShowing() override;
  void ShowMenu() override;
  void ExecuteCommand(int command) override;

 private:
  BrowserAppMenuButton* GetAppMenuButton();
  AppMenu* GetAppMenu();

  raw_ptr<Browser> browser_;
};

AppMenuTestApiViews::AppMenuTestApiViews(Browser* browser)
    : browser_(browser) {}
AppMenuTestApiViews::~AppMenuTestApiViews() {}

bool AppMenuTestApiViews::IsMenuShowing() {
  return GetAppMenuButton()->IsMenuShowing();
}

void AppMenuTestApiViews::ShowMenu() {
  GetAppMenuButton()->ShowMenu(views::MenuRunner::NO_FLAGS);
}

void AppMenuTestApiViews::ExecuteCommand(int command) {
  // TODO(ellyjones): This doesn't behave properly for nested menus.
  GetAppMenu()->ExecuteCommand(command, 0);
}

BrowserAppMenuButton* AppMenuTestApiViews::GetAppMenuButton() {
  return BrowserView::GetBrowserViewForBrowser(browser_)
      ->toolbar()
      ->app_menu_button();
}

AppMenu* AppMenuTestApiViews::GetAppMenu() {
  return GetAppMenuButton()->app_menu();
}

}  // namespace

namespace test {

std::unique_ptr<AppMenuTestApi> AppMenuTestApi::Create(Browser* browser) {
  return std::make_unique<AppMenuTestApiViews>(browser);
}

}  // namespace test
