// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"

#include "base/i18n/message_formatter.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/collaboration_messaging_page_action_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

using collaboration::messaging::ActivityLogItem;
using collaboration::messaging::TabGroupMessageMetadata;
using collaboration::messaging::TabMessageMetadata;

namespace {

// Unicode value for a bullet point.
constexpr std::u16string kBulletPoint = u"\u2022";

// Returns the correct user that should be used for a given log item.
// Sometimes the string should describe the user that triggered an event
// and sometimes it should describe the user that was affected by the event.
std::optional<data_sharing::GroupMember> GetRelevantUserForActivity(
    const ActivityLogItem& item) {
  using collaboration::messaging::CollaborationEvent;

  std::optional<data_sharing::GroupMember> user = std::nullopt;
  switch (item.collaboration_event) {
    case CollaborationEvent::TAB_UPDATED:
    case CollaborationEvent::TAB_REMOVED:
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
      // Tab and group related events should show the triggering user.
      user = item.activity_metadata.triggering_user;
      break;
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      // Membership changes should show the affected user.
      user = item.activity_metadata.affected_user;
      break;
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_REMOVED:
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
    case CollaborationEvent::UNDEFINED:
      NOTREACHED();
  }
  return user;
}

// Get the string for the title line to describe the action.
std::u16string GetTitleText(const ActivityLogItem& item, bool is_current_tab) {
  if (is_current_tab) {
    // TODO(crbug.com/396122264): This string should be updated to read
    // "this tab" instead of "a tab".
  }
  return item.title_text;
}

// Gets the string for the metadata line to describe an event.
std::u16string GetMetadataText(const ActivityLogItem& item) {
  if (item.description_text == u"") {
    // If there is no description, the line simply contains elapsed time
    // since the action.
    return item.time_delta_text;
  } else {
    // The metadata line contains the item's description, a bullet point,
    // and the elapsed time since the action, separated by spaces.
    std::u16string_view separator = u" ";
    return base::JoinString(
        {item.description_text, kBulletPoint, item.time_delta_text}, separator);
  }
}

// TODO(crbug.com/392150086): Refactor this into utilities.
std::optional<tab_groups::LocalTabGroupID> UnwrapGroupId(
    const ActivityLogItem& item) {
  if (std::optional<TabGroupMessageMetadata> tab_group_metadata =
          item.activity_metadata.tab_group_metadata) {
    return tab_group_metadata->local_tab_group_id;
  }
  return std::nullopt;
}

// TODO(crbug.com/392150086): Refactor this into utilities.
std::optional<tab_groups::LocalTabID> UnwrapTabId(const ActivityLogItem& item) {
  if (std::optional<TabMessageMetadata> tab_metadata =
          item.activity_metadata.tab_metadata) {
    return tab_metadata->local_tab_id;
  }
  return std::nullopt;
}

// TODO(crbug.com/392150086): Refactor this into utilities.
std::optional<std::string> UnwrapTabUrl(const ActivityLogItem& item) {
  if (std::optional<TabMessageMetadata> tab_metadata =
          item.activity_metadata.tab_metadata) {
    return tab_metadata->last_known_url;
  }
  return std::nullopt;
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kRecentActivityBubbleDialogId);

RecentActivityBubbleDialogView::RecentActivityBubbleDialogView(
    View* anchor_view,
    content::WebContents* web_contents,
    std::optional<int> current_tab_activity_index,
    std::vector<ActivityLogItem> activity_log,
    Profile* profile)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      activity_log_(activity_log),
      current_tab_activity_index_(current_tab_activity_index),
      profile_(profile) {
  SetProperty(views::kElementIdentifierKey, kRecentActivityBubbleDialogId);
  SetTitle(l10n_util::GetStringUTF16(IDS_DATA_SHARING_RECENT_ACTIVITY_TITLE));
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  if (activity_log.empty()) {
    CreateEmptyState();
  }

  CreateTabActivity();
  CreateGroupActivity();
}

RecentActivityBubbleDialogView::~RecentActivityBubbleDialogView() = default;

void RecentActivityBubbleDialogView::CreateEmptyState() {
  // No activity to show. Fill in the empty state label and return early.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DATA_SHARING_RECENT_ACTIVITY_NO_UPDATES),
      views::style::TextContext::CONTEXT_TABLE_ROW));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetTextStyle(views::style::TextStyle::STYLE_BODY_3_MEDIUM);
}

