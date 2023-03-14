// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_item_ui_view.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_view_ash_impl.h"
#include "components/media_message_center/media_notification_view_modern_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace global_media_controls {

namespace {

constexpr int kWidth = 400;
constexpr int kModernUIWidth = 350;
constexpr gfx::Size kNormalSize = gfx::Size(kWidth, 100);
constexpr gfx::Size kExpandedSize = gfx::Size(kWidth, 150);
constexpr gfx::Size kModernUISize = gfx::Size(kModernUIWidth, 168);
constexpr gfx::Size kDismissButtonSize = gfx::Size(30, 30);
constexpr int kDismissButtonIconSize = 20;
constexpr int kDismissButtonBackgroundRadius = 15;
constexpr gfx::Size kCrOSDismissButtonSize = gfx::Size(20, 20);
constexpr int kCrOSDismissButtonIconSize = 12;
constexpr gfx::Size kModernDismissButtonSize = gfx::Size(14, 14);
constexpr int kModernDismissButtonIconSize = 10;
constexpr gfx::Insets kSwipeableContainerInsets =
    gfx::Insets::TLBR(0, 16, 8, 16);

// The minimum number of enabled and visible user actions such that we should
// force the MediaNotificationView to be expanded.
constexpr int kMinVisibleActionsForExpanding = 4;

}  // anonymous namespace

class MediaItemUIView::DismissButton : public views::ImageButton {
 public:
  METADATA_HEADER(DismissButton);

  explicit DismissButton(PressedCallback callback)
      : views::ImageButton(std::move(callback)) {
    views::ConfigureVectorImageButton(this);
    views::InstallFixedSizeCircleHighlightPathGenerator(
        this, kDismissButtonBackgroundRadius);
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  }
  DismissButton(const DismissButton&) = delete;
  DismissButton& operator=(const DismissButton&) = delete;
  ~DismissButton() override = default;
};

BEGIN_METADATA(MediaItemUIView, DismissButton, views::ImageButton)
END_METADATA

MediaItemUIView::MediaItemUIView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    std::unique_ptr<MediaItemUIFooter> footer_view,
    std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view,
    absl::optional<media_message_center::NotificationTheme> theme)
    : views::Button(base::BindRepeating(&MediaItemUIView::ContainerClicked,
                                        base::Unretained(this))),
      id_(id),
      is_cros_(theme.has_value()) {
  DCHECK(item);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetPreferredSize(kNormalSize);
  SetNotifyEnterExitOnChild(true);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_BACK_TO_TAB));

#if BUILDFLAG(IS_CHROMEOS)
  bool use_cros_updated_ui =
      base::FeatureList::IsEnabled(media::kGlobalMediaControlsCrOSUpdatedUI);
#else
  bool use_cros_updated_ui = false;
#endif

  auto swipeable_container = std::make_unique<views::View>();
  swipeable_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  swipeable_container->SetPaintToLayer();
  swipeable_container->layer()->SetFillsBoundsOpaquely(false);
  if (use_cros_updated_ui) {
    swipeable_container->SetBorder(
        views::CreateEmptyBorder(kSwipeableContainerInsets));
  }
  swipeable_container_ = AddChildView(std::move(swipeable_container));

  gfx::Size dismiss_button_size =
      is_cros_ ? kCrOSDismissButtonSize : kDismissButtonSize;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI))
    dismiss_button_size = kModernDismissButtonSize;

  auto dismiss_button_placeholder = std::make_unique<views::View>();
  dismiss_button_placeholder->SetPreferredSize(dismiss_button_size);
  dismiss_button_placeholder->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  dismiss_button_placeholder_ = dismiss_button_placeholder.get();

  auto dismiss_button_container = std::make_unique<views::View>();
  dismiss_button_container->SetPreferredSize(dismiss_button_size);
  dismiss_button_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  dismiss_button_container->SetVisible(false);
  dismiss_button_container_ = dismiss_button_placeholder_->AddChildView(
      std::move(dismiss_button_container));

  auto dismiss_button = std::make_unique<DismissButton>(base::BindRepeating(
      &MediaItemUIView::DismissNotification, base::Unretained(this)));
  dismiss_button->SetPreferredSize(dismiss_button_size);
  dismiss_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_GLOBAL_MEDIA_CONTROLS_DISMISS_ICON_TOOLTIP_TEXT));
  dismiss_button_ =
      dismiss_button_container_->AddChildView(std::move(dismiss_button));
  UpdateDismissButtonIcon();

  std::unique_ptr<media_message_center::MediaNotificationView> view;
  if (use_cros_updated_ui) {
    view = std::make_unique<media_message_center::MediaNotificationViewAshImpl>(
        this, std::move(item), std::move(dismiss_button_placeholder), theme);
  } else if (base::FeatureList::IsEnabled(
                 media::kGlobalMediaControlsModernUI)) {
    footer_view_ = footer_view_.get();
    view =
        std::make_unique<media_message_center::MediaNotificationViewModernImpl>(
            this, std::move(item), std::move(dismiss_button_placeholder),
            std::move(footer_view), kModernUIWidth, theme);
    SetPreferredSize(kModernUISize);
  } else {
    view = std::make_unique<media_message_center::MediaNotificationViewImpl>(
        this, std::move(item), std::move(dismiss_button_placeholder),
        std::u16string(), kWidth, /*should_show_icon=*/false, theme);

    UpdateFooterView(std::move(footer_view));
    SetPreferredSize(kNormalSize);
  }
  view_ = swipeable_container_->AddChildView(std::move(view));

  UpdateDeviceSelector(std::move(device_selector_view));

  ForceExpandedState();

  slide_out_controller_ =
      std::make_unique<views::SlideOutController>(this, this);
}

