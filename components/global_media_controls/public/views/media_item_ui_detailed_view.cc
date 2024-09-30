// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/global_media_controls/media_view_utils.h"
#include "components/global_media_controls/public/views/media_action_button.h"
#include "components/global_media_controls/public/views/media_progress_view.h"
#include "components/media_message_center/media_notification_container.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/global_media_controls/public/views/chapter_item_view.h"
#include "components/vector_icons/vector_icons.h"
#endif

namespace global_media_controls {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr gfx::Insets kBackgroundInsets = gfx::Insets::TLBR(16, 8, 8, 8);
constexpr gfx::Insets kMainRowInsets = gfx::Insets::TLBR(0, 8, 8, 8);
constexpr gfx::Insets kMediaInfoInsets = gfx::Insets::TLBR(0, 16, 0, 4);
constexpr gfx::Insets kSourceRowInsets = gfx::Insets::TLBR(0, 0, 6, 0);
constexpr gfx::Insets kControlsColumnInsets = gfx::Insets::TLBR(0, 8, 4, 2);
constexpr gfx::Insets kDeviceSelectorSeparatorInsets = gfx::Insets::VH(10, 12);
constexpr gfx::Insets kDeviceSelectorSeparatorLineInsets =
    gfx::Insets::VH(1, 1);

constexpr int kBackgroundCornerRadius = 16;
constexpr int kSourceTextLineHeight = 18;
constexpr int kTextLineHeight = 20;
constexpr int kFontSize = 12;
constexpr int kMediaInfoSeparator = 4;
constexpr int kControlsColumnSeparator = 10;
constexpr int kChevronIconSize = 15;
constexpr int kMediaActionButtonIconSize = 20;

constexpr float kFocusRingHaloInset = -3.0f;

constexpr gfx::Size kArtworkSize = gfx::Size(74, 74);
constexpr gfx::Size kPlayPauseButtonSize = gfx::Size(40, 40);
constexpr gfx::Size kControlsButtonSize = gfx::Size(32, 32);

constexpr char kMediaDisplayPageHistogram[] = "Media.Notification.DisplayPage";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr int kProgressRowHeight = 20;
constexpr gfx::Insets kButtonRowInsets = gfx::Insets::TLBR(0, 8, 0, 0);
constexpr char16_t kTimestampDelimiter[] = u" / ";
const gfx::FontList kTimestampFont({"Google Sans"},
                                   gfx::Font::NORMAL,
                                   13,
                                   gfx::Font::Weight::MEDIUM);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class MediaLabelButton : public views::Button {
  METADATA_HEADER(MediaLabelButton, views::Button)

 public:
  MediaLabelButton(const views::Label::CustomFont& font,
                   int text_line_height,
                   ui::ColorId text_color_id,
                   ui::ColorId focus_ring_color_id)
      : views::Button(PressedCallback()) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kLabelText);
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_LABEL));
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    views::FocusRing::Get(this)->SetColorId(focus_ring_color_id);

    // Hide the label button if the label text is empty.
    SetEnabled(false);

    label_ =
        AddChildView(std::make_unique<views::Label>(std::u16string(), font));
    label_->SetLineHeight(text_line_height);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_->SetEnabledColorId(text_color_id);
  }

  views::Label* label() { return label_; }
  void SetText(std::u16string text) {
    label_->SetText(text);
    SetEnabled(!text.empty());
  }

 private:
  raw_ptr<views::Label> label_;
};

BEGIN_METADATA(MediaLabelButton)
END_METADATA

}  // namespace

