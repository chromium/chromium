// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_view.h"

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/media_message_center/media_notification_background.h"
#include "components/media_message_center/media_notification_constants.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;

namespace {

// The number of actions supported when the notification is expanded or not.
constexpr size_t kMediaNotificationActionsCount = 3;
constexpr size_t kMediaNotificationExpandedActionsCount = 5;

// Dimensions.
constexpr int kDefaultMarginSize = 8;
constexpr int kMediaButtonIconSize = 28;
constexpr int kTitleArtistLineHeight = 20;
constexpr double kMediaImageMaxWidthPct = 0.3;
constexpr double kMediaImageMaxWidthExpandedPct = 0.4;
constexpr gfx::Size kMediaButtonSize = gfx::Size(36, 36);
constexpr int kMediaButtonRowSeparator = 8;
constexpr gfx::Insets kMediaTitleArtistInsets = gfx::Insets(8, 8, 0, 8);
constexpr gfx::Insets kIconlessMediaNotificationHeaderInsets =
    gfx::Insets(6, 14, 0, 6);
constexpr gfx::Insets kIconMediaNotificationHeaderInsets =
    gfx::Insets(6, 0, 0, 6);
constexpr gfx::Size kMediaNotificationButtonRowSize =
    gfx::Size(124, kMediaButtonSize.height());

void RecordMetadataHistogram(MediaNotificationView::Metadata metadata) {
  UMA_HISTOGRAM_ENUMERATION(MediaNotificationView::kMetadataHistogramName,
                            metadata);
}

const gfx::VectorIcon* GetVectorIconForMediaAction(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      return &vector_icons::kMediaPreviousTrackIcon;
    case MediaSessionAction::kSeekBackward:
      return &vector_icons::kMediaSeekBackwardIcon;
    case MediaSessionAction::kPlay:
      return &vector_icons::kPlayArrowIcon;
    case MediaSessionAction::kPause:
      return &vector_icons::kPauseIcon;
    case MediaSessionAction::kSeekForward:
      return &vector_icons::kMediaSeekForwardIcon;
    case MediaSessionAction::kNextTrack:
      return &vector_icons::kMediaNextTrackIcon;
    case MediaSessionAction::kStop:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
      NOTREACHED();
      break;
  }

  return nullptr;
}

size_t GetMaxNumActions(bool expanded) {
  return expanded ? kMediaNotificationExpandedActionsCount
                  : kMediaNotificationActionsCount;
}

}  // namespace

// static
const char MediaNotificationView::kArtworkHistogramName[] =
    "Media.Notification.ArtworkPresent";

// static
const char MediaNotificationView::kMetadataHistogramName[] =
    "Media.Notification.MetadataPresent";

MediaNotificationView::MediaNotificationView(
    MediaNotificationContainer* container,
    base::WeakPtr<MediaNotificationItem> item,
    std::unique_ptr<views::View> header_row_controls_view,
    const base::string16& default_app_name,
    int notification_width,
    bool should_show_icon)
    : container_(container),
      item_(std::move(item)),
      default_app_name_(default_app_name),
      notification_width_(notification_width) {
  DCHECK(container_);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  auto header_row =
      std::make_unique<message_center::NotificationHeaderView>(this);

  if (header_row_controls_view) {
    header_row_controls_view_ =
        header_row->AddChildView(std::move(header_row_controls_view));
  }

  header_row->SetAppName(default_app_name_);

  if (should_show_icon) {
    header_row->ClearAppIcon();
    header_row->SetProperty(views::kMarginsKey,
                            kIconMediaNotificationHeaderInsets);
  } else {
    header_row->HideAppIcon();
    header_row->SetProperty(views::kMarginsKey,
                            kIconlessMediaNotificationHeaderInsets);
  }

  header_row_ = AddChildView(std::move(header_row));

  // |main_row_| holds the main content of the notification.
  auto main_row = std::make_unique<views::View>();
  main_row_ = AddChildView(std::move(main_row));

  // |title_artist_row_| contains the title and artist labels.
  auto title_artist_row = std::make_unique<views::View>();
  title_artist_row_layout_ =
      title_artist_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kMediaTitleArtistInsets,
          0));
  title_artist_row_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  title_artist_row_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  title_artist_row_ = main_row_->AddChildView(std::move(title_artist_row));

  auto title_label = std::make_unique<views::Label>(
      base::string16(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  title_label->SetFontList(base_font_list.Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::MEDIUM));
  title_label->SetLineHeight(kTitleArtistLineHeight);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_ = title_artist_row_->AddChildView(std::move(title_label));

  auto artist_label = std::make_unique<views::Label>(
      base::string16(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  artist_label->SetLineHeight(kTitleArtistLineHeight);
  artist_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  artist_label_ = title_artist_row_->AddChildView(std::move(artist_label));

  // |button_row_| contains the buttons for controlling playback.
  auto button_row = std::make_unique<views::View>();
  auto* button_row_layout =
      button_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kMediaButtonRowSeparator));
  button_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  button_row->SetPreferredSize(kMediaNotificationButtonRowSize);
  button_row_ = main_row_->AddChildView(std::move(button_row));

  CreateMediaButton(
      MediaSessionAction::kPreviousTrack,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK));
  CreateMediaButton(
      MediaSessionAction::kSeekBackward,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SEEK_BACKWARD));

  // |play_pause_button_| toggles playback.
  auto play_pause_button = views::CreateVectorToggleImageButton(this);
  play_pause_button->set_tag(static_cast<int>(MediaSessionAction::kPlay));
  play_pause_button->SetPreferredSize(kMediaButtonSize);
  play_pause_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  play_pause_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY));
  play_pause_button->SetToggledTooltipText(l10n_util::GetStringUTF16(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE));
  play_pause_button_ = button_row_->AddChildView(std::move(play_pause_button));

  CreateMediaButton(
      MediaSessionAction::kSeekForward,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SEEK_FORWARD));
  CreateMediaButton(
      MediaSessionAction::kNextTrack,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK));

  SetBackground(std::make_unique<MediaNotificationBackground>(
      message_center::kNotificationCornerRadius,
      message_center::kNotificationCornerRadius, kMediaImageMaxWidthPct));

  UpdateForegroundColor();
  UpdateCornerRadius(message_center::kNotificationCornerRadius,
                     message_center::kNotificationCornerRadius);
  UpdateViewForExpandedState();

  if (item_)
    item_->SetView(this);
}