MediaItemUIView::~MediaItemUIView() {
  for (auto& observer : observers_)
    observer.OnMediaItemUIDestroyed(id_);
}

void MediaItemUIView::AddedToWidget() {
  if (GetFocusManager())
    GetFocusManager()->AddFocusChangeListener(this);
}

void MediaItemUIView::RemovedFromWidget() {
  if (GetFocusManager())
    GetFocusManager()->RemoveFocusChangeListener(this);
}

void MediaItemUIView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateDismissButtonVisibility();
}

void MediaItemUIView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateDismissButtonVisibility();
}

void MediaItemUIView::OnGestureEvent(ui::GestureEvent* event) {
  if (scroll_view_ && event->IsScrollGestureEvent())
    scroll_view_->OnGestureEvent(event);
}

void MediaItemUIView::OnDidChangeFocus(views::View* focused_before,
                                       views::View* focused_now) {
  UpdateDismissButtonVisibility();
}

void MediaItemUIView::OnExpanded(bool expanded) {
  is_expanded_ = expanded;
  OnSizeChanged();
}

void MediaItemUIView::OnMediaSessionInfoChanged(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  is_playing_ =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  if (session_info) {
    auto audio_sink_id = session_info->audio_sink_id.value_or(
        media::AudioDeviceDescription::kDefaultDeviceId);
    if (device_selector_view_ &&
        base::FeatureList::IsEnabled(
            media::kGlobalMediaControlsSeamlessTransfer)) {
      device_selector_view_->UpdateCurrentAudioDevice(audio_sink_id);
    }
  }
}

void MediaItemUIView::OnMediaSessionMetadataChanged(
    const media_session::MediaMetadata& metadata) {
  title_ = metadata.title;

  for (auto& observer : observers_)
    observer.OnMediaItemUIMetadataChanged();
}

void MediaItemUIView::OnVisibleActionsChanged(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  has_many_actions_ =
      (actions.size() >= kMinVisibleActionsForExpanding ||
       base::Contains(
           actions,
           media_session::mojom::MediaSessionAction::kEnterPictureInPicture) ||
       base::Contains(
           actions,
           media_session::mojom::MediaSessionAction::kExitPictureInPicture));
  ForceExpandedState();

  for (auto& observer : observers_)
    observer.OnMediaItemUIActionsChanged();
}

void MediaItemUIView::OnMediaArtworkChanged(const gfx::ImageSkia& image) {
  has_artwork_ = !image.isNull();

  UpdateDismissButtonBackground();
  ForceExpandedState();
}

void MediaItemUIView::OnColorsChanged(SkColor foreground,
                                      SkColor foreground_disabled,
                                      SkColor background) {
  if (foreground_color_ != foreground ||
      foreground_disabled_color_ != foreground_disabled) {
    foreground_color_ = foreground;
    foreground_disabled_color_ = foreground_disabled;
    UpdateDismissButtonIcon();
  }

  if (background_color_ != background) {
    background_color_ = background;
    UpdateDismissButtonBackground();
  }
  if (footer_view_)
    footer_view_->OnColorsChanged(foreground, background);

  if (device_selector_view_)
    device_selector_view_->OnColorsChanged(foreground, background);
}

void MediaItemUIView::OnHeaderClicked() {
  // Since we disable the expand button, nothing happens on the
  // MediaNotificationView when the header is clicked. Treat the click as if we
  // were clicked directly.
  ContainerClicked();
}

void MediaItemUIView::OnDeviceSelectorViewSizeChanged() {
  OnSizeChanged();
}

ui::Layer* MediaItemUIView::GetSlideOutLayer() {
  return swipeable_container_->layer();
}