void RecentActivityBubbleDialogView::CreateTabActivity() {
  bool should_show_tab_activity =
      !activity_log_.empty() && current_tab_activity_index_.has_value();

  // If an index is supplied, show this element in a separate container
  // to highlight it was the last action on the current tab.
  // Tab activity container label.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_DATA_SHARING_RECENT_ACTIVITY_LATEST_UPDATE_TAB),
      views::style::TextContext::CONTEXT_TABLE_ROW));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetTextStyle(views::style::TextStyle::STYLE_BODY_3_MEDIUM);
  label->SetVisible(should_show_tab_activity);

  const int activity_container_radius =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_CONTAINER_RADIUS);

  // Tab activity container.
  tab_activity_container_ = AddChildView(std::make_unique<views::View>());
  tab_activity_container_->SetVisible(should_show_tab_activity);
  tab_activity_container_
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true);
  tab_activity_container_->SetBackground(
      views::CreateThemedRoundedRectBackground(
          kColorSharingRecentActivityDialogActivityContainer,
          activity_container_radius));
  tab_activity_container_->SetProperty(
      views::kMarginsKey, ChromeLayoutProvider::Get()->GetInsetsMetric(
                              INSETS_RECENT_ACTIVITY_TAB_CONTAINER_MARGIN));

  if (!should_show_tab_activity) {
    // Skip creating the content if there is no tab activity to show.
    return;
  }

  tab_activity_container_->AddChildView(std::make_unique<RecentActivityRowView>(
      activity_log_.at(current_tab_activity_index_.value()),
      /*is_current_tab=*/true, profile_,
      base::BindOnce(&RecentActivityBubbleDialogView::Close,
                     weak_factory_.GetWeakPtr())));
}

void RecentActivityBubbleDialogView::CreateGroupActivity() {
  bool should_show_group_activity = !activity_log_.empty();

  const int activity_container_radius =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_CONTAINER_RADIUS);

  // Group activity container label.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_DATA_SHARING_RECENT_ACTIVITY_LATEST_UPDATE_GROUP),
      views::style::TextContext::CONTEXT_TABLE_ROW));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetTextStyle(views::style::TextStyle::STYLE_BODY_3_MEDIUM);
  label->SetVisible(should_show_group_activity);

  // Group activity container.
  group_activity_container_ = AddChildView(std::make_unique<views::View>());
  group_activity_container_->SetVisible(should_show_group_activity);
  group_activity_container_
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true);
  group_activity_container_->SetBackground(
      views::CreateThemedRoundedRectBackground(
          kColorSharingRecentActivityDialogActivityContainer,
          activity_container_radius));
  group_activity_container_->SetProperty(
      views::kMarginsKey, ChromeLayoutProvider::Get()->GetInsetsMetric(
                              INSETS_RECENT_ACTIVITY_GROUP_CONTAINER_MARGIN));

  const auto num_rows =
      std::min(static_cast<int>(activity_log_.size()), kMaxNumberRows);
  for (int i = 0; i < num_rows; i++) {
    // If an index is supplied, skip the corresponding element since it
    // will be shown in the tab_activity_container.
    if (current_tab_activity_index_.has_value() &&
        i == current_tab_activity_index_.value()) {
      continue;
    }
    group_activity_container_->AddChildView(
        std::make_unique<RecentActivityRowView>(
            activity_log_.at(i), /*is_current_tab=*/false, profile_,
            base::BindOnce(&RecentActivityBubbleDialogView::Close,
                           weak_factory_.GetWeakPtr())));
  }
}

void RecentActivityBubbleDialogView::Close() {
  LocationBarBubbleDelegateView::CloseBubble();
}

