// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_HOST_H_

#include <memory>

class OmniboxPopupDeactivationBlocker;
class OmniboxPopupWebUIBaseContent;

// Where an omnibox popup hosts its WebUI. Implemented by the popup presenters,
// which show the WebUI in a widget anchored to the location bar. An
// OmniboxPopupViewWebUI owns its host; the location bar owns the AI Mode
// popup's presenter. This interface lives in its own header so
// that hosts that aren't presenters don't depend on the presenter header.
class OmniboxPopupHost {
 public:
  virtual ~OmniboxPopupHost() = default;

  // Shows the WebUI.
  virtual void Show() = 0;
  // Hides the WebUI.
  virtual void Hide() = 0;
  // Moves keyboard focus into the WebUI.
  virtual void RequestFocus() = 0;
  // Returns whether the WebUI is shown.
  virtual bool IsShown() const = 0;
  // Returns the WebUI content view. May return null before the content is
  // created.
  virtual OmniboxPopupWebUIBaseContent* GetWebUIContent() = 0;

  // Called when the omnibox of the window that hosts the popup gains focus.
  // Lets a host that creates its WebUI content on demand start loading it.
  virtual void OnOmniboxFocused() = 0;

  // Called when an embedded permission prompt starts or stops showing while
  // the WebUI is shown, so that the host can ignore the focus loss the prompt
  // causes.
  virtual void SetPermissionPromptShowing(bool showing) = 0;

  // Returns whether the host must stay open when the user clicks outside it,
  // e.g. while a file dialog or a permission prompt is showing.
  virtual bool IsClosePrevented() const = 0;

  // Creates a scoped blocker that keeps the WebUI open on deactivation while
  // an OS dialog (e.g. a file chooser) is open. May return null if
  // deactivation blocking is disabled or not applicable.
  virtual std::unique_ptr<OmniboxPopupDeactivationBlocker>
  CreateDeactivationBlocker() = 0;

  // Returns whether the WebUI page has completed its first visually non-empty
  // paint. LocationBarView drops its native focus ring under the Full popup
  // once this is true.
  virtual bool CompletedFirstVisuallyNonEmptyPaint() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_HOST_H_
