// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

class LocationBar;
class OmniboxPopupPresenterBase;
class OmniboxController;

// The content WebView for the full popup (input row + suggestions dropdown) of
// a WebUI Omnibox.
class OmniboxFullPopupWebUIContent : public OmniboxPopupWebUIContent {
  METADATA_HEADER(OmniboxFullPopupWebUIContent, OmniboxPopupWebUIContent)

 public:
  OmniboxFullPopupWebUIContent() = delete;
  OmniboxFullPopupWebUIContent(OmniboxPopupPresenterBase* presenter,
                               LocationBar* location_bar,
                               OmniboxController* controller);
  OmniboxFullPopupWebUIContent(const OmniboxFullPopupWebUIContent&) = delete;
  OmniboxFullPopupWebUIContent& operator=(const OmniboxFullPopupWebUIContent&) =
      delete;
  ~OmniboxFullPopupWebUIContent() override;

 protected:
  std::string_view GetMetricPrefix() const override;

 private:
  // WebUIContentsWrapper::Host:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  base::WeakPtrFactory<OmniboxPopupWebUIContent> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   OmniboxFullPopupWebUIContent,
                   OmniboxPopupWebUIContent)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, OmniboxFullPopupWebUIContent)

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_