RecentActivityRowView* RecentActivityBubbleDialogView::GetRowForTesting(int n) {
  int tab_activity_size = tab_activity_container()->children().size();
  int group_activity_size = group_activity_container()->children().size();

  CHECK(n < (tab_activity_size + group_activity_size));

  if (n < tab_activity_size) {
    return static_cast<RecentActivityRowView*>(
        tab_activity_container()->children().at(n));
  }
  return static_cast<RecentActivityRowView*>(
      group_activity_container()->children().at(n - tab_activity_size));
}

BEGIN_METADATA(RecentActivityBubbleDialogView)
END_METADATA

RecentActivityRowView::RecentActivityRowView(
    ActivityLogItem item,
    bool is_current_tab,
    Profile* profile,
    base::OnceCallback<void()> close_callback)
    : item_(item),
      profile_(profile),
      close_callback_(std::move(close_callback)) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kHorizontal))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetProperty(views::kMarginsKey, ChromeLayoutProvider::Get()->GetInsetsMetric(
                                      INSETS_RECENT_ACTIVITY_ROW_MARGIN));
  GetViewAccessibility().SetRole(ax::mojom::Role::kRow);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_DATA_SHARING_RECENT_ACTIVITY_TITLE));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  image_view_ =
      AddChildView(std::make_unique<RecentActivityRowImageView>(item, profile));

  auto* label_container = AddChildView(std::make_unique<views::View>());
  label_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kVertical));

  activity_text_ = GetTitleText(item, is_current_tab);
  auto* activity_label =
      label_container->AddChildView(std::make_unique<views::Label>());
  activity_label->SetText(activity_text_);
  activity_label->SetTextStyle(views::style::TextStyle::STYLE_BODY_4_MEDIUM);
  activity_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  metadata_text_ = GetMetadataText(item);
  auto* metadata_label =
      label_container->AddChildView(std::make_unique<views::Label>());
  metadata_label->SetText(metadata_text_);
  metadata_label->SetTextStyle(views::style::TextStyle::STYLE_BODY_5);
  metadata_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
}

RecentActivityRowView::~RecentActivityRowView() = default;

bool RecentActivityRowView::OnMousePressed(const ui::MouseEvent& event) {
  using collaboration::messaging::RecentActivityAction;

  switch (item_.action) {
    case RecentActivityAction::kFocusTab:
      FocusTab();
      break;
    case RecentActivityAction::kReopenTab:
      ReopenTab();
      break;
    case RecentActivityAction::kOpenTabGroupEditDialog:
      OpenTabGroupEditDialog();
      break;
    case RecentActivityAction::kManageSharing:
      ManageSharing();
      break;
    case RecentActivityAction::kNone:
      break;
  }

  std::move(close_callback_).Run();

  return true;
}

void RecentActivityRowView::FocusTab() {
  std::optional<tab_groups::LocalTabGroupID> group_id = UnwrapGroupId(item_);
  std::optional<tab_groups::LocalTabID> tab_id = UnwrapTabId(item_);
  if (!group_id.has_value() || !tab_id.has_value()) {
    return;
  }

  // Find the grouped tab and activate it within the tab strip model.
  tabs::TabInterface* tab = tab_groups::SavedTabGroupUtils::GetGroupedTab(
      group_id.value(), tab_id.value());
  if (!tab) {
    // Early return in the case that this tab has already been closed.
    return;
  }
  TabStripModel* tab_strip_model =
      tab->GetBrowserWindowInterface()->GetTabStripModel();
  tab_strip_model->ActivateTabAt(tab_strip_model->GetIndexOfTab(tab));
}

void RecentActivityRowView::ReopenTab() {
  std::optional<tab_groups::LocalTabGroupID> group_id = UnwrapGroupId(item_);
  std::optional<std::string> tab_url = UnwrapTabUrl(item_);
  if (!group_id.has_value() || !tab_url.has_value()) {
    return;
  }

  if (auto* browser = tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
          group_id.value())) {
    tab_groups::SavedTabGroupUtils::OpenTabInBrowser(
        GURL(tab_url.value()), browser, browser->profile(),
        WindowOpenDisposition::NEW_BACKGROUND_TAB, std::nullopt,
        group_id.value());
  }
}

