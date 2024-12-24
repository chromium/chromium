// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

using collaboration::messaging::ActivityLogItem;
using collaboration::messaging::ActivityLogQueryParams;
using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessagingBackendServiceFactory;

namespace {

// Unicode value for a bullet point.
constexpr std::u16string kBulletPoint = u"\u2022";

// Gets the string for the activity line to describe an event.
std::u16string GetActivityText(ActivityLogItem item) {
  auto name = item.user_is_self
                  ? l10n_util::GetStringUTF16(IDS_DATA_SHARING_YOU)
                  : base::UTF8ToUTF16(item.user_display_name);
  int message_id;

  switch (item.collaboration_event) {
    case CollaborationEvent::TAB_UPDATED:
      message_id = IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_CHANGED_TAB;
      break;
    case CollaborationEvent::TAB_REMOVED:
      message_id = IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_REMOVED_TAB;
      break;
    case CollaborationEvent::TAB_ADDED:
      message_id = IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_ADDED_TAB;
      break;
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
      message_id = IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_JOINED_GROUP;
      break;
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      message_id = IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_LEFT_GROUP;
      break;
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
      message_id = IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_CHANGED_GROUP_NAME;
      break;
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
      message_id = IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_CHANGED_GROUP_COLOR;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringFUTF16(message_id, name);
}

// Returns the correct user that should be used for a given log item.
// Sometimes the string should describe the user that triggered an event
// and sometimes it should describe the user that was affected by the event.
data_sharing::GroupMember GetRelevantUserForActivity(ActivityLogItem item) {
  std::optional<data_sharing::GroupMember> user;
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
    default:
      NOTREACHED();
  }
  CHECK(user.has_value());
  return user.value();
}

// Enum used to show the correct selector variant of the string template.
enum class TimeDimension {
  kMinutes = 0,
  kHours = 1,
  kDays = 2,
  kMaxValue = kDays,
};

// Gets the string representation of the given time delta. Time is
// binned into minutes, hours, or days.
std::u16string GetElapsedTimeText(base::TimeDelta time_delta) {
  TimeDimension dimension;
  int number;
  if (time_delta < base::Hours(1)) {
    dimension = TimeDimension::kMinutes;
    number = time_delta.InMinutes();
  } else if (time_delta < base::Days(1)) {
    dimension = TimeDimension::kHours;
    number = time_delta.InHours();
  } else {
    dimension = TimeDimension::kDays;
    number = time_delta.InDays();
  }
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringFUTF16(
          IDS_DATA_SHARING_RECENT_ACTIVITY_TIME,
          base::UTF8ToUTF16(base::NumberToString(number))),
      static_cast<int>(dimension));
}

// Gets the string for the metadata line to describe an event.
std::u16string GetMetadataText(ActivityLogItem item) {
  if (item.description == u"") {
    // If there is no description, the line simply contains elapsed time
    // since the action.
    return GetElapsedTimeText(item.time_delta);
  } else {
    // The metadata line contains the item's description, a bullet point,
    // and the elapsed time since the action, separated by spaces.
    std::u16string_view separator = u" ";
    return base::JoinString(
        {item.description, kBulletPoint, GetElapsedTimeText(item.time_delta)},
        separator);
  }
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kRecentActivityBubbleDialogId);

RecentActivityBubbleDialogView::RecentActivityBubbleDialogView(
    View* anchor_view,
    content::WebContents* web_contents,
    std::vector<ActivityLogItem> activity_log,
    Profile* profile)
    : LocationBarBubbleDelegateView(anchor_view, web_contents) {
  SetProperty(views::kElementIdentifierKey, kRecentActivityBubbleDialogId);
  SetTitle(l10n_util::GetStringUTF16(IDS_DATA_SHARING_RECENT_ACTIVITY_TITLE));
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  // Activity log should never be empty. This bubble dialog is triggered
  // by an entrypoint that only exists if a message was delivered.
  CHECK(!activity_log.empty());
  auto num_rows =
      std::min(static_cast<int>(activity_log.size()), kMaxNumberRows);
  for (int i = 0; i < num_rows; i++) {
    auto item = activity_log.at(i);
    AddChildView(std::make_unique<RecentActivityRowView>(item, profile));
  }
}

