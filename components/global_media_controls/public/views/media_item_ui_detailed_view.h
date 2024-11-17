// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DETAILED_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DETAILED_VIEW_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/notification_theme.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/global_media_controls/public/views/chapter_item_view.h"
#endif

namespace views {
class BoxLayoutView;
class Button;
class ImageView;
class Label;
}  // namespace views

namespace media_message_center {
class MediaNotificationContainer;
class MediaNotificationItem;
}  // namespace media_message_center
namespace global_media_controls {

class MediaActionButton;
class MediaProgressView;
enum class PlaybackStateChangeForDragging;

namespace {
class MediaLabelButton;
}  // namespace

// Indicates this media notification view will be displayed on which page. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused. Keep them in sync with
// tools/metrics/histograms/enums.xml.
enum class MediaDisplayPage {
  // Default value.
  kUnknown = 0,
  // Media will be displayed on the Quick Settings media view page.
  kQuickSettingsMediaView = 1,
  // Media will be displayed on the Quick Settings media detailed view page.
  kQuickSettingsMediaDetailedView = 2,
  // Media will be displayed on the system shelf media detailed view page.
  kSystemShelfMediaDetailedView = 3,
  // Media will be displayed on the lock screen media view page.
  kLockScreenMediaView = 4,
  // Media will be displayed on the Chrome browser media dialog view page.
  kMediaDialogView = 5,
  // Special enumerator that must share the highest enumerator value.
  kMaxValue = kMediaDialogView,
};

// CrOS implementation of media notification view.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIDetailedView
    : public media_message_center::MediaNotificationView {
  METADATA_HEADER(MediaItemUIDetailedView,
                  media_message_center::MediaNotificationView)

 public:
  MediaItemUIDetailedView(
      media_message_center::MediaNotificationContainer* container,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      std::unique_ptr<MediaItemUIFooter> footer_view,
      std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
      std::unique_ptr<views::View> dismiss_button,
      media_message_center::MediaColorTheme theme,
      MediaDisplayPage media_display_page);
  MediaItemUIDetailedView(const MediaItemUIDetailedView&) = delete;
  MediaItemUIDetailedView& operator=(const MediaItemUIDetailedView&) = delete;
  ~MediaItemUIDetailedView() override;

  // MediaNotificationView:
  void SetForcedExpandedState(bool* forced_expanded_state) override {}
  void SetExpanded(bool expanded) override {}
  void UpdateCornerRadius(int top_radius, int bottom_radius) override {}
  void UpdateWithMediaSessionInfo(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override;
  void UpdateWithMediaMetadata(
      const media_session::MediaMetadata& metadata) override;
  void UpdateWithMediaActions(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override;
  void UpdateWithMediaPosition(
      const media_session::MediaPosition& position) override;
  void UpdateWithMediaArtwork(const gfx::ImageSkia& image) override;
  void UpdateWithChapterArtwork(int index,
                                const gfx::ImageSkia& image) override;
  void UpdateWithFavicon(const gfx::ImageSkia& icon) override {}
  void UpdateWithVectorIcon(const gfx::VectorIcon* vector_icon) override {}
  void UpdateWithMuteStatus(bool mute) override {}
  void UpdateWithVolume(float volume) override {}
  void UpdateDeviceSelectorVisibility(bool visible) override {}
  void UpdateDeviceSelectorAvailability(bool has_devices) override;

  // views::View:
  void AddedToWidget() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Helper functions for testing:
  views::ImageView* GetArtworkViewForTesting();
  views::Label* GetSourceLabelForTesting();
  views::Label* GetTitleLabelForTesting();
  views::Label* GetArtistLabelForTesting();
  views::ImageView* GetChevronIconForTesting();
  views::Button* GetActionButtonForTesting(
      media_session::mojom::MediaSessionAction action);
  MediaProgressView* GetProgressViewForTesting();
  media_session::MediaPosition GetPositionForTesting();
  views::Button* GetStartCastingButtonForTesting();
  MediaItemUIFooter* GetFooterForTesting();
  MediaItemUIDeviceSelector* GetDeviceSelectorForTesting();
  views::View* GetDeviceSelectorSeparatorForTesting();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  views::Button* GetChapterListButtonForTesting();
  views::View* GetChapterListViewForTesting();
  views::Label* GetCurrentTimestampViewForTesting();
  views::Label* GetTotalDurationViewForTesting();
  base::flat_map<int, raw_ptr<ChapterItemView, CtnExperimental>>
  GetChaptersForTesting();
#endif

 private:
  friend class MediaItemUIDetailedViewTest;

  // Callback for a media label being pressed.
  void MediaLabelPressed(MediaLabelButton* button);

  MediaActionButton* CreateMediaActionButton(views::View* parent,
                                             int button_id,
                                             const gfx::VectorIcon& vector_icon,
                                             int tooltip_text_id);

  void UpdateActionButtonsVisibility();

  // Callback for a media action button being pressed.
  void MediaActionButtonPressed(views::Button* button);

  // Callback for when the user starts or ends dragging the progress view, and
  // the media is playing before dragging starts. The media should be
  // temporarily paused when the dragging starts, and resumed when the dragging
  // ends.
  void OnPlaybackStateChangeForProgressDrag(
      PlaybackStateChangeForDragging change);

  // Callback for when the media progress view wants to update the progress
  // position.
  void SeekTo(double seek_progress);

  // Callback for when the media progress view wants to update the progress
  // position to the given time.
  void SeekToTimestamp(const base::TimeDelta time) const;

  // Callback for when the start casting button is toggled by user.
  void StartCastingButtonPressed();

  // Update the display states of UI elements for casting devices.
  void UpdateCastingState();

  // Updates the chapter list view's chapter items with the new `metadata`.
  void UpdateChapterListViewWithMetadata(
      const media_session::MediaMetadata& metadata);

  // Creates a control row containing a timestamp view. Returns the container
  // for additional buttons that can be added later to the end of the same row.
  views::View* CreateControlsRow();

  // Callback for when the progress view updates the progress in UI given the
  // new media position.
  void OnProgressViewUpdateProgress(base::TimeDelta current_timestamp);

  // Raw pointer to the container holding this view. The |container_| should
  // never be nullptr.
  const raw_ptr<media_message_center::MediaNotificationContainer> container_;

  // Weak pointer to the media notification item associated with this view. The
  // |item_| should never be nullptr.
  base::WeakPtr<media_message_center::MediaNotificationItem> item_;

  // The color theme for all the colors in this view.
  media_message_center::MediaColorTheme theme_;

  // The display page source for this view.
  MediaDisplayPage media_display_page_;

  media_session::MediaPosition position_;

  // The list of action buttons in the view.
  std::vector<raw_ptr<views::Button, VectorExperimental>> action_buttons_;

  // Set of enabled actions.
  base::flat_set<media_session::mojom::MediaSessionAction> enabled_actions_;

  // Whether the media is currently in picture-in-picture.
  bool in_picture_in_picture_ = false;

  raw_ptr<views::ImageView> artwork_view_ = nullptr;
  raw_ptr<MediaLabelButton> source_label_ = nullptr;
  raw_ptr<MediaLabelButton> artist_label_ = nullptr;
  raw_ptr<MediaLabelButton> title_label_ = nullptr;
  raw_ptr<views::ImageView> chevron_icon_ = nullptr;

  raw_ptr<MediaProgressView> progress_view_ = nullptr;
  raw_ptr<MediaActionButton> play_pause_button_ = nullptr;
  raw_ptr<MediaActionButton> start_casting_button_ = nullptr;
  raw_ptr<MediaActionButton> picture_in_picture_button_ = nullptr;

  raw_ptr<MediaItemUIFooter> footer_view_ = nullptr;
  raw_ptr<MediaItemUIDeviceSelector> device_selector_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> device_selector_view_separator_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)

  // Callback for when the chapter list button is clicked by user.
  void ToggleChapterListView();

  // The chapter list button, which will be built only for chrome os ash.
  // Clicking on which will show the chapter list view.
  raw_ptr<MediaActionButton> chapter_list_button_ = nullptr;

  // The chapter list view, which will be built only for chrome os ash.
  raw_ptr<views::View> chapter_list_view_ = nullptr;

  // The current duration timestamp. It updates its text when
  // `OnProgressViewUpdateProgress` so the timestamp can be refreshed every
  // second.
  raw_ptr<views::Label> current_timestamp_view_ = nullptr;

  // The total duration timestamp. It updates its text when
  // `UpdateWithMediaPosition`.
  raw_ptr<views::Label> total_duration_view_ = nullptr;

  // The current `ChapterItemView` for the chapter at the index of the chapter
  // list.
  base::flat_map<int, raw_ptr<ChapterItemView, CtnExperimental>> chapters_;

  base::WeakPtrFactory<MediaItemUIDetailedView> weak_factory_{this};
#endif
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DETAILED_VIEW_H_
