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
  // Saves the current omnibox state (e.g. input) to the given tab's
  // user data, so it can be restored when switching back to this tab.
  void SaveStateToTab(content::WebContents* tab) override;
  // Pushes the current text to the WebUI.
  void PushTextToWebUI() override;
  // Called when the active tab changes.
  void OnTabChanged(content::WebContents* contents) override;
  // Called when the omnibox gains focus.
  void OnFocus() override;

 private:
  // Updates the popup state and pushes the current text to the WebUI if the
  // state is set to `kFull`. Enforces the order of operations to minimize
  // text flickers.
  void UpdatePopupStateAndContent(OmniboxPopupState state);

  // Caches the last text string sent to the WebUI to avoid redundant IPCs.
  // Null after a state reset (e.g., tab switch).
  std::optional<std::u16string> last_sent_text_;
  // True when a tab switch is in progress. Used to prevent updates to the
  // WebUI during the transition to avoid race conditions or flickering.
  bool is_switching_tab_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_FULL_WEBUI_H_
