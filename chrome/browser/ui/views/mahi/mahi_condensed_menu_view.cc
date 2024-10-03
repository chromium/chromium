// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_condensed_menu_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_shadow.h"
#include "ui/views/widget/widget.h"

namespace chromeos::mahi {

namespace {

constexpr int kButtonIconSize = 16;
constexpr int kButtonIconLabelSpacing = 8;
constexpr auto kButtonBorderInsets = gfx::Insets::VH(12, 16);

// View for the button which makes up most of the condensed Mahi menu.
class MahiCondensedMenuButton : public views::LabelButton {
  METADATA_HEADER(MahiCondensedMenuButton, views::LabelButton)

 public:
  MahiCondensedMenuButton() {
    SetCallback(base::BindRepeating(&MahiCondensedMenuButton::OnButtonClicked,
                                    weak_ptr_factory_.GetWeakPtr()));
    SetText(
        l10n_util::GetStringUTF16(IDS_ASH_MAHI_CONDENSED_MENU_BUTTON_LABEL));
    SetLabelStyle(views::style::STYLE_BODY_3_EMPHASIS);
    SetImageModel(views::Button::ButtonState::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      kMahiSparkIcon, ui::kColorSysOnSurface, kButtonIconSize));
    SetImageLabelSpacing(kButtonIconLabelSpacing);
    SetBorder(views::CreateEmptyBorder(kButtonBorderInsets));
    SetInstallFocusRingOnFocus(false);
  }
  MahiCondensedMenuButton(const MahiCondensedMenuButton&) = delete;
  MahiCondensedMenuButton& operator=(const MahiCondensedMenuButton&) = delete;
  ~MahiCondensedMenuButton() override = default;

  // views::LabelButton:
  void OnFocus() override { SetBackgroundHighlighted(true); }

  void OnBlur() override { SetBackgroundHighlighted(false); }

  void StateChanged(views::Button::ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SetBackgroundHighlighted(GetState() == views::Button::STATE_HOVERED);
  }

 private:
  void OnButtonClicked() {
    // TODO(b/324647147): Add separate button type for condensed menu.
    chromeos::MahiWebContentsManager::Get()->OnContextMenuClicked(
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(GetWidget()->GetNativeWindow())
            .id(),
        /*button_type=*/::chromeos::mahi::ButtonType::kSummary,
        /*question=*/std::u16string(),
        /*mahi_menu_bounds=*/
        parent() ? parent()->GetBoundsInScreen() : gfx::Rect());

    base::UmaHistogramEnumeration(kMahiContextMenuButtonClickHistogram,
                                  MahiMenuButton::kCondensedMenuButton);
  }

  void SetBackgroundHighlighted(bool background_highlighted) {
    if (background_highlighted) {
      SetBackground(views::CreateThemedRoundedRectBackground(
          ui::kColorMenuItemBackgroundHighlighted,
          views::LayoutProvider::Get()->GetCornerRadiusMetric(
              views::ShapeContextTokens::kMenuRadius)));
    } else {
      SetBackground(nullptr);
    }
  }

  base::WeakPtrFactory<MahiCondensedMenuButton> weak_ptr_factory_{this};
};

BEGIN_METADATA(MahiCondensedMenuButton)
END_METADATA

}  // namespace

MahiCondensedMenuView::MahiCondensedMenuView()
    : view_shadow_(std::make_unique<views::ViewShadow>(this, /*elevation=*/2)) {
  SetUseDefaultFillLayout(true);
  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysSurface, views::LayoutProvider::Get()->GetCornerRadiusMetric(
                                views::ShapeContextTokens::kMenuRadius)));

  menu_button_ = AddChildView(std::make_unique<MahiCondensedMenuButton>());
}

MahiCondensedMenuView::~MahiCondensedMenuView() = default;

void MahiCondensedMenuView::RequestFocus() {
  menu_button_->RequestFocus();
}

BEGIN_METADATA(MahiCondensedMenuView)
END_METADATA

}  // namespace chromeos::mahi
