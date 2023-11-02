// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_OBSERVER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

namespace global_media_controls {

class MediaSessionItemProducerObserver : public base::CheckedObserver {
 public:
  virtual void OnMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action) = 0;

 protected:
  ~MediaSessionItemProducerObserver() override = default;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_OBSERVER_H_
