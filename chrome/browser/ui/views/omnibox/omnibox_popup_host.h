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
  // Returns whether the browser window or its popup widget is active.
  virtual bool IsBrowserOrPopupActive() = 0;
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

  // Called on a tab switch that is about to show the WebUI, before `Show()`.
  // Lets a host that sizes itself to the WebUI's content drop the previous
  // tab's content height while the WebUI updates for the new tab.
  // TODO(crbug.com/585199097): Move to the content's OmniboxPopupContentSizer.
  virtual void ResetContentHeightForTabSwitch() = 0;

  // Blurs the omnibox and closes the WebUI unless it holds a draft, leaving
  // nothing focused. Called by `OmniboxPopupView::Blur()`.
  virtual void DeactivateAndKillFocus() = 0;

  // Called when a native file dialog opened from the popup's "+" menu is shown
  // and closed, respectively. `file_selected` is whether the user selected
  // files, which opens AI Mode with them.
  virtual void OnFileDialogOpened() {}
  virtual void OnFileDialogClosed(bool file_selected) {}

  // Called when the active tab changes. Forgets any state recorded for a modal
  // dialog, which belongs to the previous tab's omnibox.
  virtual void OnTabChanged() {}

  // Called by `OmniboxPopupView::ReshowPopup()` before it reopens the popup
  // for a draft. Returns true if the host can't show the popup yet, in which
  // case the host calls `ReshowPopup()` again once it can.
  // TODO(crbug.com/588536651): Remove once NativeWidgetNSWindowBridge no
  // longer leaves a child window unattached when it's shown while its parent
  // is off the active Space, which is the only reason a host defers.
  virtual bool DeferReshowPopup() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_HOST_H_
