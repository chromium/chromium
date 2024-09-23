// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/cast_device_selector_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

constexpr gfx::Size kCloseButtonSize{28, 28};

constexpr int kBackgroundBorderThickness = 1;
constexpr int kBackgroundCornerRadius = 8;
constexpr int kDeviceEntryCornerRadius = 4;
constexpr int kBackgroundSeparator = 8;
constexpr int kDeviceContainerSeparator = 4;
constexpr int kDeviceEntrySeparator = 8;
constexpr int kCloseButtonIconSize = 20;
constexpr int kDeviceEntryIconSize = 20;

constexpr gfx::Insets kBackgroundInsets = gfx::Insets::TLBR(12, 8, 16, 8);
constexpr gfx::Insets kCastHeaderRowInsets = gfx::Insets::TLBR(0, 8, 0, 4);
constexpr gfx::Insets kIconHoverButtonInsets = gfx::Insets::VH(6, 8);
constexpr gfx::Insets kThrobberHoverButtonInsets = gfx::Insets::VH(0, 8);

}  // namespace

IssueHoverButton::IssueHoverButton(PressedCallback callback,
                                   global_media_controls::mojom::IconType icon,
                                   const std::u16string& device_name,
                                   const std::u16string& status_text,
                                   ui::ColorId device_name_color_id,
                                   ui::ColorId status_text_color_id)
    : HoverButton(std::move(callback), std::u16string()) {
  label()->SetVisible(false);
  SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  ink_drop_container()->SetProperty(views::kViewIgnoredByLayoutKey, true);
  GetViewAccessibility().SetName(
      base::JoinString({device_name, status_text}, u"\n"));
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kIconHoverButtonInsets,
      kDeviceEntrySeparator));

  // Create a column to hold the info icon view.
  auto* icon_view_column =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  icon_view_column->SetCanProcessEventsWithinSubtree(false);
  icon_view_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  auto* icon_view = icon_view_column->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GetVectorIcon(icon), status_text_color_id, kDeviceEntryIconSize)));
  icon_view->SetCanProcessEventsWithinSubtree(false);

  // Create a column to hold the device name label and status text label.
  auto* label_column = AddChildView(std::make_unique<views::BoxLayoutView>());
  label_column->SetCanProcessEventsWithinSubtree(false);
  label_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  label_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  label_column->SetBetweenChildSpacing(kDeviceContainerSeparator);
  layout->SetFlexForView(label_column, 1);

  device_name_label_ = label_column->AddChildView(
      std::make_unique<views::Label>(device_name, views::style::CONTEXT_LABEL,
                                     views::style::STYLE_BODY_2));
  device_name_label_->SetCanProcessEventsWithinSubtree(false);
  device_name_label_->SetEnabledColorId(device_name_color_id);

  status_text_label_ = label_column->AddChildView(
      std::make_unique<views::Label>(status_text, views::style::CONTEXT_LABEL,
                                     views::style::STYLE_BODY_4));
  status_text_label_->SetCanProcessEventsWithinSubtree(false);
  status_text_label_->SetEnabledColorId(status_text_color_id);
}

gfx::Size IssueHoverButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetLayoutManager()->GetPreferredSize(this, available_size);
}

BEGIN_METADATA(IssueHoverButton)
END_METADATA

CastDeviceSelectorView::CastDeviceSelectorView(
    mojo::PendingRemote<global_media_controls::mojom::DeviceListHost>
        device_list_host,
    mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient>
        device_list_client,
    media_message_center::MediaColorTheme media_color_theme,
    bool show_devices)
    : device_list_host_(std::move(device_list_host)),
      device_list_client_(this, std::move(device_list_client)),
      media_color_theme_(media_color_theme) {
  SetBorder(views::CreateThemedRoundedRectBorder(
      kBackgroundBorderThickness, kBackgroundCornerRadius,
      media_color_theme_.device_selector_border_color_id));
  SetBackground(views::CreateThemedRoundedRectBackground(
      media_color_theme_.device_selector_background_color_id,
      kBackgroundCornerRadius));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBackgroundInsets,
      kBackgroundSeparator));

  // |cast_header_row| holds the cast header label and the close button.
  auto* cast_header_row =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  cast_header_row->SetInsideBorderInsets(kCastHeaderRowInsets);
  cast_header_row->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Create the cast header label.
  views::Label* cast_header_label =
      cast_header_row->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_CAST_HEADER_TEXT),
          views::style::CONTEXT_LABEL, views::style::STYLE_HEADLINE_5));
  cast_header_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  cast_header_row->SetFlexForView(cast_header_label, 1);

  // Create the close button.
  auto close_button =
      std::make_unique<global_media_controls::MediaActionButton>(
          base::BindRepeating(&CastDeviceSelectorView::CloseButtonPressed,
                              base::Unretained(this)),
          global_media_controls::kEmptyMediaActionButtonId,
          IDS_GLOBAL_MEDIA_CONTROLS_CLOSE_DEVICE_LIST_TEXT,
          kCloseButtonIconSize, vector_icons::kCloseSmallIcon, kCloseButtonSize,
          media_color_theme_.secondary_foreground_color_id,
          media_color_theme_.secondary_foreground_color_id,
          media_color_theme_.focus_ring_color_id);
  close_button_ = cast_header_row->AddChildView(std::move(close_button));

  // Create the container view to hold available devices.
  device_container_view_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  device_container_view_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  device_container_view_->SetBetweenChildSpacing(kDeviceContainerSeparator);

  // Create the container view to hold the permission rejected error.
  permission_rejected_view_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());

  if (show_devices) {
    ShowDevices();
  } else {
    UpdateVisibility();
  }
}

