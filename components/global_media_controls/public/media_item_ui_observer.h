// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_OBSERVER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

namespace global_media_controls {

class MediaItemUIObserver : public base::CheckedObserver {
 public:
  // Called when the size of the item UI has changed.
  virtual void OnMediaItemUISizeChanged() {}

  // Called when the metadata displayed in the item UI changes.
  virtual void OnMediaItemUIMetadataChanged() {}

  // Called when the action buttons in the item UI change.
  virtual void OnMediaItemUIActionsChanged() {}

  // Called when the item UI is clicked along with whether we want to activate
  // the web contents for the original media.
  virtual void OnMediaItemUIClicked(const std::string& id,
                                    bool activate_original_media = true) {}

  // Called when the item UI is dismissed from the dialog.
  virtual void OnMediaItemUIDismissed(const std::string& id) {}

  // Called when the item UI is about to be deleted.
  virtual void OnMediaItemUIDestroyed(const std::string& id) {}

  // Called when the start casting button is clicked on the quick settings media
  // view to request showing device list using device selector view in the quick
  // settings media detailed view.
  virtual void OnMediaItemUIShowDevices(const std::string& id) {}

 protected:
  ~MediaItemUIObserver() override = default;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_OBSERVER_H_
