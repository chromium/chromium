// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "chrome/browser/web_applications/icons/icon_masker.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_shadow.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(WebAppInstallOptionsView, kViewId);

namespace {

constexpr int kIconArrowSize = 24;
constexpr int kIllustrationCornerRadius = 8;
constexpr int kMacArrowWidth = 64;
constexpr int kMacArrowHeight = 94;
constexpr int kMacArrowBottomPadding = 20;

// A simple implementation of rounded shadows around the icon. The vector
// dimensions correspond to the x and y offset of the shadow. The shadow will
// appear like light is coming from the top.
gfx::ShadowValues GetIconShadowValues() {
  gfx::ShadowValues shadows;
  shadows.emplace_back(gfx::Vector2d(0, 2), 4,
                       SkColorSetA(SK_ColorBLACK, 0x3d));
  shadows.emplace_back(gfx::Vector2d(0, 1), 2,
                       SkColorSetA(SK_ColorBLACK, 0x1f));
  return shadows;
}

gfx::ImageSkia GetStandardizedIcon(const gfx::ImageSkia& image,
                                   bool is_maskable) {
  if (is_maskable) {
    return image;
  }
  image.EnsureRepsForSupportedScales();
  return apps::CreateStandardIconImage(image);
}

gfx::ImageSkia ApplyShadowToIcon(const gfx::ImageSkia& image) {
  return gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      image, GetIconShadowValues());
}

// Creates an Icon View with a label, used to display icons and an associated
// label under it.
views::Builder<views::BoxLayoutView> CreateIconWithLabelView(
    const ui::ImageModel& image_model,
    const std::u16string& label_text,
    const gfx::Size& image_size,
    int corner_radius = 0,
    raw_ptr<views::ImageView>* icon_view_out = nullptr) {
  auto image_builder = views::Builder<views::ImageView>()
                           .SetImage(image_model)
                           .SetImageSize(image_size)
                           .SetPreferredSize(image_size);
  if (corner_radius > 0) {
    image_builder.SetCornerRadius(corner_radius);
  }
  if (icon_view_out) {
    image_builder.CopyAddressTo(icon_view_out);
  }

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
      .AddChildren(std::move(image_builder),
                   views::Builder<views::Label>()
                       .SetText(label_text)
                       .SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL)
                       .SetTextStyle(views::style::STYLE_SECONDARY));
}

}  // namespace

// static
std::unique_ptr<WebAppInstallOptionsView> WebAppInstallOptionsView::Create(
    InstallOsType os_type,
    const std::u16string& title,
    const gfx::ImageSkia& icon_image,
    const gfx::ImageSkia& large_icon_image,
    bool is_maskable,
    const GURL& start_url) {
  return base::WrapUnique(new WebAppInstallOptionsView(
      os_type, title, icon_image, large_icon_image, is_maskable, start_url));
}