void MediaItemUIView::OnSlideChanged(bool in_progress) {
  // Make sure we are only scrolling in one dimension.
  if (scroll_view_ && in_progress && !is_sliding_ &&
      slide_out_controller_->GetGestureAmount()) {
    is_sliding_ = true;
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
  }

  if (!in_progress && scroll_view_ && is_sliding_) {
    is_sliding_ = false;
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kEnabled);
  }
}

void MediaItemUIView::OnSlideOut() {
  DismissNotification();
}

void MediaItemUIView::AddObserver(
    global_media_controls::MediaItemUIObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaItemUIView::RemoveObserver(
    global_media_controls::MediaItemUIObserver* observer) {
  observers_.RemoveObserver(observer);
}

const std::u16string& MediaItemUIView::GetTitle() const {
  return title_;
}

void MediaItemUIView::SetScrollView(views::ScrollView* scroll_view) {
  scroll_view_ = scroll_view;
}

void MediaItemUIView::UpdateFooterView(
    std::unique_ptr<MediaItemUIFooter> footer_view) {
  if (footer_view_) {
    RemoveChildView(footer_view_);
    delete footer_view_;
    footer_view_ = nullptr;
  }

  if (footer_view) {
    footer_view->OnColorsChanged(foreground_color_, background_color_);
    footer_view_ = AddChildView(std::move(footer_view));
  }
}

void MediaItemUIView::UpdateDeviceSelector(
    std::unique_ptr<MediaItemUIDeviceSelector> device_selector_view) {
  if (device_selector_view_) {
    RemoveChildView(device_selector_view_);
    delete device_selector_view_;
    device_selector_view_ = nullptr;
  }

  if (device_selector_view) {
    device_selector_view_ = AddChildView(std::move(device_selector_view));
    device_selector_view_->SetMediaItemUIView(this);
    view_->UpdateCornerRadius(message_center::kNotificationCornerRadius, 0);
    device_selector_view_->OnColorsChanged(foreground_color_,
                                           background_color_);
  }
}

views::ImageButton* MediaItemUIView::GetDismissButtonForTesting() {
  return dismiss_button_;
}

void MediaItemUIView::UpdateDismissButtonIcon() {
  int icon_size =
      is_cros_ ? kCrOSDismissButtonIconSize : kDismissButtonIconSize;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI))
    icon_size = kModernDismissButtonIconSize;

  views::SetImageFromVectorIconWithColor(
      dismiss_button_, vector_icons::kCloseRoundedIcon, icon_size,
      foreground_color_, foreground_disabled_color_);
}

void MediaItemUIView::UpdateDismissButtonBackground() {
  if (!has_artwork_) {
    dismiss_button_container_->SetBackground(nullptr);
    return;
  }

  dismiss_button_container_->SetBackground(views::CreateRoundedRectBackground(
      background_color_, kDismissButtonBackgroundRadius));
}

void MediaItemUIView::UpdateDismissButtonVisibility() {
  bool has_focus = false;
  if (GetFocusManager()) {
    views::View* focused_view = GetFocusManager()->GetFocusedView();
    if (focused_view)
      has_focus = Contains(focused_view);
  }

  dismiss_button_container_->SetVisible(IsMouseHovered() || has_focus);
}

void MediaItemUIView::DismissNotification() {
  for (auto& observer : observers_)
    observer.OnMediaItemUIDismissed(id_);
}

void MediaItemUIView::ForceExpandedState() {
  if (view_) {
    bool should_expand = has_many_actions_ || has_artwork_;
    view_->SetForcedExpandedState(&should_expand);
  }
}

void MediaItemUIView::ContainerClicked() {
  for (auto& observer : observers_)
    observer.OnMediaItemUIClicked(id_);
}

void MediaItemUIView::OnSizeChanged() {
  gfx::Size new_size;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI)) {
    new_size = kModernUISize;
  } else {
    new_size = is_expanded_ ? kExpandedSize : kNormalSize;
  }

  // |new_size| does not contain the height for the device selector view.
  // If this view is present, we should query it for its preferred height and
  // include that in |new_size|.
  if (device_selector_view_) {
    auto device_selector_view_size = device_selector_view_->GetPreferredSize();
    DCHECK(device_selector_view_size.width() == kWidth);
    new_size.set_height(new_size.height() + device_selector_view_size.height());
    view_->UpdateDeviceSelectorAvailability(
        device_selector_view_->GetVisible());
  }

  SetPreferredSize(new_size);
  PreferredSizeChanged();

  for (auto& observer : observers_)
    observer.OnMediaItemUISizeChanged();
}

BEGIN_METADATA(MediaItemUIView, views::Button)
ADD_READONLY_PROPERTY_METADATA(std::u16string, Title)
END_METADATA

}  // namespace global_media_controls
