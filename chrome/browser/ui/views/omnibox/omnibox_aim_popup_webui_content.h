// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_AIM_POPUP_WEBUI_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_AIM_POPUP_WEBUI_CONTENT_H_

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

namespace content {
class WebContents;
}  // namespace content

class LocationBarView;
class OmniboxController;
class OmniboxPopupAimHandler;
class OmniboxPopupPresenterBase;

// The content WebView for the popup of a WebUI Omnibox.
class OmniboxAimPopupWebUIContent : public OmniboxPopupWebUIBaseContent {
  METADATA_HEADER(OmniboxAimPopupWebUIContent, OmniboxPopupWebUIBaseContent)

 public:
  OmniboxAimPopupWebUIContent() = delete;
  OmniboxAimPopupWebUIContent(OmniboxPopupPresenterBase* presenter,
                              LocationBarView* location_bar_view,
                              OmniboxController* controller);
  OmniboxAimPopupWebUIContent(const OmniboxAimPopupWebUIContent&) = delete;
  OmniboxAimPopupWebUIContent& operator=(const OmniboxAimPopupWebUIContent&) =
      delete;
  ~OmniboxAimPopupWebUIContent() override;

  // OmniboxPopupWebUIBaseContent:
  // The call flow for hiding the popup is:
  // 1. `OmniboxPopupPresenterBase::Hide()` hides the widget
  //    and calls `Clear()` in sequence, which calls
  // 2. OmniboxAimPopupWebUIContent::Clear(), which calls
  // 3. OmniboxPopupAimHandler::ClearPopup(), which calls
  // 4. omnibox_popup_aim.mojom.Page::ClearPopup() (Mojo call to JS).
  // 5. JS runs and eventually executes the Mojo callback, which runs the C++
  //    callback:
  //    a. OmniboxAimPopupWebUIContent::OnClearCallback() which calls
  //    b. OmniboxPopupWebUIBaseContent::Detach() (Detaches WebContents
  //       and breaks recursion loops).
  //    c. OmniboxAimPopupWebUIContent::ApplyInputAndCleanup() (Resets the
  //       OmniboxView text).
  void Clear() override;

  // Called from the browser after popup has already closed. `input` is
  // the possibly empty input that should replace the omnibox text.
  void ApplyInputAndCleanup(const std::string& input);

  // Refocuses the location bar if screen readers are enabled and the popup is
  // active.
  void UpdateLocationBarFocusForScreenReader();

 protected:
  std::string_view GetMetricPrefix() const override;

 private:
  // Saves the input to the background tab's state.
  void SaveInputToBackgroundTab(content::WebContents* original_web_contents,
                                const std::string& input);

  // WebUIContentsWrapper::Host:
  // Called from WebUI code to close the widget. I.e. when user presses
  // <escape>, presses the 'x' button, or moves focus out of the popup.
  void CloseUI() override;
  void ShowUI() override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  // Returns the WebUI Handler. Can return null.
  OmniboxPopupAimHandler* popup_aim_handler();

  // Called when the popup is hidden and the WebUI has painted a clean frame.
  void OnClearCallback(base::WeakPtr<content::WebContents> original_web_contents,
                       const std::string& input);

  FRIEND_TEST_ALL_PREFIXES(OmniboxAimPopupBrowserTest,
                           DraftTextPreservedOnTabSwitch);

  base::WeakPtr<content::WebContents> active_web_contents_;

  base::WeakPtrFactory<OmniboxAimPopupWebUIContent> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   OmniboxAimPopupWebUIContent,
                   OmniboxPopupWebUIBaseContent)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, OmniboxAimPopupWebUIContent)

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_AIM_POPUP_WEBUI_CONTENT_H_
