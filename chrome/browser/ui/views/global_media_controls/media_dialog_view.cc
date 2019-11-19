// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

using media_session::mojom::MediaSessionAction;

// static
MediaDialogView* MediaDialogView::instance_ = nullptr;

// static
bool MediaDialogView::has_been_opened_ = false;

// static
void MediaDialogView::ShowDialog(views::View* anchor_view,
                                 MediaNotificationService* service) {
  DCHECK(!instance_);
  DCHECK(service);
  instance_ = new MediaDialogView(anchor_view, service);

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();

  base::UmaHistogramBoolean("Media.GlobalMediaControls.RepeatUsage",
                            has_been_opened_);
  has_been_opened_ = true;
}

// static
void MediaDialogView::HideDialog() {
  if (IsShowing()) {
    instance_->service_->SetDialogDelegate(nullptr);
    instance_->GetWidget()->Close();
  }

  // Set |instance_| to nullptr so that |IsShowing()| returns false immediately.
  // We also set to nullptr in |WindowClosing()| (which happens asynchronously),
  // since |HideDialog()| is not always called.
  instance_ = nullptr;
}

// static
bool MediaDialogView::IsShowing() {
  return instance_ != nullptr;
}

MediaNotificationContainerImpl* MediaDialogView::ShowMediaSession(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  auto container =
      std::make_unique<MediaNotificationContainerImplView>(id, item);
  MediaNotificationContainerImplView* container_ptr = container.get();
  container_ptr->AddObserver(this);
  observed_containers_[id] = container_ptr;

  active_sessions_view_->ShowNotification(id, std::move(container));
  SizeToContents();

  for (auto& observer : observers_)
    observer.OnMediaSessionShown();

  return container_ptr;
}

void MediaDialogView::HideMediaSession(const std::string& id) {
  active_sessions_view_->HideNotification(id);

  if (active_sessions_view_->empty())
    HideDialog();
  else
    SizeToContents();

  for (auto& observer : observers_)
    observer.OnMediaSessionHidden();
}

std::unique_ptr<OverlayMediaNotification> MediaDialogView::PopOut(
    const std::string& id,
    gfx::Rect bounds) {
  return active_sessions_view_->PopOut(id, bounds);
}

bool MediaDialogView::Close() {
  return Cancel();
}

void MediaDialogView::AddedToWidget() {
  int corner_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_HIGH);

  views::BubbleFrameView* frame = GetBubbleFrameView();
  if (frame) {
    frame->SetCornerRadius(corner_radius);
  }

  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));

  service_->SetDialogDelegate(this);
}

gfx::Size MediaDialogView::CalculatePreferredSize() const {
  // If we have active sessions, then fit to them.
  if (!active_sessions_view_->empty())
    return views::BubbleDialogDelegateView::CalculatePreferredSize();

  // Otherwise, use a standard size for bubble dialogs.
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, 1);
}

void MediaDialogView::OnContainerExpanded(bool expanded) {
  SizeToContents();
}

void MediaDialogView::OnContainerMetadataChanged() {
  for (auto& observer : observers_)
    observer.OnMediaSessionMetadataUpdated();
}

void MediaDialogView::OnContainerDestroyed(const std::string& id) {
  auto iter = observed_containers_.find(id);
  DCHECK(iter != observed_containers_.end());

  iter->second->RemoveObserver(this);
  observed_containers_.erase(iter);
}

void MediaDialogView::AddObserver(MediaDialogViewObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaDialogView::RemoveObserver(MediaDialogViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

const std::map<const std::string, MediaNotificationContainerImplView*>&
MediaDialogView::GetNotificationsForTesting() const {
  return active_sessions_view_->notifications_for_testing();
}

MediaDialogView::MediaDialogView(views::View* anchor_view,
                                 MediaNotificationService* service)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      service_(service),
      active_sessions_view_(
          AddChildView(std::make_unique<MediaNotificationListView>())) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
  DCHECK(service_);
}

MediaDialogView::~MediaDialogView() {
  for (auto container_pair : observed_containers_)
    container_pair.second->RemoveObserver(this);
}

void MediaDialogView::Init() {
  // Remove margins.
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void MediaDialogView::WindowClosing() {
  if (instance_ == this) {
    instance_ = nullptr;
    service_->SetDialogDelegate(nullptr);
  }
}