MediaItemUIDetailedView::MediaItemUIDetailedView(
    media_message_center::MediaNotificationContainer* container,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    std::unique_ptr<MediaItemUIFooter> footer_view,
    std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
    std::unique_ptr<views::View> dismiss_button,
    media_message_center::MediaColorTheme theme,
    MediaDisplayPage media_display_page)
    : container_(container),
      item_(std::move(item)),
      theme_(theme),
      media_display_page_(media_display_page) {
  CHECK(container_);

  // Media display page histogram for lock screen media view will be recorded in
  // LockScreenMediaView when the media view becomes visible to users.
  if (media_display_page_ == MediaDisplayPage::kLockScreenMediaView) {
    CHECK(dismiss_button);
  } else {
    CHECK(item_);
    base::UmaHistogramEnumeration(kMediaDisplayPageHistogram,
                                  media_display_page_);
  }

  SetBackground(views::CreateThemedRoundedRectBackground(
      theme_.background_color_id, kBackgroundCornerRadius));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBackgroundInsets));

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kBackgroundCornerRadius);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kFocusRingHaloInset);
  focus_ring->SetColorId(theme_.focus_ring_color_id);

  // |main_row| holds the media artwork, media information column and the
  // play/pause button.
  auto* main_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  main_row->SetInsideBorderInsets(kMainRowInsets);

  artwork_view_ = main_row->AddChildView(std::make_unique<views::ImageView>());
  artwork_view_->SetPreferredSize(kArtworkSize);

  // |media_info_column| inside |main_row| holds the media source, title, and
  // artist.
  auto* media_info_column =
      main_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  media_info_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  media_info_column->SetInsideBorderInsets(kMediaInfoInsets);
  media_info_column->SetBetweenChildSpacing(kMediaInfoSeparator);
  media_info_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  main_row->SetFlexForView(media_info_column, 1);

  const views::Label::CustomFont text_fonts = {
      gfx::FontList({"Google Sans", "Roboto"}, gfx::Font::NORMAL, kFontSize,
                    gfx::Font::Weight::NORMAL)};

  // Create the media source label.
  auto* source_row =
      media_info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  source_row->SetInsideBorderInsets(kSourceRowInsets);

  source_label_ = source_row->AddChildView(std::make_unique<MediaLabelButton>(
      text_fonts, kSourceTextLineHeight, theme_.secondary_foreground_color_id,
      theme_.focus_ring_color_id));
  source_label_->SetCallback(
      base::BindRepeating(&MediaItemUIDetailedView::MediaLabelPressed,
                          base::Unretained(this), source_label_));
  source_row->SetFlexForView(source_label_, 1);

  // Create the media title label.
  auto* title_row =
      media_info_column->AddChildView(std::make_unique<views::BoxLayoutView>());
  title_row->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  title_label_ = title_row->AddChildView(std::make_unique<MediaLabelButton>(
      text_fonts, kTextLineHeight, theme_.primary_foreground_color_id,
      theme_.focus_ring_color_id));
  title_label_->SetCallback(
      base::BindRepeating(&MediaItemUIDetailedView::MediaLabelPressed,
                          base::Unretained(this), title_label_));
  title_row->SetFlexForView(title_label_, 1);

  // Add a chevron right icon to the title if the media is displaying on the
  // quick settings media view to indicate user can click on the view to go to
  // the detailed view page.
  if (media_display_page_ == MediaDisplayPage::kQuickSettingsMediaView) {
    chevron_icon_ = title_row->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            media_message_center::kChevronRightIcon,
            theme_.secondary_foreground_color_id, kChevronIconSize)));
    chevron_icon_->SetFlipCanvasOnPaintForRTLUI(true);
  }

  // Create the media artist label.
  artist_label_ =
      media_info_column->AddChildView(std::make_unique<MediaLabelButton>(
          text_fonts, kTextLineHeight, theme_.secondary_foreground_color_id,
          theme_.focus_ring_color_id));
  artist_label_->SetCallback(
      base::BindRepeating(&MediaItemUIDetailedView::MediaLabelPressed,
                          base::Unretained(this), artist_label_));

  // |controls_column| inside |main_row| holds the play/pause button and the
  // dismiss button on the top right corner if it exists.
  auto* controls_column =
      main_row->AddChildView(std::make_unique<views::BoxLayoutView>());
  controls_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  controls_column->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  controls_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);
  controls_column->SetInsideBorderInsets(kControlsColumnInsets);

  if (dismiss_button) {
    controls_column->SetBetweenChildSpacing(kControlsColumnSeparator);
    controls_column->AddChildView(std::move(dismiss_button));
  }

  // Create the play/pause button.
  play_pause_button_ = CreateMediaActionButton(
      controls_column, static_cast<int>(MediaSessionAction::kPlay),
      media_message_center::kPlayArrowIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY);
  play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      theme_.play_button_container_color_id,
      kPlayPauseButtonSize.height() / 2));

  // `controls_row` holds all the available media action buttons and the
  // progress view.
  auto* controls_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  // TODO(b/328317702): The fllowing lines are removed as a temp fix of the
  // tobo bug.
  // controls_row->SetCrossAxisAlignment(
  //     views::BoxLayout::CrossAxisAlignment::kCenter);

  views::View* button_container = CreateControlsRow();
  if (!button_container) {
    button_container = controls_row;
  }

  // Create the previous track button.
  CreateMediaActionButton(
      button_container, static_cast<int>(MediaSessionAction::kPreviousTrack),
      media_message_center::kMediaPreviousTrackIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK);

  // Create the progress view.
  progress_view_ =
      controls_row->AddChildView(std::make_unique<MediaProgressView>(
          (media_display_page_ != MediaDisplayPage::kMediaDialogView),
          theme_.playing_progress_foreground_color_id,
          theme_.playing_progress_background_color_id,
          theme_.paused_progress_foreground_color_id,
          theme_.paused_progress_background_color_id,
          theme_.focus_ring_color_id,
          /*drag_state_change_callback=*/base::DoNothing(),
          base::BindRepeating(
              &MediaItemUIDetailedView::OnPlaybackStateChangeForProgressDrag,
              base::Unretained(this)),
          base::BindRepeating(&MediaItemUIDetailedView::SeekTo,
                              base::Unretained(this)),
          base::BindRepeating(
              &MediaItemUIDetailedView::OnProgressViewUpdateProgress,
              base::Unretained(this))));
  controls_row->SetFlexForView(progress_view_, 1);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    controls_row->SetMinimumCrossAxisSize(kProgressRowHeight);

    // Create the replay 10 button.
    CreateMediaActionButton(
        button_container, static_cast<int>(MediaSessionAction::kSeekBackward),
        vector_icons::kReplay10Icon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_REPLAY_10);

    // Create the forward 10 button.
    CreateMediaActionButton(
        button_container, static_cast<int>(MediaSessionAction::kSeekForward),
        vector_icons::kForward10Icon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_FORWARD_10);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Create the next track button.
  CreateMediaActionButton(
      button_container, static_cast<int>(MediaSessionAction::kNextTrack),
      media_message_center::kMediaNextTrackIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK);

  const gfx::VectorIcon* devices_icon =
      &media_message_center::kMediaCastStartIcon;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    // Create the chapter list button.
    // TODO(b/327505486): The string id is a place holder for now, the real
    // label is TBD.
    chapter_list_button_ = CreateMediaActionButton(
        button_container, kEmptyMediaActionButtonId,
        vector_icons::kVideoLibraryIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SHOW_DEVICE_LIST);
    chapter_list_button_->SetCallback(
        base::BindRepeating(&MediaItemUIDetailedView::ToggleChapterListView,
                            base::Unretained(this)));
    chapter_list_button_->SetVisible(false);

    // Show the `kDevicesIcon` as the device selector button's icon.
    devices_icon = &vector_icons::kDevicesIcon;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Create the start casting button.
  if (device_selector_view) {
    start_casting_button_ = CreateMediaActionButton(
        button_container, kEmptyMediaActionButtonId, *devices_icon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SHOW_DEVICE_LIST);
    start_casting_button_->SetCallback(
        base::BindRepeating(&MediaItemUIDetailedView::StartCastingButtonPressed,
                            base::Unretained(this)));
    start_casting_button_->SetVisible(false);
  }

  // Create the picture-in-picture button.
  picture_in_picture_button_ = CreateMediaActionButton(
      button_container,
      static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
      media_message_center::kMediaEnterPipIcon,
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP);

  // Create the stop casting button. It will only show up when this media item
  // is being casted to another device.
  if (footer_view) {
    footer_view_ = button_container->AddChildView(std::move(footer_view));
    picture_in_picture_button_->SetVisible(false);
  }

  if (device_selector_view) {
    // Create a separator line between the media view and device selector view.
    device_selector_view_separator_ =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    device_selector_view_separator_->SetInsideBorderInsets(
        kDeviceSelectorSeparatorInsets);
    auto* separator = device_selector_view_separator_->AddChildView(
        std::make_unique<views::BoxLayoutView>());
    separator->SetInsideBorderInsets(kDeviceSelectorSeparatorLineInsets);
    separator->SetBackground(
        views::CreateThemedSolidBackground(theme_.separator_color_id));
    device_selector_view_separator_->SetFlexForView(separator, 1);

    // Create the device selector view.
    device_selector_view_ = AddChildView(std::move(device_selector_view));
  }

  if (item_) {
    item_->SetView(this);
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF8(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACCESSIBLE_NAME));
}

