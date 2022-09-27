// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"

#include "base/json/values_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/segmentation_platform/internal/constants.h"

namespace segmentation_platform {

SelectedSegment::SelectedSegment(SegmentId segment_id,
                                 absl::optional<float> rank)
    : segment_id(segment_id), rank(rank), in_use(false) {}

SelectedSegment::~SelectedSegment() = default;

SegmentationResultPrefs::SegmentationResultPrefs(PrefService* pref_service)
    : prefs_(pref_service) {}

void SegmentationResultPrefs::SaveSegmentationResultToPref(
    const std::string& result_key,
    const absl::optional<SelectedSegment>& selected_segment) {
  ScopedDictPrefUpdate update(prefs_, kSegmentationResultPref);
  base::Value::Dict& dictionary = update.Get();
  if (!selected_segment.has_value()) {
    dictionary.Remove(result_key);
    return;
  }

  base::Value::Dict segmentation_result;
  segmentation_result.Set("segment_id", selected_segment->segment_id);
  if (selected_segment->rank)
    segmentation_result.Set("segment_rank", *selected_segment->rank);
  segmentation_result.Set("in_use", selected_segment->in_use);
  segmentation_result.Set("selection_time",
                          base::TimeToValue(selected_segment->selection_time));
  dictionary.Set(result_key, std::move(segmentation_result));
}

absl::optional<SelectedSegment>
SegmentationResultPrefs::ReadSegmentationResultFromPref(
    const std::string& result_key) {
  const base::Value::Dict& dictionary =
      prefs_->GetDict(kSegmentationResultPref);

  const base::Value* value = dictionary.Find(result_key);
  if (!value)
    return absl::nullopt;

  const base::DictionaryValue& segmentation_result =
      base::Value::AsDictionaryValue(*value);

  absl::optional<int> segment_id = segmentation_result.FindIntKey("segment_id");
  absl::optional<float> rank =
      segmentation_result.FindDoubleKey("segment_rank");
  absl::optional<bool> in_use = segmentation_result.FindBoolKey("in_use");
  absl::optional<base::Time> selection_time =
      base::ValueToTime(segmentation_result.FindPath("selection_time"));

  SelectedSegment selected_segment(static_cast<SegmentId>(segment_id.value()),
                                   rank);
  if (in_use.has_value())
    selected_segment.in_use = in_use.value();
  if (selection_time.has_value())
    selected_segment.selection_time = selection_time.value();

  return selected_segment;
}

}  // namespace segmentation_platform
