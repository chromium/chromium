// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_FAKE_TAB_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_FAKE_TAB_INTERFACE_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"

namespace content {
class WebContents;
}

namespace page_actions {

class FakeTabInterface : public tabs::MockTabInterface {
 public:
  explicit FakeTabInterface(content::WebContents* contents);
  ~FakeTabInterface() override;

  // tabs::MockTabInterface
  bool IsActivated() const override;
  base::CallbackListSubscription RegisterDidActivate(
      base::RepeatingCallback<void(TabInterface*)> cb) override;
  base::CallbackListSubscription RegisterWillDeactivate(
      base::RepeatingCallback<void(TabInterface*)> cb) override;

  void Activate();
  void Deactivate();

 private:
  raw_ptr<content::WebContents> contents_;
  bool is_activated_ = false;
  base::RepeatingCallbackList<void(TabInterface*)> activation_callbacks_;
  base::RepeatingCallbackList<void(TabInterface*)> deactivation_callbacks_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_FAKE_TAB_INTERFACE_H_
