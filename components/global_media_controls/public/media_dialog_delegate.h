// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_DIALOG_DELEGATE_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_DIALOG_DELEGATE_H_

#include <string>

#include "base/memory/weak_ptr.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace global_media_controls {

class MediaItemUI;

// Delegate for MediaToolbarButtonController that is told when to display or
// hide a media session.
class MediaDialogDelegate {
 public:
  // Displays a media session and returns a pointer to the MediaItemUI that was
  // added to the dialog. The returned MediaItemUI is owned by the
  // MediaDialogDelegate.
  virtual MediaItemUI* ShowMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) = 0;

  // Hides a media item.
  virtual void HideMediaItem(const std::string& id) = 0;

  // Updates the media item's UI.
  virtual void RefreshMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) = 0;

  // Closes the dialog.
  virtual void HideMediaDialog() = 0;

  // Changes focus to the dialog.
  virtual void Focus() = 0;

 protected:
  virtual ~MediaDialogDelegate() = default;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_DIALOG_DELEGATE_H_
