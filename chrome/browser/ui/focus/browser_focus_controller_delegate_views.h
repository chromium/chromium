// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_DELEGATE_VIEWS_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/focus/browser_focus_controller.h"
#include "ui/base/interaction/element_identifier.h"

class BrowserElements;
class Profile;
class ToolbarButtonProvider;

namespace views {
class View;
}

class BrowserFocusControllerDelegateViews
    : public BrowserFocusController::Delegate {
 public:
  BrowserFocusControllerDelegateViews(
      Profile* profile,
      BrowserElements* browser_elements,
      ToolbarButtonProvider* toolbar_button_provider);
  ~BrowserFocusControllerDelegateViews() override;

  BrowserFocusControllerDelegateViews(
      const BrowserFocusControllerDelegateViews&) = delete;
  BrowserFocusControllerDelegateViews& operator=(
      const BrowserFocusControllerDelegateViews&) = delete;

  // BrowserFocusController::Delegate:
  void FocusWebContentsPane() override;
  void FocusInactivePopupForAccessibility() override;
  bool ActivateFirstInactiveBubbleForAccessibility() override;

 private:
  views::View* GetViewForId(ui::ElementIdentifier element_id);

  const raw_ref<Profile> profile_;
  const raw_ref<BrowserElements> browser_elements_;
  const raw_ref<ToolbarButtonProvider> toolbar_button_provider_;
};

#endif  // CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_DELEGATE_VIEWS_H_
