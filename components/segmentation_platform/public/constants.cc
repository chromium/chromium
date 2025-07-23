// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/constants.h"

#include <cstring>

#include "base/strings/string_util.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

namespace {

std::string SegmentIdToHistogramVariantInternal(
    const proto::SegmentId segment_id) {
  std::string name = proto::SegmentId_Name(segment_id);

  // Remove the project name prefixes from the segment IDs and change to lower
  // case.
  constexpr char kOptimizationSegmentationPrefix[] =
      "OPTIMIZATION_TARGET_SEGMENTATION_";
  constexpr char kOptimizationTargetPrefix[] = "OPTIMIZATION_TARGET_";
  if (base::StartsWith(name, kOptimizationSegmentationPrefix)) {
    name.erase(0, strlen(kOptimizationSegmentationPrefix));
  } else if (base::StartsWith(name, kOptimizationTargetPrefix)) {
    name.erase(0, strlen(kOptimizationTargetPrefix));
  }
  name = base::ToLowerASCII(name);

  // Convert to camel case.
  std::string result;
  bool cap_first_letter = true;
  for (char ch : name) {
    if ('a' <= ch && ch <= 'z') {
      if (cap_first_letter) {
        constexpr char toupper = ('A' - 'a');
        result += ch + toupper;
      } else {
        result += ch;
      }
      cap_first_letter = false;
    } else if ('0' <= ch && ch <= '9') {
      result += ch;
      cap_first_letter = true;
    } else {
      CHECK_EQ(ch, '_');
      cap_first_letter = true;
    }
  }
  return result;
}

}  // namespace

// Please keep in sync with SegmentationModel variant in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
// Should also update the field trials allowlist in
// go/segmentation-field-trials-map.
std::string SegmentIdToHistogramVariant(proto::SegmentId segment_id) {
  // This case is reached when UNKNOWN segment is valid, in case of boolean
  // segment results.
  if (segment_id == proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    return "Other";
  }
  return SegmentIdToHistogramVariantInternal(segment_id);
}

std::string GetSubsegmentKey(const std::string& segmentation_key) {
  return segmentation_key + kSubsegmentDiscreteMappingSuffix;
}

}  // namespace segmentation_platform
