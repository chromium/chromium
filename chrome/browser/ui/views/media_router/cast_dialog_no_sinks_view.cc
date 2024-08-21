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
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

class HoverButtonHandCursor : public HoverButton {
  METADATA_HEADER(HoverButtonHandCursor, HoverButton)

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

BEGIN_METADATA(HoverButtonHandCursor)
END_METADATA

}  // namespace

namespace media_router {

constexpr base::TimeDelta CastDialogNoSinksView::kSearchWaitTime;

CastDialogNoSinksView::CastDialogNoSinksView(Profile* profile,
                                             bool permission_rejected)
    : profile_(profile), permission_rejected_(permission_rejected) {
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

  if (permission_rejected_) {
    MediaRouterMetrics::RecordMediaRouterUiPermissionRejectedViewEvents(
        MediaRouterUiPermissionRejectedViewEvents::kCastDialogErrorShown);

    SetHelpIcon();
    size_t offset;
    std::u16string settings_text_for_link = l10n_util::GetStringUTF16(
        IDS_MEDIA_ROUTER_LOCAL_DISCOVERY_PERMISSION_REJECTED_LINK);
    std::u16string label_text = l10n_util::GetStringFUTF16(
        IDS_MEDIA_ROUTER_LOCAL_DISCOVERY_PERMISSION_REJECTED_LABEL,
        settings_text_for_link, &offset);

    // TODO(crbug.com/359973625): Remove this empty container once
    // AXPlatformNodeCocoa::AXBoundsForRange is implemented.
    auto* accessibility_container =
        AddChildView(std::make_unique<views::View>());
    accessibility_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>());
    accessibility_container->GetViewAccessibility().SetRole(
        ax::mojom::Role::kGroup);
    accessibility_container->GetViewAccessibility().SetName(
        label_text, ax::mojom::NameFrom::kRelatedElement);
    accessibility_container->SetFocusBehavior(
        views::View::FocusBehavior::ACCESSIBLE_ONLY);

    permission_rejected_label_ = accessibility_container->AddChildView(
        std::make_unique<views::StyledLabel>());
    permission_rejected_label_->SetText(label_text);
    permission_rejected_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
#if BUILDFLAG(IS_MAC)
    base::RepeatingClosure open_settings_cb = base::BindRepeating([]() {
      // TODO(crbug.com/358725038): Open the Local Network sub-pane in system
      // settings directly once the feature request to Apple (FB14789617) is
      // solved.
      base::mac::OpenSystemSettingsPane(
          base::mac::SystemSettingsPane::kPrivacySecurity);
      MediaRouterMetrics::RecordMediaRouterUiPermissionRejectedViewEvents(
          MediaRouterUiPermissionRejectedViewEvents::kCastDialogLinkClicked);
    });
    permission_rejected_label_->AddStyleRange(
        gfx::Range(offset, offset + settings_text_for_link.length()),
        views::StyledLabel::RangeStyleInfo::CreateForLink(open_settings_cb));
#endif
  } else {
    icon_ = AddChildView(CreateThrobber());
    label_ =
        AddChildView(std::make_unique<views::Label>(l10n_util::GetStringUTF16(
            IDS_MEDIA_ROUTER_STATUS_LOOKING_FOR_DEVICES)));

    timer_.Start(FROM_HERE, kSearchWaitTime,
                 base::BindOnce(&CastDialogNoSinksView::ShowNoDeviceFoundView,
                                base::Unretained(this)));
  }
}

CastDialogNoSinksView::~CastDialogNoSinksView() = default;

void CastDialogNoSinksView::SetHelpIcon() {
  // Replace the throbber with the help icon.
  if (icon_) {
    RemoveChildViewT(icon_.get());
  }
  const auto navigate = [](Profile* profile) {
    NavigateParams params(profile, GURL(chrome::kCastNoDestinationFoundURL),
                          ui::PAGE_TRANSITION_LINK);
    Navigate(&params);
  };
  auto* icon =
      AddChildViewAt(std::make_unique<HoverButtonHandCursor>(
                         base::BindRepeating(navigate, profile_),
                         ui::ImageModel::FromVectorIcon(
                             vector_icons::kHelpOutlineIcon,
                             kColorCastDialogHelpIcon, kPrimaryIconSize)),
                     0);
  icon->SetInstallFocusRingOnFocus(true);
  icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  auto a11y_text = l10n_util::GetStringUTF16(
      permission_rejected_
          ? IDS_MEDIA_ROUTER_LOCAL_DISCOVERY_PERMISSION_REJECTED_BUTTON
          : IDS_MEDIA_ROUTER_NO_DEVICES_FOUND_BUTTON);
  icon->GetViewAccessibility().SetName(a11y_text);
  icon->SetTooltipText(a11y_text);
  views::InkDrop::Get(icon)->SetMode(views::InkDropHost::InkDropMode::OFF);
  icon_ = icon;
}

void CastDialogNoSinksView::ShowNoDeviceFoundView() {
  SetHelpIcon();
  label_->SetText(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STATUS_NO_DEVICES_FOUND));
}

BEGIN_METADATA(CastDialogNoSinksView)
END_METADATA

}  // namespace media_router
