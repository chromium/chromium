// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_OBSERVER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/gfx/geometry/rect.h"

class MediaNotificationContainerObserver : public base::CheckedObserver {
 public:
  // Called when the size of the container has changed.
  virtual void OnContainerSizeChanged() {}

  // Called when the metadata displayed in the container changes.
  virtual void OnContainerMetadataChanged() {}

  // Called when the action buttons in the container change.
  virtual void OnContainerActionsChanged() {}

  // Called when the container is clicked.
  virtual void OnContainerClicked(const std::string& id) {}

  // Called when the container is dismissed from the dialog.
  virtual void OnContainerDismissed(const std::string& id) {}

  // Called when the container is about to be deleted.
  virtual void OnContainerDestroyed(const std::string& id) {}

  // Called when the container has been dragged out of a dialog.
  virtual void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) {}

  // Called when the audio output device for the container should change
  virtual void OnAudioSinkChosen(const std::string& id,
                                 const std::string& sink_id) {}

 protected:
  ~MediaNotificationContainerObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_OBSERVER_H_
