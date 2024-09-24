// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_VIDEO_PLANE_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_VIDEO_PLANE_H_

#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"

namespace chromecast {
namespace media {

// A VideoPlane that can be used to register callbacks that will be run when the
// plane's geometry changes. Must be created on a sequence.
//
// Public functions can be called from any sequence. Note that registered
// callbacks will be run from the sequence on which the StarboardVideoPlane was
// constructed. To run the callback on another sequence, use base::BindPostTask
// or similar logic.
class StarboardVideoPlane : public VideoPlane {
 public:
  using GeometryChangedCallback =
      base::RepeatingCallback<void(const RectF& display_rect,
                                   Transform transform)>;

  StarboardVideoPlane();

  // Disallow copy and assign.
  StarboardVideoPlane(const StarboardVideoPlane&) = delete;
  StarboardVideoPlane& operator=(const StarboardVideoPlane&) = delete;

  ~StarboardVideoPlane() override;

  // Registers a callback that will be run when the geometry changes. Returns an
  // opaque token that can be used to remove the callback later.
  //
  // If this plane's geometry has already been set, `callback` will be run with
  // that geometry. This means that `callback` may be run before this function
  // returns.
  int64_t RegisterCallback(GeometryChangedCallback callback);

  // Unregisters a callback from the list of callbacks that are run when the
  // geometry changes. `token` should be the value returned from a previous
  // RegisterCallback call. If no matching token is found, this is a no-op.
  void UnregisterCallback(int64_t token);

  // VideoPlane implementation:
  void SetGeometry(const RectF& display_rect, Transform transform) override;

 private:
  // A helper function that must be run on task_runner_. Registers `callback`
  // with the specified `token`.
  void RegisterCallbackForToken(int64_t token,
                                GeometryChangedCallback callback);

  base::flat_map<int64_t, GeometryChangedCallback> token_to_callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::Lock current_token_lock_;
  // This must only be accessed while current_token_lock_ is held.
  int64_t current_token_ = 0;

  // The rectangle and transform representing the current video plane. If this
  // is set when a new callback is registered, we will call the callback with
  // this info.
  std::optional<std::pair<RectF, Transform>> current_plane_;

  // This must be destructed first.
  base::WeakPtrFactory<StarboardVideoPlane> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_VIDEO_PLANE_H_
