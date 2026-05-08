// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kDefaultWidth = 800;
constexpr int kDefaultHeight = 600;
}  // namespace

DrivePickerHostView::DrivePickerHostView(Profile* profile) {
  // Set a default preferred size for the picker. This ensures that on
  // initialization (and particularly on Mac), the window has a non-zero size
  // before the WebUI content determines the final layout.
  SetPreferredSize(gfx::Size(kDefaultWidth, kDefaultHeight));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::WebView* web_view =
      AddChildView(std::make_unique<views::WebView>(profile));
  view_tracker_.SetView(web_view);

  // Since the WebView was just created with a valid profile, GetWebContents()
  // is guaranteed to return a valid pointer. We use a CHECK here to assert
  // this state and remove redundant defensive checks.
  CHECK(web_view->GetWebContents());
  web_view->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  web_view->LoadInitialURL(GURL(chrome::kChromeUIDrivePickerHostURL));
}

DrivePickerHostView::~DrivePickerHostView() = default;

content::WebContents* DrivePickerHostView::GetWebContents() {
  if (!view_tracker_.view()) {
    return nullptr;
  }
  return views::AsViewClass<views::WebView>(view_tracker_.view())
      ->GetWebContents();
}

void DrivePickerHostView::TriggerDrivePickerHostUi(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  if (!view_tracker_.view()) {
    return;
  }
  views::WebView* web_view =
      views::AsViewClass<views::WebView>(view_tracker_.view());
  if (!web_view) {
    return;
  }
  content::WebContents* contents = web_view->GetWebContents();
  if (contents && contents->GetWebUI()) {
    auto* drive_picker_host_ui =
        contents->GetWebUI()->GetController()->GetAs<DrivePickerHostUI>();
    if (drive_picker_host_ui) {
      drive_picker_host_ui->TriggerDrivePickerHost(std::move(result_handler));
    }
  }
}

BEGIN_METADATA(DrivePickerHostView)
END_METADATA
