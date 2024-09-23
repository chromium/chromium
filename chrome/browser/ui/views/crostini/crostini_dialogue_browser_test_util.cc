// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class WebContentsWaiter : public content::WebContentsObserver {
 public:
  enum Operation { LOAD };  // Add other operations as required.
  explicit WebContentsWaiter(content::WebContents* contents,
                             Operation operation)
      : content::WebContentsObserver(contents), operation_(operation) {}

  ~WebContentsWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (operation_ == LOAD) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  Operation operation_;
};

void CrostiniDialogBrowserTest::WaitForLoadFinished(
    content::WebContents* contents) {
  WebContentsWaiter(contents, WebContentsWaiter::LOAD).Wait();
}
