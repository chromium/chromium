// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENTATION_RESULT_PREFS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENTATION_RESULT_PREFS_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

class PrefService;

namespace segmentation_platform {

using proto::SegmentId;

// Struct containing information about the selected segment. Convenient for
// reading and writing to prefs.
struct SelectedSegment {
 public:
  SelectedSegment(SegmentId segment_id, std::optional<float> rank);
  ~SelectedSegment();

  // The segment selection result.
  SegmentId segment_id;

  // The discrete score computed based on the `segment_id` model execution. If a
  // discrete mapping is not provided, the value will be equal to the model
  // score. Otherwise the value will be the mapped score based on the mapping.
  // May not be available in prefs for versions older than M107.
  std::optional<float> rank;

  // The time when the segment was selected.
  base::Time selection_time;

  // Whether or not the segment selection result is in use.
  bool in_use;
};

// Stores the result of segmentation into prefs for faster lookup. The result
// consists of (1) The selected segment ID. (2) The time when the segment was
// first selected. Used to enforce segment selection TTL. (3) Whether the
// selected segment has started to be used by clients.
class SegmentationResultPrefs {
 public:
  explicit SegmentationResultPrefs(PrefService* pref_service);
  virtual ~SegmentationResultPrefs() = default;

  // Disallow copy/assign.
  SegmentationResultPrefs(const SegmentationResultPrefs& other) = delete;
  SegmentationResultPrefs operator=(const SegmentationResultPrefs& other) =
      delete;

  // Writes the selected segment to prefs. Deletes the previous results if
  // |selected_segment| is empty.
  virtual void SaveSegmentationResultToPref(
      const std::string& result_key,
      const std::optional<SelectedSegment>& selected_segment);

  // Reads the selected segment from pref, if any.
  virtual std::optional<SelectedSegment> ReadSegmentationResultFromPref(
      const std::string& result_key);

 private:
  raw_ptr<PrefService> prefs_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_SEGMENTATION_RESULT_PREFS_H_