void RecentActivityRowView::OpenTabGroupEditDialog() {
  std::optional<tab_groups::LocalTabGroupID> group_id = UnwrapGroupId(item_);
  if (!group_id.has_value()) {
    return;
  }

  auto* browser = tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
      group_id.value());
  if (!browser) {
    return;
  }

  if (auto* tab_group_header = BrowserView::GetBrowserViewForBrowser(browser)
                                   ->tabstrip()
                                   ->group_header(group_id.value())) {
    TabGroupEditorBubbleView::Show(browser, group_id.value(), tab_group_header);
  }
}

void RecentActivityRowView::ManageSharing() {
  std::optional<tab_groups::LocalTabGroupID> group_id = UnwrapGroupId(item_);
  if (!group_id.has_value()) {
    return;
  }

  if (auto* browser = tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
          group_id.value())) {
    DataSharingBubbleController::GetOrCreateForBrowser(browser)->Show(
        group_id.value());
  }
}

BEGIN_METADATA(RecentActivityRowView)
END_METADATA

RecentActivityRowImageView::RecentActivityRowImageView(ActivityLogItem item,
                                                       Profile* profile)
    : item_(item), profile_(profile) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  const int avatar_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RECENT_ACTIVITY_AVATAR_SIZE);
  const int favicon_container_radius =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_RADIUS);
  const int favicon_container_offset =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_OFFSET_FROM_AVATAR);

  // The favicon container hangs halfway off the avatar image less the
  // container offset, which moves the container towards the avatar image.
  const int favicon_container_x_overhang =
      favicon_container_radius - favicon_container_offset;

  // The favicon container aligns at the bottom of the avatar before
  // being moved down by the container offset.
  const int favicon_container_y_overhang = favicon_container_offset;

  // The complete dimensions for the avatar/favicon include the avatar
  // image diameter plus the overhang of the favicon container.
  SetPreferredSize(gfx::Size(avatar_size + favicon_container_x_overhang,
                             avatar_size + favicon_container_y_overhang));

  // The margin between the avatar image and labels should ignore the
  // space taken up by the favicon container.
  gfx::Insets margins = ChromeLayoutProvider::Get()->GetInsetsMetric(
      INSETS_RECENT_ACTIVITY_IMAGE_MARGIN);
  margins.set_right(margins.right() - favicon_container_x_overhang);
  SetProperty(views::kMarginsKey, margins);

  FetchAvatar();
  if (item_.show_favicon) {
    FetchFavicon();
  }
}

RecentActivityRowImageView::~RecentActivityRowImageView() = default;

void RecentActivityRowImageView::FetchAvatar() {
  auto user = GetRelevantUserForActivity(item_);
  if (!user.has_value()) {
    return;
  }

  image_fetcher::ImageFetcherService* image_fetcher_service =
      ImageFetcherServiceFactory::GetForKey(profile_->GetProfileKey());
  if (!image_fetcher_service) {
    return;
  }

  data_sharing::DataSharingService* const data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile_);
  if (!data_sharing_service) {
    return;
  }

  data_sharing_service->GetAvatarImageForURL(
      user->avatar_url, signin::kAccountInfoImageSize,
      base::BindOnce(&RecentActivityRowImageView::SetAvatar,
                     weak_factory_.GetWeakPtr()),
      image_fetcher_service->GetImageFetcher(
          image_fetcher::ImageFetcherConfig::kDiskCacheOnly));
}

void RecentActivityRowImageView::SetAvatar(const gfx::Image& avatar) {
  const int avatar_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RECENT_ACTIVITY_AVATAR_SIZE);
  avatar_image_ = gfx::ImageSkiaOperations::CreateResizedImage(
      avatar.AsImageSkia(), skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
      gfx::Size(avatar_size, avatar_size));
  SchedulePaint();
}