WebAppInstallOptionsView::WebAppInstallOptionsView(

    InstallOsType os_type,
    const std::u16string& title,
    const gfx::ImageSkia& icon_image,
    const gfx::ImageSkia& large_icon_image,
    bool is_maskable,
    const GURL& start_url) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));
  SetProperty(views::kElementIdentifierKey, kViewId);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  switch (os_type) {
    case InstallOsType::kCros: {
      gfx::ImageSkia displayed_image =
          ApplyShadowToIcon(GetStandardizedIcon(large_icon_image, is_maskable));

      AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetMainAxisAlignment(
                  views::BoxLayout::MainAxisAlignment::kCenter)
              .SetBetweenChildSpacing(
                  views::LayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
              .AddChildren(
                  CreateIconWithLabelView(
                      ui::ImageModel::FromImageSkia(displayed_image),
                      url_formatter::FormatOriginForSecurityDisplay(
                          url::Origin::Create(start_url),
                          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS),
                      displayed_image.size(),
                      /*corner_radius=*/0, &icon_view_),
                  views::Builder<views::ImageView>()
                      .SetImage(ui::ImageModel::FromVectorIcon(
                          kArrowForwardIcon, ui::kColorIcon))
                      .SetPreferredSize(
                          gfx::Size(kIconArrowSize, kIconArrowSize)),
                  CreateIconWithLabelView(
                      bundle.GetThemedLottieImageNamed(
                          IDR_WEB_APP_INTERNALS_CROS_LAUNCHER),
                      l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_LAUNCHER),
                      gfx::Size(kLargeImageSize, kLargeImageSize),
                      kIllustrationCornerRadius))
              .Build());

      AddChildView(views::Builder<views::Checkbox>()
                       .SetText(l10n_util::GetStringUTF16(
                           IDS_WEB_APP_INSTALL_PIN_TO_SHELF))
                       .SetChecked(true)
                       .CopyAddressTo(&pin_to_shelf_checkbox_)
                       .Build());

      MaybeApplyOsIconMasking(large_icon_image, is_maskable);

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
      gfx::ImageSkia displayed_image = ApplyShadowToIcon(large_icon_image);

      AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetMainAxisAlignment(
                  views::BoxLayout::MainAxisAlignment::kCenter)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kCenter)
              .SetBetweenChildSpacing(
                  views::LayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
              .AddChildren(
                  CreateIconWithLabelView(
                      ui::ImageModel::FromImageSkia(displayed_image),
                      url_formatter::FormatOriginForSecurityDisplay(
                          url::Origin::Create(start_url),
                          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS),
                      displayed_image.size(),
                      /*corner_radius=*/0, &icon_view_),
                  views::Builder<views::ImageView>()
                      .SetImage(ui::ImageModel::FromVectorIcon(
                          kPwaInstallArrowCustomIcon, ui::kColorIcon,
                          kMacArrowWidth))
                      .SetPreferredSize(
                          gfx::Size(kMacArrowWidth, kMacArrowHeight))
                      .SetBorder(views::CreateEmptyBorder(
                          gfx::Insets::TLBR(0, 0, kMacArrowBottomPadding, 0))),
                  views::Builder<views::Label>()
                      // TODO(crbug.com/507108235): Replace with
                      // Mac Apps Folder.
                      .SetText(u"[Apps Folder]")
                      .SetTooltipText(u"Apps Folder Placeholder"))
              .Build());

      MaybeApplyOsIconMasking(large_icon_image, is_maskable);
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

void WebAppInstallOptionsView::OnIconMaskingCompleteWithShadow(
    SkBitmap masked_bitmap) {
  CHECK(icon_view_);
  gfx::Image masked_image = gfx::Image::CreateFrom1xBitmap(masked_bitmap);
  gfx::ShadowValues callback_shadows = GetIconShadowValues();
  gfx::ImageSkia image_with_shadow =
      gfx::ImageSkiaOperations::CreateImageWithDropShadow(
          masked_image.AsImageSkia(), callback_shadows);
  icon_view_->SetImage(ui::ImageModel::FromImageSkia(image_with_shadow));
}

void WebAppInstallOptionsView::MaybeApplyOsIconMasking(
    const gfx::ImageSkia& icon_image,
    bool is_maskable) {
  if (is_maskable && icon_view_) {
    web_app::MaskIconOnOs(
        *icon_image.bitmap(),
        base::BindOnce(
            &WebAppInstallOptionsView::OnIconMaskingCompleteWithShadow,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

WebAppInstallOptionsView::~WebAppInstallOptionsView() = default;

bool WebAppInstallOptionsView::IsPinToShelfChecked() const {
  return pin_to_shelf_checkbox_ && pin_to_shelf_checkbox_->GetChecked();
}

void WebAppInstallOptionsView::SetPinToShelfCheckedForTesting(bool checked) {
  if (pin_to_shelf_checkbox_) {
    pin_to_shelf_checkbox_->SetChecked(checked);
  }
}

bool WebAppInstallOptionsView::IsAddDesktopShortcutChecked() const {
  return add_desktop_shortcut_checkbox_ &&
         add_desktop_shortcut_checkbox_->GetChecked();
}

bool WebAppInstallOptionsView::IsPinToTaskBarChecked() const {
  return pin_to_task_bar_checkbox_ && pin_to_task_bar_checkbox_->GetChecked();
}

void WebAppInstallOptionsView::SetAddDesktopShortcutCheckedForTesting(
    bool checked) {
  if (add_desktop_shortcut_checkbox_) {
    add_desktop_shortcut_checkbox_->SetChecked(checked);
  }
}

void WebAppInstallOptionsView::SetPinToTaskBarCheckedForTesting(bool checked) {
  if (pin_to_task_bar_checkbox_) {
    pin_to_task_bar_checkbox_->SetChecked(checked);
  }
}

base::WeakPtr<WebAppInstallOptionsView> WebAppInstallOptionsView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace web_app
