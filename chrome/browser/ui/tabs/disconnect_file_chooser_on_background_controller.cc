// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/disconnect_file_chooser_on_background_controller.h"

#include "base/functional/bind.h"
#include "content/public/browser/web_contents.h"

namespace tabs {

DisconnectFileChooserOnBackgroundController::
    DisconnectFileChooserOnBackgroundController(TabInterface& tab)
    : will_enter_background_subscription_(
          tab.RegisterWillEnterBackground(base::BindRepeating(
              &DisconnectFileChooserOnBackgroundController::WillEnterBackground,
              base::Unretained(this)))) {}

DisconnectFileChooserOnBackgroundController::
    ~DisconnectFileChooserOnBackgroundController() = default;

void DisconnectFileChooserOnBackgroundController::WillEnterBackground(
    TabInterface* tab) {
  content::WebContents* web_contents = tab->GetContents();
  if (web_contents) {
    web_contents->DisconnectFileSelectListenerIfAny();
  }
}

}  // namespace tabs
