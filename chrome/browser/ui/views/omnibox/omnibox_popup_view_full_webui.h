// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_FULL_WEBUI_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_FULL_WEBUI_H_

#include <optional>

#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"

class LocationBar;
class OmniboxController;
class OmniboxView;
class OmniboxPopupPresenterDelegate;
class OmniboxPopupHandler;

class OmniboxPopupViewFullWebUI : public OmniboxPopupViewWebUI {
 public:
  OmniboxPopupViewFullWebUI(OmniboxView* omnibox_view,
                            OmniboxController* controller,
                            LocationBar* location_bar,
                            OmniboxPopupPresenterDelegate& presenter_delegate);
  OmniboxPopupViewFullWebUI(const OmniboxPopupViewFullWebUI&) = delete;
  OmniboxPopupViewFullWebUI& operator=(const OmniboxPopupViewFullWebUI&) =
      delete;
  ~OmniboxPopupViewFullWebUI() override;

  // OmniboxPopupView:
  // Pushes the current permanent display text (e.g. a URL) to the WebUI on
  // focus or if the text changed.
  void UpdatePopupAppearance() override;
  // Syncs the text and selection state from the native location bar to the
  // WebUI omnibox.
  void SyncNativeStateToWebUI(bool query_zps) override;
  // Saves the current omnibox state (e.g. input) to the given tab's
  // user data, so it can be restored when switching back to this tab.
  void SaveStateToTab(content::WebContents* tab) override;
  // Called when the active tab changes.
  void OnTabChanged(content::WebContents* contents) override;
  // Called when the native omnibox gains focus. If the popup state changed,
  // synchronizes full input state (`SyncNativeStateToWebUI()`). If the popup
  // was already open, sends a dedicated `SetFocus(true)` Mojo IPC to ensure
  // DOM input focus in the WebUI is restored without resetting input state.
  void OnFocus(bool query_zps) override;
  // Called when the native omnibox loses focus. Sends a dedicated
  // `SetFocus(false)` Mojo IPC to ensure DOM input focus in the WebUI is
  // cleared.
  void OnBlur() override;
  bool IsReverting() const override;
  void SetIsReverting(bool reverting) override;

  // True if this has focus, reckoned by OnFocus/OnBlur. This may be slightly
  // out-of-sync with the model's when doing auto-focus, since the timings of
  // OnTabChanged() and LocationBar::FocusLocation(/*user_initiated=*/false) are
  // messy.
  bool is_focused() { return focused_; }

 private:
  // Gets the OmniboxPopupHandler associated with this view's WebUI.
  OmniboxPopupHandler* GetPopupHandler();

  // Caches the last text string sent to the WebUI to avoid redundant IPCs.
  // Null after a state reset (e.g., tab switch).
  std::optional<std::u16string> last_sent_text_;
  // Caches the last focus state sent to the WebUI to detect focus transitions.
  std::optional<bool> last_sent_focus_;
  bool has_completed_first_tab_changed_ = false;
  bool is_reverting_ = false;
  bool focused_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_FULL_WEBUI_H_
