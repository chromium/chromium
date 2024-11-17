// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_OBSERVER_SET_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_OBSERVER_SET_H_

#include <map>
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "components/global_media_controls/public/media_item_ui.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"

namespace global_media_controls {

// A helper class that keeps track of and observes multiple MediaItemUIs on
// behalf of its owner.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIObserverSet
    : public MediaItemUIObserver {
 public:
  explicit MediaItemUIObserverSet(MediaItemUIObserver* owner);
  ~MediaItemUIObserverSet() override;
  MediaItemUIObserverSet(const MediaItemUIObserverSet&) = delete;
  MediaItemUIObserverSet& operator=(const MediaItemUIObserverSet&) = delete;

  void Observe(const std::string& id, MediaItemUI* item_ui);
  void StopObserving(const std::string& id);

  // MediaItemUIObserver:
  void OnMediaItemUISizeChanged() override;
  void OnMediaItemUIMetadataChanged() override;
  void OnMediaItemUIActionsChanged() override;
  void OnMediaItemUIClicked(const std::string& id,
                            bool activate_original_media) override;
  void OnMediaItemUIDismissed(const std::string& id) override;
  void OnMediaItemUIDestroyed(const std::string& id) override;
  void OnMediaItemUIShowDevices(const std::string& id) override;

 private:
  const raw_ptr<MediaItemUIObserver> owner_;
  std::map<std::string, raw_ptr<MediaItemUI, CtnExperimental>>
      observed_item_uis_;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_UI_OBSERVER_SET_H_
