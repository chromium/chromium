// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_MODEL_PROVIDER_H_
#define COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_MODEL_PROVIDER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/observer_list_types.h"

class MediaEffectsModelProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when background segmentation model becomes available or was
    // updated. `path` will contain the file system path to the new model file.
    virtual void OnBackgroundSegmentationModelUpdated(
        const base::FilePath& path) = 0;
  };

  virtual ~MediaEffectsModelProvider() = default;

  // Subscribes to background segmentation model changes. If the model file is
  // already known, the `Observer::OnBackgroundSegmentationModelUpdated()`
  // should be immediately invoked with the latest model file.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_MODEL_PROVIDER_H_
