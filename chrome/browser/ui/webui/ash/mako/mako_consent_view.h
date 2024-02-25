// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_CONSENT_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_CONSENT_VIEW_H_

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// A view to contain the Mako consent UI.
class MakoConsentView : public WebUIBubbleDialogView {
  METADATA_HEADER(MakoConsentView, WebUIBubbleDialogView)

 public:
  MakoConsentView(WebUIContentsWrapper* contents_wrapper,
                  const gfx::Rect& caret_bounds);
  MakoConsentView(const MakoConsentView&) = delete;
  MakoConsentView& operator=(const MakoConsentView&) = delete;
  ~MakoConsentView() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_CONSENT_VIEW_H_
