// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_CONTROL_H_

#include "ui/menus/simple_menu_model.h"
#include "ui/views/view.h"

// TODO(crbug.com/444358999): maybe rename this to `ReloadButton`, and the two
// implementations become `ReloadButtonImpl` / `ReloadButtonWebViewImpl`.
class ReloadControl : public ui::SimpleMenuModel::Delegate {
 public:
  // The mode indicates whether the button should be used to reload the page or
  // stop the loading.
  enum class Mode { kReload = 0, kStop };

  ~ReloadControl() override = default;

  // Ask for a specified button state. If `force` is true this will be applied
  // immediately.
  virtual void ChangeMode(Mode mode, bool force) = 0;

  // Gets/Sets whether reload drop-down menu is enabled.
  virtual bool GetDevToolsStatusForTesting() const = 0;
  virtual void SetDevToolsStatus(bool is_dev_tools_connected) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_CONTROL_H_
