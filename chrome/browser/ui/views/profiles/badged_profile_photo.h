// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_BADGED_PROFILE_PHOTO_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_BADGED_PROFILE_PHOTO_H_

#include "ui/gfx/image/image.h"
#include "ui/views/view.h"

// Creates a bagded profile photo for the current profile card in the
// profile chooser menu. The view includes the photo and the badge itself,
// but not the bagde border to the right and to the bottom.
// More badges, e.g. for syncing, will be supported in the future (project
// DICE).
class BadgedProfilePhoto : public views::View {
 public:
  enum BadgeType {
    BADGE_TYPE_NONE,
    BADGE_TYPE_SUPERVISOR,
    BADGE_TYPE_CHILD,
    BADGE_TYPE_SYNC_COMPLETE,
    BADGE_TYPE_SYNC_ERROR,
    BADGE_TYPE_SYNC_PAUSED,
    BADGE_TYPE_SYNC_DISABLED,
    BADGE_TYPE_SYNC_OFF,
  };

  static const char kViewClassName[];

  // Width/Height of the profile photo.
  static constexpr int kImageSize = 40;

  // Constructs a View hierarchy with the gfx::ImageSkia corresponding to
  // |badge_type| positioned in the bottom-right corner of |profile_photo|. In
  // RTL mode the badge is positioned in the bottom-left corner. The profile
  // photo will be adjusted to be circular and of size 40x40 (kImageSize).
  // If |badge_type| is BADGE_TYPE_NONE no badge will be placed on top of the
  // profile photo. The size of the View is fixed.
  // TODO(tangltom): Add accessibility features in the future.
  BadgedProfilePhoto(BadgeType badge_type, const gfx::Image& profile_photo);

  // views::View:
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BadgedProfilePhoto);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_BADGED_PROFILE_PHOTO_H_
