// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_preselection_bubble.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "lens_preselection_bubble.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_delegate.h"

namespace lens {
namespace {

// The minimum y value in screen coordinates for the preselection bubble.
const int kPreselectionBubbleMinY = 8;

}  // namespace

LensPreselectionBubble::LensPreselectionBubble(views::View* anchor_view,
                                               bool offline,
                                               ExitClickedCallback callback)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::NONE,
                               views::BubbleBorder::NO_SHADOW),
      offline_(offline),
      callback_(std::move(callback)) {
  // Toast bubble doesn't have any buttons, cannot be active, and should not be
  // focus traversable.
  SetShowCloseButton(false);
  SetCanActivate(false);
  set_focus_traversable_from_anchor_view(false);
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  set_corner_radius(48);
  SetProperty(views::kElementIdentifierKey, kLensPreselectionBubbleElementId);
  SetAccessibleWindowRole(ax::mojom::Role::kAlertDialog);
}

LensPreselectionBubble::~LensPreselectionBubble() = default;

void LensPreselectionBubble::Init() {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets()));
  offline_ ? set_margins(gfx::Insets::TLBR(6, 16, 6, 6))
           : set_margins(gfx::Insets::TLBR(12, 16, 12, 16));

  // Set bubble icon and text
  const std::u16string toast_text =
      offline_
          ? l10n_util::GetStringUTF16(
                IDS_LENS_OVERLAY_INITIAL_TOAST_ERROR_MESSAGE)
          : l10n_util::GetStringUTF16(IDS_LENS_OVERLAY_INITIAL_TOAST_MESSAGE);
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  label_ = AddChildView(std::make_unique<views::Label>(toast_text));
  label_->SetMultiLine(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetAllowCharacterBreak(false);
  layout->set_between_child_spacing(8);
  // Need to set this false so label color token doesn't get changed by
  // changed by SetEnabledColor() color mapper. Color tokens provided
  // have enough contrast.
  label_->SetAutoColorReadabilityEnabled(false);
  if (offline_) {
    exit_button_ = AddChildView(std::make_unique<views::MdTextButton>(
        std::move(callback_),
        l10n_util::GetStringUTF16(
            IDS_LENS_OVERLAY_INITIAL_TOAST_ERROR_EXIT_BUTTON_TEXT)));
    exit_button_->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, 8, 0, 0));
    exit_button_->SetPreferredSize(gfx::Size(55, 36));
    exit_button_->SetStyle(ui::ButtonStyle::kProminent);
    exit_button_->SetProperty(views::kElementIdentifierKey,
                              kLensPreselectionBubbleExitButtonElementId);
  }
  NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

gfx::Rect LensPreselectionBubble::GetBubbleBounds() {
  views::View* anchor_view = GetAnchorView();
  if (anchor_view) {
    const gfx::Size bubble_size =
        GetWidget()->GetContentsView()->GetPreferredSize();
    const gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();
    const int x =
        anchor_bounds.x() + (anchor_bounds.width() - bubble_size.width()) / 2;
    // Take bubble out of its original bounds to cross "line of death". However,
    // if there is no line of death, we set the bubble to below the top of the
    // screen.
    const int y = std::max(kPreselectionBubbleMinY,
                           anchor_bounds.bottom() - bubble_size.height() / 2);
    return gfx::Rect(x, y, bubble_size.width(), bubble_size.height());
  }
  return gfx::Rect();
}

void LensPreselectionBubble::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  set_color(color_provider->GetColor(kColorLensOverlayToastBackground));
  icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      offline_ ? vector_icons::kErrorOutlineIcon
               : vector_icons::kGoogleLensMonochromeLogoIcon,
#else
      offline_ ? vector_icons::kErrorOutlineIcon
               : vector_icons::kSearchChromeRefreshIcon,
#endif
      color_provider->GetColor(ui::kColorToastForeground), 24));
  label_->SetEnabledColor(color_provider->GetColor(ui::kColorToastForeground));

  if (offline_) {
    CHECK(exit_button_);
    exit_button_->SetEnabledTextColors(
        color_provider->GetColor(kColorLensOverlayToastButtonText));
    exit_button_->SetBorder(views::CreateRoundedRectBorder(
        1, 48, color_provider->GetColor(ui::kColorButtonBorder)));
    exit_button_->SetBgColorIdOverride(kColorLensOverlayToastBackground);
  }
}

BEGIN_METADATA(LensPreselectionBubble)
END_METADATA

}  // namespace lens