MediaNotificationView::~MediaNotificationView() {
  if (item_)
    item_->SetView(nullptr);
}

void MediaNotificationView::SetExpanded(bool expanded) {
  if (expanded_ == expanded)
    return;

  expanded_ = expanded;

  UpdateViewForExpandedState();

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateCornerRadius(int top_radius,
                                               int bottom_radius) {
  if (GetMediaNotificationBackground()->UpdateCornerRadius(top_radius,
                                                           bottom_radius)) {
    SchedulePaint();
  }
}

void MediaNotificationView::SetForcedExpandedState(
    bool* forced_expanded_state) {
  if (forced_expanded_state) {
    if (forced_expanded_state_ == *forced_expanded_state)
      return;
    forced_expanded_state_ = *forced_expanded_state;
  } else {
    if (!forced_expanded_state_.has_value())
      return;
    forced_expanded_state_ = base::nullopt;
  }

  header_row_->SetExpandButtonEnabled(IsExpandable());
  UpdateViewForExpandedState();
}

void MediaNotificationView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListItem;
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));

  if (!accessible_name_.empty())
    node_data->SetName(accessible_name_);
}

void MediaNotificationView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == header_row_) {
    SetExpanded(!expanded_);
    container_->OnHeaderClicked();
    return;
  }

  if (sender->parent() == button_row_) {
    if (item_) {
      item_->OnMediaSessionActionButtonPressed(GetActionFromButtonTag(*sender));
    }
    return;
  }

  NOTREACHED();
}

