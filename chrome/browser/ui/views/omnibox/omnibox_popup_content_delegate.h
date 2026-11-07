// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENT_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENT_DELEGATE_H_

#include <memory>

#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "ui/gfx/geometry/size.h"

// An interface representing a deactivation blocker. Keeping an instance of this
// interface alive prevents the omnibox popup widget from closing when it loses
// focus.
class OmniboxPopupDeactivationBlocker {
 public:
  virtual ~OmniboxPopupDeactivationBlocker() = default;
};

// What OmniboxPopupWebUIBaseContent and its subclasses need from the object
// that shows them: whether the popup is shown and takes focus, plus lifecycle
// callbacks for Escape and file dialogs. It is also the
// SearchboxHandler::Delegate that the content binds to its WebUI controller.
// The content reads OmniboxPopupPresenterDelegate from its LocationBar, not
// from this interface.
class OmniboxPopupContentDelegate : public SearchboxHandler::Delegate {
 public:
  virtual ~OmniboxPopupContentDelegate() = default;

  // Returns whether the popup is currently shown.
  virtual bool IsShown() const = 0;

  // Returns true if the WebUI takes keyboard focus while shown (Full and AI
  // Mode popups). The classic popup returns false, because focus stays in the
  // Views omnibox and its widget isn't activatable. The Full popup also returns
  // false while a menu is open.
  virtual bool ShouldReceiveFocus() const = 0;

  // Notifies the delegate that Escape was pre-handled by the WebUI content.
  virtual void NotifyEscapeKeyPressed() = 0;

  // Creates a scoped blocker that prevents the popup from closing on
  // deactivation while an OS dialog (e.g. file chooser) is open. May return
  // nullptr if deactivation blocking is disabled or not applicable.
  virtual std::unique_ptr<OmniboxPopupDeactivationBlocker>
  CreateDeactivationBlocker() = 0;

  // Triggered when a file selection dialog opened by this popup is closed.
  virtual void OnFileSelectionClosed() = 0;
};

// Optional sizing collaborator for popups whose widget bounds track the
// WebUI's auto-resized content height.
class OmniboxPopupContentSizer {
 public:
  virtual ~OmniboxPopupContentSizer() = default;

  virtual const gfx::Size& GetPopupMinimumSize() const = 0;
  virtual void OnContentHeightChanged(int content_height) = 0;
  virtual void SynchronizePopupBounds() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENT_DELEGATE_H_
