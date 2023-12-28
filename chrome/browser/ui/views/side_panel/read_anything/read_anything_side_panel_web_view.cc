// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/base/metadata/metadata_impl_macros.h"

using SidePanelWebUIViewT_ReadAnythingUntrustedUI =
    SidePanelWebUIViewT<ReadAnythingUntrustedUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_ReadAnythingUntrustedUI,
                        SidePanelWebUIViewT);

END_METADATA

ReadAnythingSidePanelWebView::ReadAnythingSidePanelWebView(Profile* profile)
    : SidePanelWebUIViewT(
          base::RepeatingClosure(),
          base::RepeatingClosure(),
          std::make_unique<BubbleContentsWrapperT<ReadAnythingUntrustedUI>>(
              GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL),
              profile,
              IDS_READING_MODE_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false)) {}

bool ReadAnythingSidePanelWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

ReadAnythingSidePanelWebView::~ReadAnythingSidePanelWebView() = default;

BEGIN_METADATA(ReadAnythingSidePanelWebView)
END_METADATA
