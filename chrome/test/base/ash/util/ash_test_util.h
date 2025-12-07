// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_UTIL_ASH_TEST_UTIL_H_
#define CHROME_TEST_BASE_ASH_UTIL_ASH_TEST_UTIL_H_

#include <string_view>
#include <vector>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/events/event_constants.h"

class Browser;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace views {
class View;
}  // namespace views

namespace ash::test {

// Performs a click on `view` with optional `flags`.
void Click(const views::View* view, int flags = ui::EF_NONE);

// Creates a file at the root of the downloads mount point with the specified
// `extension`. The default extension is "txt". Returns the path of the created
// file.
base::FilePath CreateFile(Profile* profile, std::string_view extension = "txt");

// Moves mouse to `view` over `count` number of events. `count` is 1 by default.
void MoveMouseTo(const views::View* view, size_t count = 1u);

void InstallSystemAppsForTesting(Profile* profile);

// Creates a system web app window (os settings, camera, files, etc.). Note that
// a test needs to call `InstallSystemWebAppsForTesting()` prior to using this.
webapps::AppId CreateSystemWebApp(
    Profile* profile,
    ash::SystemWebAppType app_type,
    std::optional<int32_t> window_id = std::nullopt);

// Creates a browser and tabs with given `urls`. The active tab is indicated by
// `active_url_index`. The browser is not shown after creation.
Browser* CreateBrowser(Profile* profile,
                       const std::vector<GURL>& urls,
                       std::optional<size_t> active_url_index);

Browser* CreateAndShowBrowser(
    Profile* profile,
    const std::vector<GURL>& urls,
    std::optional<size_t> active_url_index = std::nullopt);

Browser* InstallAndLaunchPWA(Profile* profile,
                             const GURL& start_url,
                             bool launch_in_browser,
                             const std::u16string& app_title = u"A Web App");

// Class used to wait for multiple browser windows to be created.
class BrowsersWaiter : public BrowserListObserver {
 public:
  explicit BrowsersWaiter(int expected_count);
  BrowsersWaiter(const BrowsersWaiter&) = delete;
  BrowsersWaiter& operator=(const BrowsersWaiter&) = delete;
  ~BrowsersWaiter() override;

  void Wait();

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

 private:
  int current_count_ = 0;
  const int expected_count_;
  base::RunLoop run_loop_;
};

}  // namespace ash::test

#endif  // CHROME_TEST_BASE_ASH_UTIL_ASH_TEST_UTIL_H_
