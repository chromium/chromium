// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_H_

namespace global_media_controls {

class MediaItemUIObserver;

class MediaItemUI {
 public:
  virtual void AddObserver(MediaItemUIObserver* observer) = 0;
  virtual void RemoveObserver(MediaItemUIObserver* observer) = 0;

 protected:
  virtual ~MediaItemUI() = default;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_H_