MediaItemUIDetailedView::~MediaItemUIDetailedView() {
  if (item_) {
    item_->SetView(nullptr);
  }
}

///////////////////////////////////////////////////////////////////////////////
// MediaNotificationView implementations:

void MediaItemUIDetailedView::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  if (playing) {
    play_pause_button_->Update(
        static_cast<int>(MediaSessionAction::kPause),
        media_message_center::kPauseIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE,
        theme_.pause_button_foreground_color_id);
    play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        theme_.pause_button_container_color_id,
        kPlayPauseButtonSize.height() / 2));
  } else {
    play_pause_button_->Update(
        static_cast<int>(MediaSessionAction::kPlay),
        media_message_center::kPlayArrowIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY,
        theme_.play_button_foreground_color_id);
    play_pause_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        theme_.play_button_container_color_id,
        kPlayPauseButtonSize.height() / 2));
  }

  in_picture_in_picture_ =
      session_info &&
      session_info->picture_in_picture_state ==
          media_session::mojom::MediaPictureInPictureState::kInPictureInPicture;
  if (in_picture_in_picture_) {
    picture_in_picture_button_->Update(
        static_cast<int>(MediaSessionAction::kExitPictureInPicture),
        media_message_center::kMediaExitPipIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_EXIT_PIP,
        theme_.primary_foreground_color_id);
  } else {
    picture_in_picture_button_->Update(
        static_cast<int>(MediaSessionAction::kEnterPictureInPicture),
        media_message_center::kMediaEnterPipIcon,
        IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP,
        theme_.primary_foreground_color_id);
  }

  UpdateActionButtonsVisibility();
  container_->OnMediaSessionInfoChanged(session_info);
}