void MediaNotificationView::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  play_pause_button_->SetToggled(playing);

  MediaSessionAction action =
      playing ? MediaSessionAction::kPause : MediaSessionAction::kPlay;
  play_pause_button_->set_tag(static_cast<int>(action));

  UpdateActionButtonsVisibility();

  container_->OnMediaSessionInfoChanged(session_info);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  header_row_->SetAppName(metadata.source_title.empty()
                              ? default_app_name_
                              : metadata.source_title);
  title_label_->SetText(metadata.title);
  artist_label_->SetText(metadata.artist);
  header_row_->SetSummaryText(metadata.album);

  accessible_name_ = GetAccessibleNameFromMetadata(metadata);

  // The title label should only be a11y-focusable when there is text to be
  // read.
  if (metadata.title.empty()) {
    title_label_->SetFocusBehavior(FocusBehavior::NEVER);
  } else {
    title_label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    RecordMetadataHistogram(Metadata::kTitle);
  }

  // The artist label should only be a11y-focusable when there is text to be
  // read.
  if (metadata.artist.empty()) {
    artist_label_->SetFocusBehavior(FocusBehavior::NEVER);
  } else {
    artist_label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    RecordMetadataHistogram(Metadata::kArtist);
  }

  if (!metadata.album.empty())
    RecordMetadataHistogram(Metadata::kAlbum);

  RecordMetadataHistogram(Metadata::kCount);

  container_->OnMediaSessionMetadataChanged();

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaActions(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  enabled_actions_ = actions;

  header_row_->SetExpandButtonEnabled(IsExpandable());
  UpdateViewForExpandedState();

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  GetMediaNotificationBackground()->UpdateArtwork(image);

  has_artwork_ = !image.isNull();
  UpdateViewForExpandedState();

  UMA_HISTOGRAM_BOOLEAN(kArtworkHistogramName, has_artwork_);

  UpdateForegroundColor();

  container_->OnMediaArtworkChanged(image);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

views::Button* MediaNotificationView::GetHeaderRowForTesting() const {
  return header_row_;
}

void MediaNotificationView::UpdateActionButtonsVisibility() {
  base::flat_set<MediaSessionAction> ignored_actions = {
      GetPlayPauseIgnoredAction(GetActionFromButtonTag(*play_pause_button_))};

  base::flat_set<MediaSessionAction> visible_actions =
      GetTopVisibleActions(enabled_actions_, ignored_actions,
                           GetMaxNumActions(IsActuallyExpanded()));

  for (auto* view : button_row_->children()) {
    views::Button* action_button = views::Button::AsButton(view);
    bool should_show =
        base::Contains(visible_actions, GetActionFromButtonTag(*action_button));
    bool should_invalidate = should_show != action_button->GetVisible();

    action_button->SetVisible(should_show);

    if (should_invalidate)
      action_button->InvalidateLayout();
  }

  // We want to give the container a list of all possibly visible actions, and
  // not just currently visible actions so it can make a decision on whether or
  // not to force an expanded state.
  container_->OnVisibleActionsChanged(GetTopVisibleActions(
      enabled_actions_, ignored_actions, GetMaxNumActions(true)));
}

void MediaNotificationView::UpdateViewForExpandedState() {
  bool expanded = IsActuallyExpanded();

  // Adjust the layout of the |main_row_| based on the expanded state. If the
  // notification is expanded then the buttons should be below the title/artist
  // information. If it is collapsed then the buttons will be to the right.
  if (expanded) {
    static_cast<views::BoxLayout*>(button_row_->GetLayoutManager())
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);

    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical,
            gfx::Insets(
                kDefaultMarginSize, kDefaultMarginSize, kDefaultMarginSize,
                has_artwork_
                    ? (notification_width_ * kMediaImageMaxWidthExpandedPct)
                    : kDefaultMarginSize),
            kDefaultMarginSize))
        ->SetDefaultFlex(1);
  } else {
    static_cast<views::BoxLayout*>(button_row_->GetLayoutManager())
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(0, kDefaultMarginSize, 14,
                        has_artwork_
                            ? (notification_width_ * kMediaImageMaxWidthPct)
                            : kDefaultMarginSize),
            kDefaultMarginSize, true))
        ->SetFlexForView(title_artist_row_, 1);
  }

  main_row_->Layout();

  if (GetMediaNotificationBackground()->UpdateArtworkMaxWidthPct(
          expanded ? kMediaImageMaxWidthExpandedPct : kMediaImageMaxWidthPct)) {
    SchedulePaint();
  }

  header_row_->SetExpanded(expanded);
  container_->OnExpanded(expanded);

  UpdateActionButtonsVisibility();
}

void MediaNotificationView::CreateMediaButton(
    MediaSessionAction action,
    const base::string16& accessible_name) {
  auto button = views::CreateVectorImageButton(this);
  button->set_tag(static_cast<int>(action));
  button->SetPreferredSize(kMediaButtonSize);
  button->SetAccessibleName(accessible_name);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button_row_->AddChildView(std::move(button));
}

MediaNotificationBackground*
MediaNotificationView::GetMediaNotificationBackground() {
  return static_cast<MediaNotificationBackground*>(background());
}

bool MediaNotificationView::IsExpandable() const {
  if (forced_expanded_state_.has_value())
    return false;

  base::flat_set<MediaSessionAction> ignored_actions = {
      GetPlayPauseIgnoredAction(GetActionFromButtonTag(*play_pause_button_))};

  // If we can show more notifications if we were expanded then we should be
  // expandable.
  return GetTopVisibleActions(enabled_actions_, ignored_actions,
                              GetMaxNumActions(true))
             .size() > kMediaNotificationActionsCount;
}

bool MediaNotificationView::IsActuallyExpanded() const {
  if (forced_expanded_state_.has_value())
    return forced_expanded_state_.value();
  return expanded_ && IsExpandable();
}

void MediaNotificationView::UpdateForegroundColor() {
  const SkColor background =
      GetMediaNotificationBackground()->GetBackgroundColor(*this);
  const SkColor foreground =
      GetMediaNotificationBackground()->GetForegroundColor(*this);

  title_label_->SetEnabledColor(foreground);
  artist_label_->SetEnabledColor(foreground);
  header_row_->SetAccentColor(foreground);

  title_label_->SetBackgroundColor(background);
  artist_label_->SetBackgroundColor(background);
  header_row_->SetBackgroundColor(background);

  // Update play/pause button images.
  views::SetImageFromVectorIconWithColor(
      play_pause_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kPlay),
      kMediaButtonIconSize, foreground);
  views::SetToggledImageFromVectorIconWithColor(
      play_pause_button_,
      *GetVectorIconForMediaAction(MediaSessionAction::kPause),
      kMediaButtonIconSize, foreground);

  // Update action buttons.
  for (views::View* child : button_row_->children()) {
    // Skip the play pause button since it is a special case.
    if (child == play_pause_button_)
      continue;

    // Skip if the view is not an image button.
    if (child->GetClassName() != views::ImageButton::kViewClassName)
      continue;

    views::ImageButton* button = static_cast<views::ImageButton*>(child);

    views::SetImageFromVectorIconWithColor(
        button, *GetVectorIconForMediaAction(GetActionFromButtonTag(*button)),
        kMediaButtonIconSize, foreground);

    button->SchedulePaint();
  }

  container_->OnColorsChanged(foreground, background);
}

}  // namespace media_message_center
