// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_MANAGER_OBSERVER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace global_media_controls {

class MediaItemManagerObserver : public base::CheckedObserver {
 public:
  // Called when the list of active, cast, or frozen media items
  // changes.
  virtual void OnItemListChanged() = 0;

  // Called when a media dialog associated with the service is opened or closed.
  virtual void OnMediaDialogOpened() = 0;
  virtual void OnMediaDialogClosed() = 0;

 protected:
  ~MediaItemManagerObserver() override = default;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_MANAGER_OBSERVER_H_