void RecentActivityRowImageView::FetchFavicon() {
  if (auto tab_metadata = item_.activity_metadata.tab_metadata) {
    if (auto url = tab_metadata->last_known_url) {
      // Note: Favicons are only loaded if they exist in the favicon
      // database, i.e. you've visited this site before.
      // TODO(crbug.com/386766083): Fallback to host for loading favicons
      favicon::FaviconService* favicon_service =
          FaviconServiceFactory::GetForProfile(
              profile_, ServiceAccessType::EXPLICIT_ACCESS);

      favicon_service->GetFaviconImageForPageURL(
          GURL(url.value()),
          base::BindOnce(&RecentActivityRowImageView::SetFavicon,
                         weak_factory_.GetWeakPtr()),
          &favicon_fetching_task_tracker_);
    }
  }
}

void RecentActivityRowImageView::SetFavicon(
    const favicon_base::FaviconImageResult& favicon) {
  const int favicon_container_radius =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_RADIUS);
  const int favicon_container_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_PADDING);

  // Diameter of the favicon after resizing to fit the container.
  const int resized_favicon_size =
      (favicon_container_radius - favicon_container_padding) * 2;

  // Resize the favicon image to fit in the circle.
  resized_favicon_image_ = gfx::ImageSkiaOperations::CreateResizedImage(
      favicon.image.AsImageSkia(),
      skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
      gfx::Size(resized_favicon_size, resized_favicon_size));

  SchedulePaint();
}

void RecentActivityRowImageView::PaintFavicon(gfx::Canvas* canvas,
                                              gfx::Rect avatar_bounds) {
  const int favicon_container_radius =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_RADIUS);
  const int favicon_container_border_width =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_BORDER_WIDTH);
  const int favicon_container_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_PADDING);
  const int favicon_container_offset =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_FAVICON_CONTAINER_OFFSET_FROM_AVATAR);

  // Radius of the favicon to fit the container.
  const int resized_favicon_radius =
      favicon_container_radius - favicon_container_padding;

  // Diameter of the favicon.
  const int resized_favicon_size = resized_favicon_radius * 2;

  // The favicon container, its border, and the favicon image all center
  // around this point.
  gfx::Point favicon_center = base::i18n::IsRTL()
                                  ? avatar_bounds.bottom_left()
                                  : avatar_bounds.bottom_right();

  // Offset favicon center so avatar and favicon container align at bottom edge.
  favicon_center.Offset(0, -favicon_container_radius);

  // Additional favicon container offset from avatar.
  favicon_center.Offset(
      // Move x value toward avatar center (rtl: toward right, ltr: toward
      // left).
      base::i18n::IsRTL() ? favicon_container_offset
                          : -favicon_container_offset,
      // Move y value away from avatar.
      favicon_container_offset);

  // Clear a circle in the avatar to fit the favicon container with an
  // empty border.
  cc::PaintFlags clear_flags;
  clear_flags.setAntiAlias(true);
  clear_flags.setBlendMode(SkBlendMode::kClear);
  canvas->DrawCircle(favicon_center,
                     favicon_container_radius + favicon_container_border_width,
                     clear_flags);
  // Restore the previously saved background.
  canvas->Restore();

  // Draw the favicon container with a background.
  cc::PaintFlags indicator_flags;
  indicator_flags.setColor(GetColorProvider()->GetColor(
      kColorSharingRecentActivityDialogFaviconContainer));
  indicator_flags.setAntiAlias(true);
  canvas->DrawCircle(favicon_center, favicon_container_radius, indicator_flags);

  // Set the bounds of the favicon based off the center point.
  gfx::Rect resized_favicon_bounds(favicon_center.x() - resized_favicon_radius,
                                   favicon_center.y() - resized_favicon_radius,
                                   resized_favicon_size, resized_favicon_size);

  // Draw the resized favicon image.
  canvas->DrawImageInt(
      resized_favicon_image_, 0, 0, resized_favicon_size, resized_favicon_size,
      resized_favicon_bounds.x(), resized_favicon_bounds.y(),
      resized_favicon_bounds.width(), resized_favicon_bounds.height(), false);
}