RecentActivityBubbleDialogView::~RecentActivityBubbleDialogView() = default;

RecentActivityRowView* RecentActivityBubbleDialogView::GetRowForTesting(int n) {
  CHECK(n < static_cast<int>(children().size()));
  return static_cast<RecentActivityRowView*>(children().at(n));
}

BEGIN_METADATA(RecentActivityBubbleDialogView)
END_METADATA

RecentActivityRowView::RecentActivityRowView(ActivityLogItem item,
                                             Profile* profile) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kHorizontal))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetProperty(views::kMarginsKey, ChromeLayoutProvider::Get()->GetInsetsMetric(
                                      INSETS_RECENT_ACTIVITY_ROW_MARGIN));

  AddChildView(std::make_unique<RecentActivityRowImageView>(item, profile));

  auto* label_container = AddChildView(std::make_unique<views::View>());
  label_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kVertical));

  activity_text_ = GetActivityText(item);
  auto* activity_label =
      label_container->AddChildView(std::make_unique<views::Label>());
  activity_label->SetText(activity_text_);
  activity_label->SetTextStyle(views::style::TextStyle::STYLE_PRIMARY);
  activity_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  metadata_text_ = GetMetadataText(item);
  auto* metadata_label =
      label_container->AddChildView(std::make_unique<views::Label>());
  metadata_label->SetText(metadata_text_);
  metadata_label->SetTextStyle(views::style::TextStyle::STYLE_CAPTION);
  metadata_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
}

RecentActivityRowView::~RecentActivityRowView() = default;

BEGIN_METADATA(RecentActivityRowView)
END_METADATA

RecentActivityRowImageView::RecentActivityRowImageView(ActivityLogItem item,
                                                       Profile* profile)
    : item_(item), profile_(profile) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  SetProperty(views::kMarginsKey, ChromeLayoutProvider::Get()->GetInsetsMetric(
                                      INSETS_RECENT_ACTIVITY_IMAGE_MARGIN));
  int size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RECENT_ACTIVITY_AVATAR_SIZE);
  SetPreferredSize(gfx::Size(size, size));

  avatar_image_ = AddChildView(std::make_unique<views::ImageView>());
  FetchAvatar();

  // TODO(crbug.com/384466393): Add favicon to RecentActivity dialog
}

RecentActivityRowImageView::~RecentActivityRowImageView() = default;

void RecentActivityRowImageView::FetchAvatar() {
  auto avatar_url = GetRelevantUserForActivity(item_).avatar_url;

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
      avatar_url,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RECENT_ACTIVITY_AVATAR_SIZE),
      base::BindOnce(&RecentActivityRowImageView::SetAvatar,
                     base::Unretained(this)),
      image_fetcher_service->GetImageFetcher(
          image_fetcher::ImageFetcherConfig::kDiskCacheOnly));
}

void RecentActivityRowImageView::SetAvatar(const gfx::Image& avatar) {
  avatar_image_->SetImage(ui::ImageModel::FromImage(avatar));
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

void RecentActivityBubbleCoordinator::Show(
    views::View* anchor_view,
    content::WebContents* web_contents,
    std::vector<ActivityLogItem> activity_log,
    Profile* profile) {
  DCHECK(!tracker_.view());
  auto bubble = std::make_unique<RecentActivityBubbleDialogView>(
      anchor_view, web_contents, activity_log, profile);
  tracker_.SetView(bubble.get());
  auto* widget =
      RecentActivityBubbleDialogView::CreateBubble(std::move(bubble));
  bubble_widget_observation_.Observe(widget);
  widget->Show();
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