CastDeviceSelectorView::~CastDeviceSelectorView() = default;

///////////////////////////////////////////////////////////////////////////////
// global_media_controls::MediaItemUIDeviceSelector implementations:

void CastDeviceSelectorView::SetMediaItemUIUpdatedView(
    global_media_controls::MediaItemUIUpdatedView* view) {
  media_item_ui_updated_view_ = view;
}

void CastDeviceSelectorView::ShowDevices() {
  CHECK(!is_expanded_);
  is_expanded_ = true;
  UpdateVisibility();
}

void CastDeviceSelectorView::HideDevices() {
  CHECK(is_expanded_);
  is_expanded_ = false;
  UpdateVisibility();
}

bool CastDeviceSelectorView::IsDeviceSelectorExpanded() {
  return is_expanded_;
}

///////////////////////////////////////////////////////////////////////////////
// global_media_controls::mojom::DeviceListClient implementations:

void CastDeviceSelectorView::OnDevicesUpdated(
    std::vector<global_media_controls::mojom::DevicePtr> devices) {
  device_container_view_->RemoveAllChildViews();
  has_permission_rejected_issue_ = false;
  has_device_issue_ = false;
  for (const auto& device : devices) {
    auto device_view = BuildCastDeviceEntryView(
        base::BindRepeating(&CastDeviceSelectorView::OnCastDeviceSelected,
                            base::Unretained(this), device->id),
        device->icon, base::UTF8ToUTF16(device->name),
        base::UTF8ToUTF16(device->status_text));
    device_container_view_->AddChildView(std::move(device_view));
  }
  UpdateVisibility();
}

