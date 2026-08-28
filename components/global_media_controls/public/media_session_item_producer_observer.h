// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_OBSERVER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_OBSERVER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "services/media_session/public/cpp/media_position.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace global_media_controls {

class MediaSessionItemProducerObserver : public base::CheckedObserver {
 public:
  virtual void OnMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action) = 0;
  virtual void OnMediaSessionInfoChanged(
      const std::string& id,
      const media_session::mojom::MediaSessionInfoPtr& session_info) {}
  virtual void OnMediaSessionActionsChanged(
      const std::string& id,
      const std::vector<media_session::mojom::MediaSessionAction>& actions) {}
  virtual void OnMediaSessionPositionChanged(
      const std::string& id,
      const std::optional<media_session::MediaPosition>& position) {}

 protected:
  ~MediaSessionItemProducerObserver() override = default;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_OBSERVER_H_
