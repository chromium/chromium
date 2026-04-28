// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"

#include <memory>

#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace web_app {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallOptionsView, kViewId);

// TODO(crbug.com/496279290): Pass web app icon and origin as constructor
// arguments.
WebAppInstallOptionsView::WebAppInstallOptionsView(
    InstallOsType os_type,
    const std::u16string& title,
    const gfx::ImageSkia& icon_image) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));
  SetProperty(views::kElementIdentifierKey, kViewId);
  InitView(os_type, title, icon_image);
}

WebAppInstallOptionsView::~WebAppInstallOptionsView() = default;

void WebAppInstallOptionsView::InitView(InstallOsType os_type,
                                        const std::u16string& title,
                                        const gfx::ImageSkia& icon_image) {
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
      AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetBetweenChildSpacing(10)
              .AddChildren(
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(
                          views::BoxLayout::Orientation::kHorizontal)
                      .SetBetweenChildSpacing(10)
                      .AddChildren(
                          views::Builder<views::ImageView>().SetImage(
                              ui::ImageModel::FromImageSkia(icon_image)),
                          views::Builder<views::BoxLayoutView>()
                              .SetOrientation(
                                  views::BoxLayout::Orientation::kVertical)
                              .AddChildren(
                                  views::Builder<views::Label>()
                                      .SetText(l10n_util::GetStringUTF16(
                                          IDS_WEB_APP_INSTALL_ADD_TO_START_MENU))
                                      .SetTextContext(
                                          views::style::CONTEXT_LABEL)
                                      .SetTextStyle(
                                          views::style::STYLE_SECONDARY)
                                      .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                                  views::Builder<views::Label>()
                                      .SetText(l10n_util::GetStringFUTF16(
                                          IDS_WEB_APP_INSTALL_CHROME_APPS_LOCATION,
                                          title))
                                      .SetTextContext(
                                          views::style::CONTEXT_LABEL)
                                      .SetTextStyle(
                                          views::style::STYLE_EMPHASIZED))),
                  views::Builder<views::Checkbox>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_WEB_APP_INSTALL_CREATE_DESKTOP_SHORTCUT))
                      .SetChecked(true)
                      .CopyAddressTo(&add_desktop_shortcut_checkbox_),
                  views::Builder<views::Checkbox>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_WEB_APP_INSTALL_PIN_TO_TASKBAR))
                      .SetChecked(true)
                      .CopyAddressTo(&pin_to_task_bar_checkbox_))
              .Build());
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

void WebAppInstallOptionsView::SetPinToShelfCheckedForTesting(bool checked) {
  if (pin_to_shelf_checkbox_) {
    pin_to_shelf_checkbox_->SetChecked(checked);
  }
}

base::WeakPtr<WebAppInstallOptionsView> WebAppInstallOptionsView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool WebAppInstallOptionsView::IsAddDesktopShortcutChecked() const {
  return add_desktop_shortcut_checkbox_ &&
         add_desktop_shortcut_checkbox_->GetChecked();
}

bool WebAppInstallOptionsView::IsPinToTaskBarChecked() const {
  return pin_to_task_bar_checkbox_ && pin_to_task_bar_checkbox_->GetChecked();
}

}  // namespace web_app