void MediaItemUIDetailedView::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  source_label_->label()->SetElideBehavior(gfx::ELIDE_HEAD);
  source_label_->SetText(metadata.source_title);
  title_label_->SetText(metadata.title);
  artist_label_->SetText(metadata.artist);

  UpdateChapterListViewWithMetadata(metadata);
  container_->OnMediaSessionMetadataChanged(metadata);
}

void MediaItemUIDetailedView::UpdateWithMediaActions(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  enabled_actions_ = actions;
  UpdateActionButtonsVisibility();

  container_->OnVisibleActionsChanged(enabled_actions_);
}

void MediaItemUIDetailedView::UpdateWithMediaPosition(
    const media_session::MediaPosition& position) {
  position_ = position;
  progress_view_->UpdateProgress(position);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    total_duration_view_->SetText(kTimestampDelimiter +
                                  GetFormattedDuration(position.duration()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void MediaItemUIDetailedView::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Hide the image so the other contents will adjust to fill the container.
    artwork_view_->SetVisible(false);
  } else {
    artwork_view_->SetVisible(true);
    artwork_view_->SetImageSize(
        ScaleImageSizeToFitView(image.size(), kArtworkSize));
    artwork_view_->SetImage(ui::ImageModel::FromImageSkia(image));

    // Draw the image with rounded corners.
    auto path = SkPath().addRoundRect(
        RectToSkRect(gfx::Rect(kArtworkSize.width(), kArtworkSize.height())),
        kDefaultArtworkCornerRadius, kDefaultArtworkCornerRadius);
    artwork_view_->SetClipPath(path);
  }
  SchedulePaint();
}

void MediaItemUIDetailedView::UpdateWithChapterArtwork(
    int index,
    const gfx::ImageSkia& image) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    return;
  }

  if (auto it = chapters_.find(index); it != chapters_.end()) {
    it->second->UpdateArtwork(image);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void MediaItemUIDetailedView::UpdateDeviceSelectorAvailability(
    bool has_devices) {
  CHECK(start_casting_button_);
  // Do not show the start casting button if this media item is being casted to
  // another device and has a footer view of stop casting button.
  bool visible = has_devices && !footer_view_;
  if (visible != start_casting_button_->GetVisible()) {
    start_casting_button_->SetVisible(visible);
    UpdateCastingState();
  }
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

void MediaItemUIDetailedView::AddedToWidget() {
  // Ink drop on the start casting button requires color provider to be ready,
  // so we need to update the state after the widget is ready.
  if (device_selector_view_) {
    UpdateCastingState();
  }
}

bool MediaItemUIDetailedView::OnKeyPressed(const ui::KeyEvent& event) {
  // As soon as the media view gets the focus, it should be able to handle key
  // events that can change the progress.
  return progress_view_->OnKeyPressed(event);
}

///////////////////////////////////////////////////////////////////////////////
// MediaItemUIDetailedView implementations:

void MediaItemUIDetailedView::MediaLabelPressed(MediaLabelButton* button) {
  // Pressing any media info label on the quick settings media view will try to
  // activate the original web contents if it is hidden, or go to detailed view
  // if it is not. Pressing other places on the quick settings media view will
  // only go to detailed view. The code for going to detailed view is in
  // QuickSettingsMediaViewController. Meanwhile, pressing any media info label
  // on other media views has the same effect as pressing other places.
  container_->OnHeaderClicked(/*activate_original_media=*/true);
}

MediaActionButton* MediaItemUIDetailedView::CreateMediaActionButton(
    views::View* parent,
    int button_id,
    const gfx::VectorIcon& vector_icon,
    int tooltip_text_id) {
  auto button = std::make_unique<MediaActionButton>(
      views::Button::PressedCallback(), button_id, tooltip_text_id,
      kMediaActionButtonIconSize, vector_icon,
      (button_id == static_cast<int>(MediaSessionAction::kPlay)
           ? kPlayPauseButtonSize
           : kControlsButtonSize),
      theme_.primary_foreground_color_id, theme_.secondary_foreground_color_id,
      theme_.focus_ring_color_id);
  auto* button_ptr = parent->AddChildView(std::move(button));

  if (button_id != kEmptyMediaActionButtonId) {
    button_ptr->SetCallback(
        base::BindRepeating(&MediaItemUIDetailedView::MediaActionButtonPressed,
                            base::Unretained(this), button_ptr));
    action_buttons_.push_back(button_ptr);
  }
  return button_ptr;
}

void MediaItemUIDetailedView::UpdateActionButtonsVisibility() {
  bool should_invalidate_layout = false;

  for (views::Button* button : action_buttons_) {
    bool should_show = base::Contains(
        enabled_actions_, static_cast<MediaSessionAction>(button->GetID()));

    if (button == picture_in_picture_button_) {
      // Force the picture-in-picture button to be visible if the media is
      // currently in the picture-in-picture state, since the media actions
      // may not contain pip actions for a short period of time for unknown
      // reason, which can cause the picture-in-picture button to lose focus,
      // but we want the button to keep the focus so that the user is able to
      // undo the pip action immediately if needed.
      if (in_picture_in_picture_) {
        should_show = true;
      }

      // The picture-in-picture button remains invisible if there is a footer
      // view regardless of media actions.
      if (footer_view_) {
        should_show = false;
      }
    }

    if (should_show != button->GetVisible()) {
      button->SetVisible(should_show);
      should_invalidate_layout = true;
    }
  }

  if (should_invalidate_layout) {
    InvalidateLayout();
  }
}

void MediaItemUIDetailedView::MediaActionButtonPressed(views::Button* button) {
  const auto action = static_cast<MediaSessionAction>(button->GetID());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (action == MediaSessionAction::kSeekBackward) {
    const auto backward_duration =
        std::max(base::Seconds(0), position_.GetPosition() - kSeekTime);
    if (item_) {
      item_->SeekTo(backward_duration);
    } else {
      container_->SeekTo(backward_duration);
    }
    return;
  }
  if (action == MediaSessionAction::kSeekForward) {
    const auto forward_duration =
        std::min(position_.GetPosition() + kSeekTime, position_.duration());
    if (item_) {
      item_->SeekTo(forward_duration);
    } else {
      container_->SeekTo(forward_duration);
    }
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (item_) {
    item_->OnMediaSessionActionButtonPressed(action);
  } else {
    // LockScreenMediaView does not have MediaNotificationItem and will handle
    // the action itself.
    container_->OnMediaSessionActionButtonPressed(action);
  }
}

void MediaItemUIDetailedView::OnPlaybackStateChangeForProgressDrag(
    PlaybackStateChangeForDragging change) {
  const auto action =
      (change == PlaybackStateChangeForDragging::kPauseForDraggingStarted
           ? MediaSessionAction::kPause
           : MediaSessionAction::kPlay);
  if (item_) {
    item_->OnMediaSessionActionButtonPressed(action);
  } else {
    // LockScreenMediaView does not have MediaNotificationItem and will handle
    // the action itself.
    container_->OnMediaSessionActionButtonPressed(action);
  }
}

void MediaItemUIDetailedView::SeekTo(double seek_progress) {
  const auto time = seek_progress * position_.duration();
  SeekToTimestamp(time);
}

void MediaItemUIDetailedView::SeekToTimestamp(
    const base::TimeDelta time) const {
  if (item_) {
    item_->SeekTo(time);
  } else {
    // LockScreenMediaView does not have MediaNotificationItem and will handle
    // the seek event itself.
    container_->SeekTo(time);
  }
}

void MediaItemUIDetailedView::StartCastingButtonPressed() {
  CHECK(device_selector_view_);

  switch (media_display_page_) {
    case MediaDisplayPage::kQuickSettingsMediaView: {
      // Clicking the button on the quick settings media view should redirect
      // the user to the quick settings media detailed view and open the device
      // selector view there instead.
      container_->OnShowCastingDevicesRequested();
      break;
    }
    case MediaDisplayPage::kQuickSettingsMediaDetailedView:
    case MediaDisplayPage::kSystemShelfMediaDetailedView:
    case MediaDisplayPage::kMediaDialogView: {
      // Clicking the button on the media detailed view will toggle the device
      // list in the device selector view.
      if (device_selector_view_->IsDeviceSelectorExpanded()) {
        device_selector_view_->HideDevices();
      } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
        // Hide the chapter list view if it's shown.
        if (chapter_list_view_ && chapter_list_view_->GetVisible()) {
          ToggleChapterListView();
        }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        device_selector_view_->ShowDevices();
      }
      UpdateCastingState();
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void MediaItemUIDetailedView::UpdateCastingState() {
  CHECK(start_casting_button_);
  CHECK(device_selector_view_);
  CHECK(device_selector_view_separator_);

  if (start_casting_button_->GetVisible()) {
    device_selector_view_->SetVisible(true);
    bool is_expanded = device_selector_view_->IsDeviceSelectorExpanded();
    if (is_expanded) {
      // Use the ink drop color as the button background if user clicks the
      // button to show devices.
      views::InkDrop::Get(start_casting_button_)
          ->GetInkDrop()
          ->SnapToActivated();

      // Indicate the user can hide the device list.
      start_casting_button_->UpdateText(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_HIDE_DEVICE_LIST);
    } else {
      // Hide the ink drop color if user clicks the button to hide devices.
      views::InkDrop::Get(start_casting_button_)->GetInkDrop()->SnapToHidden();

      // Indicate the user can show the device list.
      start_casting_button_->UpdateText(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SHOW_DEVICE_LIST);
    }

    bool is_ash_background_listening_enabled = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    is_ash_background_listening_enabled =
        base::FeatureList::IsEnabled(media::kBackgroundListening);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    device_selector_view_separator_->SetVisible(
        is_expanded && !is_ash_background_listening_enabled);
  } else {
    device_selector_view_->SetVisible(false);
    device_selector_view_separator_->SetVisible(false);
  }

  container_->OnListViewSizeChanged();
}

void MediaItemUIDetailedView::UpdateChapterListViewWithMetadata(
    const media_session::MediaMetadata& metadata) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    return;
  }

  const bool is_chapter_button_visible = chapter_list_button_->GetVisible();
  if (!chapter_list_view_ && metadata.chapters.empty()) {
    if (is_chapter_button_visible) {
      chapter_list_button_->SetVisible(false);
    }
    return;
  }

  if (metadata.chapters.empty()) {
    chapter_list_view_->RemoveAllChildViews();
    chapters_.clear();

    if (is_chapter_button_visible) {
      chapter_list_button_->SetVisible(false);
    }
    if (chapter_list_view_->GetVisible()) {
      chapter_list_view_->SetVisible(false);
      container_->OnListViewSizeChanged();
    }
    return;
  }

  if (!is_chapter_button_visible) {
    chapter_list_button_->SetVisible(true);
  }

  // Ensures the chapter list view exists and is up-to-date:
  //  1) Creates the chapter list view if it doesn't exist.
  //  2) Compares existing chapter item views to new metadata:
  //     a) If chapters match, no updates are needed.
  //     b) If chapters differ, the list is cleared and updated with the new
  // chapters.
  if (!chapter_list_view_) {
    chapter_list_view_ = AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetInsideBorderInsets(gfx::Insets::TLBR(16, 8, 8, 8))
            .Build());
    chapter_list_view_->SetVisible(false);
  } else {
    bool chapters_not_changed = metadata.chapters.size() == chapters_.size();
    // Further checks if every chapter is the same.
    if (chapters_not_changed) {
      for (int index = 0; index < static_cast<int>(metadata.chapters.size());
           index++) {
        if (!chapters_[index] ||
            chapters_[index]->chapter() != metadata.chapters[index]) {
          chapters_not_changed = false;
          break;
        }
      }
    }
    if (chapters_not_changed) {
      return;
    }
    chapter_list_view_->RemoveAllChildViews();
  }

  chapters_.clear();
  for (int index = 0; index < static_cast<int>(metadata.chapters.size());
       index++) {
    chapters_[index] =
        chapter_list_view_->AddChildView(std::make_unique<ChapterItemView>(
            metadata.chapters[index], theme_,
            /*on_chapter_pressed=*/
            base::BindRepeating(&MediaItemUIDetailedView::SeekToTimestamp,
                                weak_factory_.GetWeakPtr())));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

views::View* MediaItemUIDetailedView::CreateControlsRow() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    return nullptr;
  }

  views::View* button_container;
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetInsideBorderInsets(kButtonRowInsets)
          .AddChildren(
              views::Builder<views::BoxLayoutView>()
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                  .SetProperty(views::kBoxLayoutFlexKey,
                               views::BoxLayoutFlexSpecification())
                  .AddChildren(
                      views::Builder<views::Label>()
                          .CopyAddressTo(&current_timestamp_view_)
                          .SetText(
                              GetFormattedDuration(position_.GetPosition()))
                          .SetFontList(kTimestampFont)
                          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                          .SetEnabledColorId(
                              theme_.primary_foreground_color_id),
                      views::Builder<views::Label>()
                          .CopyAddressTo(&total_duration_view_)
                          .SetText(kTimestampDelimiter +
                                   GetFormattedDuration(position_.duration()))
                          .SetFontList(kTimestampFont)
                          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                          .SetEnabledColorId(
                              theme_.primary_foreground_color_id)))
          .AddChildren(
              views::Builder<views::BoxLayoutView>()
                  .CopyAddressTo(&button_container)
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal))
          .Build());

  return button_container;
#else
  return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void MediaItemUIDetailedView::OnProgressViewUpdateProgress(
    base::TimeDelta current_timestamp) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    current_timestamp_view_->SetText(GetFormattedDuration(current_timestamp));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Helper functions for testing:
views::ImageView* MediaItemUIDetailedView::GetArtworkViewForTesting() {
  return artwork_view_;
}

views::Label* MediaItemUIDetailedView::GetSourceLabelForTesting() {
  return source_label_->label();
}

views::Label* MediaItemUIDetailedView::GetTitleLabelForTesting() {
  return title_label_->label();
}

views::Label* MediaItemUIDetailedView::GetArtistLabelForTesting() {
  return artist_label_->label();
}

views::ImageView* MediaItemUIDetailedView::GetChevronIconForTesting() {
  return chevron_icon_;
}

views::Button* MediaItemUIDetailedView::GetActionButtonForTesting(
    media_session::mojom::MediaSessionAction action) {
  const auto i = base::ranges::find(action_buttons_, static_cast<int>(action),
                                    &views::View::GetID);
  return (i == action_buttons_.end()) ? nullptr : *i;
}

MediaProgressView* MediaItemUIDetailedView::GetProgressViewForTesting() {
  return progress_view_;
}

media_session::MediaPosition MediaItemUIDetailedView::GetPositionForTesting() {
  return position_;
}

views::Button* MediaItemUIDetailedView::GetStartCastingButtonForTesting() {
  return start_casting_button_;
}

MediaItemUIFooter* MediaItemUIDetailedView::GetFooterForTesting() {
  return footer_view_;
}

MediaItemUIDeviceSelector*
MediaItemUIDetailedView::GetDeviceSelectorForTesting() {
  return device_selector_view_;
}

views::View* MediaItemUIDetailedView::GetDeviceSelectorSeparatorForTesting() {
  return device_selector_view_separator_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
views::Button* MediaItemUIDetailedView::GetChapterListButtonForTesting() {
  return chapter_list_button_;
}

views::View* MediaItemUIDetailedView::GetChapterListViewForTesting() {
  return chapter_list_view_;
}

views::Label* MediaItemUIDetailedView::GetCurrentTimestampViewForTesting() {
  return current_timestamp_view_;
}

views::Label* MediaItemUIDetailedView::GetTotalDurationViewForTesting() {
  return total_duration_view_;
}

base::flat_map<int, raw_ptr<ChapterItemView, CtnExperimental>>
MediaItemUIDetailedView::GetChaptersForTesting() {
  return chapters_;
}

void MediaItemUIDetailedView::ToggleChapterListView() {
  if (!base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    return;
  }

  if (!chapter_list_view_) {
    return;
  }

  const bool is_showing_chapters = chapter_list_view_->GetVisible();

  if (chapters_.empty()) {
    if (chapter_list_button_->GetVisible()) {
      chapter_list_button_->SetVisible(false);
    }

    if (is_showing_chapters) {
      chapter_list_view_->SetVisible(false);
    }
    return;
  }

  // TODO(b/327505486): Update tooltips.
  if (is_showing_chapters) {
    // Hide the ink drop color if user clicks the button to hide devices.
    views::InkDrop::Get(chapter_list_button_)->GetInkDrop()->SnapToHidden();
  } else {
    // Use the ink drop color as the button background if user clicks the
    // button to show devices.
    views::InkDrop::Get(chapter_list_button_)->GetInkDrop()->SnapToActivated();

    // Hide the `device_selector_view_` if it's shown.
    if (device_selector_view_ &&
        device_selector_view_->IsDeviceSelectorExpanded()) {
      device_selector_view_->HideDevices();
      UpdateCastingState();
    }
  }

  chapter_list_view_->SetVisible(!is_showing_chapters);
  container_->OnListViewSizeChanged();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

BEGIN_METADATA(MediaItemUIDetailedView)
END_METADATA

}  // namespace global_media_controls
