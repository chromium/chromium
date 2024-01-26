// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SCORE_PROVIDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SCORE_PROVIDER_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

using proto::SegmentId;

class SegmentInfoDatabase;

// Result of a single segment.
// TODO(shaktisahu, ssid): Modify the result fields as the API evolves.
struct SegmentScore {
  // Raw scores from the model.
  std::optional<ModelProvider::Response> scores;

  // Constructors.
  SegmentScore();
  SegmentScore(const SegmentScore& other);
  ~SegmentScore();
};

// Used for retrieving the result of a particular model. The results are read
// from the database on startup and never modified during the current session.
// Note that this class is currently unused, but can be used to serve future
// clients and be modified as needed.
class SegmentScoreProvider {
 public:
  SegmentScoreProvider() = default;
  virtual ~SegmentScoreProvider() = default;

  using SegmentScoreCallback = base::OnceCallback<void(const SegmentScore&)>;

  // Creates the instance.
  static std::unique_ptr<SegmentScoreProvider> Create(
      SegmentInfoDatabase* segment_database,
      base::flat_set<proto::SegmentId> segment_ids);

  // Called to initialize the manager. Reads results from the database into
  // memory on startup. Must be invoked before calling any other method.
  virtual void Initialize(base::OnceClosure callback) = 0;

  // Client API to get the score for a single segment. Returns the cached score
  // from the last session.
  // Note that there is no strong reason to keep this async, feel free to change
  // this to sync if needed.
  virtual void GetSegmentScore(SegmentId segment_id,
                               SegmentScoreCallback callback) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENT_SCORE_PROVIDER_H_
