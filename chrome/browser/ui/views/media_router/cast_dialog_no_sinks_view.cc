// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace {

class HoverButtonHandCursor : public HoverButton {
 public:
  HoverButtonHandCursor(PressedCallback callback, const ui::ImageModel& icon)
      : HoverButton(std::move(callback), icon, std::u16string()) {}

  HoverButtonHandCursor(const HoverButtonHandCursor&) = delete;
  HoverButtonHandCursor& operator=(const HoverButtonHandCursor&) = delete;

  ~HoverButtonHandCursor() override = default;

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }
};

}  // namespace

namespace media_router {

constexpr base::TimeDelta CastDialogNoSinksView::kSearchWaitTime;

CastDialogNoSinksView::CastDialogNoSinksView(Profile* profile)
    : profile_(profile) {
  // Use horizontal button padding to ensure consistent spacing with the
  // CastDialogView and its sink views that are implemented as Buttons.
  const int horizontal_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);

  // Maintain required padding between the throbber / icon and the label.
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, horizontal_padding), icon_label_spacing));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  icon_ = AddChildView(CreateThrobber());
  label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STATUS_LOOKING_FOR_DEVICES)));

  timer_.Start(FROM_HERE, kSearchWaitTime,
               base::BindOnce(&CastDialogNoSinksView::SetHelpIconView,
                              base::Unretained(this)));
}

CastDialogNoSinksView::~CastDialogNoSinksView() = default;

void CastDialogNoSinksView::SetHelpIconView() {
  // Replace the throbber with the help icon.
  RemoveChildViewT(icon_.get());
  const auto navigate = [](Profile* profile) {
    NavigateParams params(profile, GURL(chrome::kCastNoDestinationFoundURL),
                          ui::PAGE_TRANSITION_LINK);
    Navigate(&params);
  };
  auto* icon = AddChildViewAt(
      std::make_unique<HoverButtonHandCursor>(
          base::BindRepeating(navigate, profile_),
          ui::ImageModel::FromVectorIcon(vector_icons::kHelpOutlineIcon,
                                         ui::kColorAccent, kPrimaryIconSize)),
      0);
  icon->SetInstallFocusRingOnFocus(true);
  icon->SetBorder(views::CreateEmptyBorder(media_router::kPrimaryIconBorder));
  icon->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_NO_DEVICES_FOUND_BUTTON));
  icon->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_NO_DEVICES_FOUND_BUTTON));
  views::InkDrop::Get(icon)->SetMode(views::InkDropHost::InkDropMode::OFF);
  icon_ = icon;

  label_->SetText(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STATUS_NO_DEVICES_FOUND));
}

BEGIN_METADATA(CastDialogNoSinksView, views::View)
END_METADATA

}  // namespace media_router
