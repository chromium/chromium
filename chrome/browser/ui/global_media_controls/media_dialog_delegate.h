// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_DELEGATE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

class MediaNotificationContainerImpl;
class OverlayMediaNotification;

// Delegate for MediaToolbarButtonController that is told when to display or
// hide a media session.
class MediaDialogDelegate {
 public:
  // Displays a media session and returns a pointer to the
  // MediaNotificationContainerImpl that was added to the dialog. The returned
  // MediaNotificationContainerImpl is owned by the MediaDialogDelegate.
  virtual MediaNotificationContainerImpl* ShowMediaSession(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) = 0;

  // Hides a media session.
  virtual void HideMediaSession(const std::string& id) = 0;

  // Returns an OverlayMediaNotification containing the media notification for
  // the given |id|. The notification should be removed from the dialog.
  virtual std::unique_ptr<OverlayMediaNotification> PopOut(
      const std::string& id,
      gfx::Rect bounds) = 0;

 protected:
  virtual ~MediaDialogDelegate();
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_DELEGATE_H_
