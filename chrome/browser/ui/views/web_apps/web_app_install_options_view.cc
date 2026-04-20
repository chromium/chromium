// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"

#include <memory>

#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace web_app {

// TODO(crbug.com/496279290): Pass web app icon and origin as constructor
// arguments.
WebAppInstallOptionsView::WebAppInstallOptionsView(InstallOsType os_type) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));
  InitView(os_type);
}

WebAppInstallOptionsView::~WebAppInstallOptionsView() = default;

void WebAppInstallOptionsView::InitView(InstallOsType os_type) {
  switch (os_type) {
    case InstallOsType::kCros: {
      AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetBetweenChildSpacing(10)
              .AddChildren(
                  views::Builder<views::Label>()
                      // TODO(crbug.com/503771561): Modify dialog title instead.
                      .SetText(u"Install this page as an app")
                      .SetTextContext(views::style::CONTEXT_LABEL)
                      .SetTextStyle(views::style::STYLE_PRIMARY)
                      .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD),
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(
                          views::BoxLayout::Orientation::kHorizontal)
                      .SetBetweenChildSpacing(10)
                      .AddChildren(
                          views::Builder<views::Label>()
                              // TODO(crbug.com/496279290): Replace with icon.
                              .SetText(u"[Icon]")
                              .SetTooltipText(u"Icon (Filler)"),
                          views::Builder<views::Label>()
                              // TODO(crbug.com/496279290): Replace with arrow
                              // icon
                              .SetText(u"[Arrow]")
                              .SetTooltipText(u"Arrow (Filler)"),
                          views::Builder<views::Label>()
                              // TODO(crbug.com/496279290): Replace with
                              // Launcher img.
                              .SetText(u"[Launcher Icon]")
                              .SetTooltipText(u"Launcher Icon (Filler)")),
                  views::Builder<views::Checkbox>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_WEB_APP_INSTALL_PIN_TO_SHELF))
                      .SetChecked(true)
                      .CopyAddressTo(&pin_to_shelf_checkbox_))
              .Build());
      break;
    }
    case InstallOsType::kWin: {
      AddChildView(views::Builder<views::Label>()
                       // TODO(crbug.com/503767931): Localize this string.
                       .SetText(u"Installer options Windows view")
                       .SetTextContext(views::style::CONTEXT_LABEL)
                       .SetTextStyle(views::style::STYLE_PRIMARY)
                       .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
                       .Build());
      // TODO(b/492662500): Implement Windows installation options.
      break;
    }
    case InstallOsType::kMac: {
      AddChildView(views::Builder<views::Label>()
                       // TODO(crbug.com/503767931): Localize this string.
                       .SetText(u"Installer options Mac view")
                       .SetTextContext(views::style::CONTEXT_LABEL)
                       .SetTextStyle(views::style::STYLE_PRIMARY)
                       .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
                       .Build());
      // TODO(b/473615915): Implement MacOS installation options.
      break;
    }
    case InstallOsType::kOther:
    default:
      // TODO(b/492663415): Implement kOther installation options.
      AddChildView(views::Builder<views::Label>()
                       // TODO(crbug.com/503767931): Localize this string.
                       .SetText(u"Installer options Other view.")
                       .Build());
      break;
  }
}

bool WebAppInstallOptionsView::IsPinToShelfChecked() const {
  return pin_to_shelf_checkbox_ && pin_to_shelf_checkbox_->GetChecked();
}

}  // namespace web_app