void RecentActivityRowImageView::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldShowAvatar()) {
    // Nothing should be painted as the avatar is loading.
    return;
  }

  gfx::Rect contents_bounds = GetContentsBounds();
  const int avatar_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RECENT_ACTIVITY_AVATAR_SIZE);

  // Set the bounds of the avatar based off the container.
  gfx::Rect avatar_bounds(contents_bounds.x(), contents_bounds.y(), avatar_size,
                          avatar_size);

  // Save background layer to be used in favicon container border.
  canvas->SaveLayerAlpha(0xff);

  // Draw the avatar image.
  canvas->DrawImageInt(avatar_image_, 0, 0, avatar_size, avatar_size,
                       avatar_bounds.x(), avatar_bounds.y(),
                       avatar_bounds.width(), avatar_bounds.height(), false);

  if (ShouldShowFavicon()) {
    PaintFavicon(canvas, avatar_bounds);
  }
}

BEGIN_METADATA(RecentActivityRowImageView)
END_METADATA

// RecentActivityBubbleCoordinator
RecentActivityBubbleCoordinator::RecentActivityBubbleCoordinator() = default;
RecentActivityBubbleCoordinator::~RecentActivityBubbleCoordinator() = default;

void RecentActivityBubbleCoordinator::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK(bubble_widget_observation_.IsObservingSource(widget));
  bubble_widget_observation_.Reset();
}

void RecentActivityBubbleCoordinator::ShowCommon(
    std::unique_ptr<RecentActivityBubbleDialogView> bubble) {
  DCHECK(!tracker_.view());
  tracker_.SetView(bubble.get());
  auto* widget =
      RecentActivityBubbleDialogView::CreateBubble(std::move(bubble));
  bubble_widget_observation_.Observe(widget);
  widget->Show();

  tab_groups::saved_tab_groups::metrics::RecordSharedTabGroupManageType(
      tab_groups::saved_tab_groups::metrics::SharedTabGroupManageTypeDesktop::
          kRecentActivity);
}

void RecentActivityBubbleCoordinator::Show(
    views::View* anchor_view,
    content::WebContents* web_contents,
    std::vector<ActivityLogItem> activity_log,
    Profile* profile) {
  auto bubble = std::make_unique<RecentActivityBubbleDialogView>(
      anchor_view, web_contents, std::nullopt, activity_log, profile);
  bubble->SetArrow(views::BubbleBorder::Arrow::TOP_LEFT);

  RecentActivityBubbleCoordinator::ShowCommon(std::move(bubble));
}

void RecentActivityBubbleCoordinator::ShowForCurrentTab(
    views::View* anchor_view,
    content::WebContents* web_contents,
    std::vector<ActivityLogItem> activity_log,
    Profile* profile) {
  tab_groups::LocalTabID tab_id =
      tabs::TabInterface::GetFromContents(web_contents)
          ->GetHandle()
          .raw_value();
  // Find the first activity item for this tab, if any.
  auto it = std::find_if(activity_log.begin(), activity_log.end(),
                         [&tab_id](const ActivityLogItem& item) -> bool {
                           std::optional<TabMessageMetadata> tab_metadata =
                               item.activity_metadata.tab_metadata;
                           return tab_metadata.has_value() &&
                                  tab_metadata->local_tab_id == tab_id;
                         });

  std::optional<int> index;
  if (it != activity_log.end()) {
    index = std::distance(activity_log.begin(), it);
  }

  auto bubble = std::make_unique<RecentActivityBubbleDialogView>(
      anchor_view, web_contents, index, activity_log, profile);
  bubble->SetArrow(views::BubbleBorder::Arrow::TOP_RIGHT);
  RecentActivityBubbleCoordinator::ShowCommon(std::move(bubble));
}

void RecentActivityBubbleCoordinator::Hide() {
  if (IsShowing()) {
    tracker_.view()->GetWidget()->Close();
  }
  tracker_.SetView(nullptr);
}

RecentActivityBubbleDialogView* RecentActivityBubbleCoordinator::GetBubble()
    const {
  return tracker_.view() ? views::AsViewClass<RecentActivityBubbleDialogView>(
                               const_cast<views::View*>(tracker_.view()))
                         : nullptr;
}

bool RecentActivityBubbleCoordinator::IsShowing() {
  return tracker_.view() != nullptr;
}