void CastDeviceSelectorView::OnPermissionRejected() {
  if (has_permission_rejected_issue_ ||
      g_browser_process->local_state()->GetBoolean(
          media_router::prefs::kSuppressLocalDiscoveryPermissionError)) {
    return;
  }
  has_permission_rejected_issue_ = true;
  media_router::MediaRouterMetrics::
      RecordMediaRouterUiPermissionRejectedViewEvents(
          media_router::MediaRouterUiPermissionRejectedViewEvents::
              kGmcDialogErrorShown);

  size_t offset;
  std::u16string settings_text_for_link = l10n_util::GetStringUTF16(
      IDS_MEDIA_ROUTER_LOCAL_DISCOVERY_PERMISSION_REJECTED_LINK);
  std::u16string label_text = l10n_util::GetStringFUTF16(
      IDS_MEDIA_ROUTER_LOCAL_DISCOVERY_PERMISSION_REJECTED_LABEL,
      settings_text_for_link, &offset);

  // TODO(crbug.com/359973625): Do not set the accessibility name for
  // `permission_rejected_view_` once AXPlatformNodeCocoa::AXBoundsForRange is
  // implemented.
  permission_rejected_view_->GetViewAccessibility().SetRole(
      ax::mojom::Role::kGroup);
  permission_rejected_view_->GetViewAccessibility().SetName(
      label_text, ax::mojom::NameFrom::kRelatedElement);
  permission_rejected_view_->SetFocusBehavior(
      views::View::FocusBehavior::ACCESSIBLE_ONLY);

  auto* permission_rejected_label_ = permission_rejected_view_->AddChildView(
      std::make_unique<views::StyledLabel>());
  permission_rejected_label_->SetBorder(
      views::CreateEmptyBorder(kCastHeaderRowInsets));
  permission_rejected_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  permission_rejected_label_->SetDefaultEnabledColorId(
      media_color_theme_.secondary_foreground_color_id);
  permission_rejected_label_->SetText(label_text);

#if BUILDFLAG(IS_MAC)
  base::RepeatingClosure open_settings_cb = base::BindRepeating([]() {
    // TODO(crbug.com/358725038): Open the Local Network sub-pane in system
    // settings directly once the feature request to Apple (FB14789617) is
    // solved.
    base::mac::OpenSystemSettingsPane(
        base::mac::SystemSettingsPane::kPrivacySecurity);
    media_router::MediaRouterMetrics::
        RecordMediaRouterUiPermissionRejectedViewEvents(
            media_router::MediaRouterUiPermissionRejectedViewEvents::
                kGmcDialogLinkClicked);
  });
  permission_rejected_label_->AddStyleRange(
      gfx::Range(offset, offset + settings_text_for_link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(open_settings_cb));
#endif

  UpdateVisibility();
}

///////////////////////////////////////////////////////////////////////////////
// CastDeviceSelectorView implementations:

std::unique_ptr<HoverButton> CastDeviceSelectorView::BuildCastDeviceEntryView(
    views::Button::PressedCallback callback,
    global_media_controls::mojom::IconType icon,
    const std::u16string& device_name,
    const std::u16string& status_text) {
  std::unique_ptr<HoverButton> device_entry_button;
  if (icon == global_media_controls::mojom::IconType::kThrobber) {
    // Create the device entry button with an animating throbber icon view.
    std::unique_ptr<views::Throbber> throbber =
        std::make_unique<views::Throbber>();
    throbber->Start();

    device_entry_button = std::make_unique<HoverButton>(
        std::move(callback), std::move(throbber), device_name);
    device_entry_button->SetBorder(
        views::CreateEmptyBorder(kThrobberHoverButtonInsets));
    device_entry_button->title()->SetDefaultTextStyle(
        views::style::STYLE_BODY_2);
    device_entry_button->title()->SetDefaultEnabledColorId(
        media_color_theme_.secondary_foreground_color_id);
  } else if (icon == global_media_controls::mojom::IconType::kInfo) {
    // Create the device entry button with a static info icon view, and
    // display the issue for the device in an error format.
    device_entry_button = std::make_unique<IssueHoverButton>(
        std::move(callback), icon, device_name, status_text,
        media_color_theme_.secondary_foreground_color_id,
        media_color_theme_.error_foreground_color_id);
    has_device_issue_ = true;
  } else {
    // Create the device entry button with a static icon view.
    device_entry_button = std::make_unique<HoverButton>(
        std::move(callback),
        ui::ImageModel::FromVectorIcon(
            GetVectorIcon(icon),
            media_color_theme_.secondary_foreground_color_id,
            kDeviceEntryIconSize),
        device_name);
    device_entry_button->SetBorder(
        views::CreateEmptyBorder(kIconHoverButtonInsets));
    device_entry_button->SetLabelStyle(views::style::STYLE_BODY_2);
    device_entry_button->SetEnabledTextColorIds(
        media_color_theme_.secondary_foreground_color_id);
    device_entry_button->SetImageLabelSpacing(kDeviceEntrySeparator);
  }
  device_entry_button->SetFocusRingCornerRadius(kDeviceEntryCornerRadius);
  return device_entry_button;
}

void CastDeviceSelectorView::OnCastDeviceSelected(
    const std::string& device_id) {
  if (device_list_host_) {
    device_list_host_->SelectDevice(device_id);
  }
}

void CastDeviceSelectorView::UpdateVisibility() {
  // Show the view if user requests to show the list and the device selector is
  // available.
  SetVisible(is_expanded_ && IsDeviceSelectorAvailable());

  device_container_view_->SetVisible(!has_permission_rejected_issue_);
  permission_rejected_view_->SetVisible(has_permission_rejected_issue_);

  // Visibility changes can result in size changes, which should change sizes of
  // parent views too.
  PreferredSizeChanged();

  // Update the casting state on the parent view.
  if (media_item_ui_updated_view_) {
    media_item_ui_updated_view_->UpdateDeviceSelectorAvailability(
        IsDeviceSelectorAvailable());
    media_item_ui_updated_view_->UpdateDeviceSelectorIssue(
        has_device_issue_ || has_permission_rejected_issue_);
    media_item_ui_updated_view_->UpdateDeviceSelectorVisibility(is_expanded_);
  }
}

void CastDeviceSelectorView::CloseButtonPressed() {
  base::UmaHistogramEnumeration(
      global_media_controls::kMediaItemUIUpdatedViewActionHistogram,
      global_media_controls::MediaItemUIUpdatedViewAction::
          kCloseDeviceListForCasting);
  if (has_permission_rejected_issue_) {
    media_router::MediaRouterMetrics::
        RecordMediaRouterUiPermissionRejectedViewEvents(
            media_router::MediaRouterUiPermissionRejectedViewEvents::
                kGmcDialogErrorDismissed);
    g_browser_process->local_state()->SetBoolean(
        media_router::prefs::kSuppressLocalDiscoveryPermissionError, true);
    has_permission_rejected_issue_ = false;
  }

  HideDevices();
}

bool CastDeviceSelectorView::IsDeviceSelectorAvailable() {
  return !device_container_view_->children().empty() ||
         has_permission_rejected_issue_;
}
///////////////////////////////////////////////////////////////////////////////
// Helper functions for testing:

bool CastDeviceSelectorView::GetHasDeviceIssueForTesting() {
  return has_device_issue_;
}

global_media_controls::MediaActionButton*
CastDeviceSelectorView::GetCloseButtonForTesting() {
  return close_button_;
}

views::View* CastDeviceSelectorView::GetDeviceContainerViewForTesting() {
  return device_container_view_;
}

views::View* CastDeviceSelectorView::GetPermissionRejectedViewForTesting() {
  return permission_rejected_view_;
}

BEGIN_METADATA(CastDeviceSelectorView)
END_METADATA
