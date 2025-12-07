// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard_video_plane.h"

#include "base/logging.h"

namespace chromecast {
namespace media {

namespace {

// Checks equality between two RectF objects.
bool RectFEqual(const RectF& r1, const RectF& r2) {
  return r1.x == r2.x && r1.y == r2.y && r1.width == r2.width &&
         r1.height == r2.height;
}

}  // namespace

StarboardVideoPlane::StarboardVideoPlane() {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

StarboardVideoPlane::~StarboardVideoPlane() = default;

void StarboardVideoPlane::SetGeometry(const RectF& display_rect,
                                      Transform transform) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardVideoPlane::SetGeometry,
                       weak_factory_.GetWeakPtr(), display_rect, transform));
    return;
  }

  if (current_plane_.has_value() &&
      RectFEqual(current_plane_->first, display_rect) &&
      current_plane_->second == transform) {
    // No change. Avoid spamming starboard with the same bounds.
    return;
  }

  // We store the current plane size so that we can run any newly-added
  // callbacks with this resolution.
  current_plane_ = std::make_pair(display_rect, transform);

  for (const auto& token_and_callback : token_to_callback_) {
    token_and_callback.second.Run(display_rect, transform);
  }
}

int64_t StarboardVideoPlane::RegisterCallback(
    GeometryChangedCallback callback) {
  int64_t token = 0;
  current_token_lock_.Acquire();
  token = current_token_++;
  current_token_lock_.Release();

  if (task_runner_->RunsTasksInCurrentSequence()) {
    RegisterCallbackForToken(token, std::move(callback));
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardVideoPlane::RegisterCallbackForToken,
                       weak_factory_.GetWeakPtr(), token, std::move(callback)));
  }
  return token;
}

void StarboardVideoPlane::RegisterCallbackForToken(
    int64_t token,
    GeometryChangedCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (current_plane_) {
    LOG(INFO) << "Running pending geometry callback. Setting video plane to "
              << current_plane_->first.width << "x"
              << current_plane_->first.height << ", offset ("
              << current_plane_->first.x << ", " << current_plane_->first.y
              << ")";
    callback.Run(current_plane_->first, current_plane_->second);
  }
  token_to_callback_[token] = std::move(callback);
}

void StarboardVideoPlane::UnregisterCallback(int64_t token) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StarboardVideoPlane::UnregisterCallback,
                                  weak_factory_.GetWeakPtr(), token));
    return;
  }

  token_to_callback_.erase(token);
}

}  // namespace media
}  // namespace chromecast
