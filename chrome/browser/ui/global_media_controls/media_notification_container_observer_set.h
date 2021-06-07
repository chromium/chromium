// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_OBSERVER_SET_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_OBSERVER_SET_H_

#include <map>
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"

// A helper class that keeps track of and observes multiple
// MediaNotificationContainerImpls on behalf of its owner.
class MediaNotificationContainerObserverSet
    : public MediaNotificationContainerObserver {
 public:
  explicit MediaNotificationContainerObserverSet(
      MediaNotificationContainerObserver* owner);
  ~MediaNotificationContainerObserverSet() override;
  MediaNotificationContainerObserverSet(
      const MediaNotificationContainerObserverSet&) = delete;
  MediaNotificationContainerObserverSet& operator=(
      const MediaNotificationContainerObserverSet&) = delete;

  void Observe(const std::string& id,
               MediaNotificationContainerImpl* container);
  void StopObserving(const std::string& id);

  // MediaNotificationContainerObserver:
  void OnContainerSizeChanged() override;
  void OnContainerMetadataChanged() override;
  void OnContainerActionsChanged() override;
  void OnContainerClicked(const std::string& id) override;
  void OnContainerDismissed(const std::string& id) override;
  void OnContainerDestroyed(const std::string& id) override;
  void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) override;
  void OnAudioSinkChosen(const std::string& id,
                         const std::string& sink_id) override;

 private:
  MediaNotificationContainerObserver* const owner_;
  std::map<std::string, MediaNotificationContainerImpl*> observed_containers_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_OBSERVER_SET_H_
