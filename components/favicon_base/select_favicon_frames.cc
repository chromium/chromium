// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/select_favicon_frames.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "components/favicon_base/favicon_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"

namespace {

size_t BiggestCandidate(const std::vector<gfx::Size>& candidate_sizes) {
  size_t max_index = 0;
  int max_area = candidate_sizes[0].GetArea();
  for (size_t i = 1; i < candidate_sizes.size(); ++i) {
    int area = candidate_sizes[i].GetArea();
    if (area > max_area) {
      max_area = area;
      max_index = i;
    }
  }
  return max_index;
}

SkBitmap SampleNearestNeighbor(const SkBitmap& contents, int desired_size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(desired_size, desired_size);
  if (!contents.isOpaque())
    bitmap.eraseARGB(0, 0, 0, 0);

  {
    SkCanvas canvas(bitmap, SkSurfaceProps{});
    canvas.drawImageRect(contents.asImage(),
                         SkRect::MakeIWH(desired_size, desired_size),
                         SkSamplingOptions());
  }

  return bitmap;
}

size_t GetCandidateIndexWithBestScore(
    const std::vector<gfx::Size>& candidate_sizes,
    int desired_size,
    float* output_score) {
  DCHECK_NE(desired_size, 0);

  // Try to find an exact match.
  for (size_t i = 0; i < candidate_sizes.size(); ++i) {
    if (candidate_sizes[i].width() == desired_size &&
        candidate_sizes[i].height() == desired_size) {
      *output_score = 1;
      return i;
    }
  }

  // Huge favicon bitmaps often have a completely different visual style from
  // smaller favicon bitmaps. Avoid them.
  const int kHugeEdgeSize = desired_size * 8;

  // Order of preference:
  // 1) Bitmaps with width and height smaller than |kHugeEdgeSize|.
  // 2) Bitmaps which need to be scaled down instead of up.
  // 3) Bitmaps which do not need to be scaled as much.
  size_t candidate_index = std::numeric_limits<size_t>::max();
  float candidate_score = 0;
  for (size_t i = 0; i < candidate_sizes.size(); ++i) {
    float average_edge =
        (candidate_sizes[i].width() + candidate_sizes[i].height()) / 2.0f;

    float score = 0;
    if (candidate_sizes[i].width() >= kHugeEdgeSize ||
        candidate_sizes[i].height() >= kHugeEdgeSize) {
      score = std::min(1.0f, desired_size / average_edge) * 0.01f;
    } else if (candidate_sizes[i].width() >= desired_size &&
               candidate_sizes[i].height() >= desired_size) {
      score = desired_size / average_edge * 0.01f + 0.15f;
    } else {
      score = std::min(1.0f, average_edge / desired_size) * 0.01f + 0.1f;
    }

    if (candidate_index == std::numeric_limits<size_t>::max() ||
        score > candidate_score) {
      candidate_index = i;
      candidate_score = score;
    }
  }
  *output_score = candidate_score;

  return candidate_index;
}

// Represents the index of the best candidate for |desired_size| from the
// |candidate_sizes| passed into GetCandidateIndicesWithBestScores().
struct SelectionResult {
  // index in |candidate_sizes| of the best candidate.
  size_t index;

  // The desired size for which |index| is the best candidate.
  int desired_size;
};

void GetCandidateIndicesWithBestScores(
    const std::vector<gfx::Size>& candidate_sizes,
    const std::vector<int>& desired_sizes,
    float* match_score,
    std::vector<SelectionResult>* results) {
  if (candidate_sizes.empty() || desired_sizes.empty()) {
    if (match_score)
      *match_score = 0.0f;
    return;
  }

  if (base::Contains(desired_sizes, 0)) {
    // Just return the biggest image available.
    SelectionResult result;
    result.index = BiggestCandidate(candidate_sizes);
    result.desired_size = 0;
    results->push_back(result);
    if (match_score)
      *match_score = 1.0f;
    return;
  }

  float total_score = 0;
  for (size_t i = 0; i < desired_sizes.size(); ++i) {
    float score;
    SelectionResult result;
    result.desired_size = desired_sizes[i];
    result.index = GetCandidateIndexWithBestScore(
        candidate_sizes, result.desired_size, &score);
    results->push_back(result);
    total_score += score;
  }

  if (match_score)
    *match_score = total_score / desired_sizes.size();
}

// Resize |source_bitmap|
SkBitmap GetResizedBitmap(const SkBitmap& source_bitmap,
                          gfx::Size original_size,
                          int desired_size_in_pixel) {
  if (desired_size_in_pixel == 0 ||
      (original_size.width() == desired_size_in_pixel &&
       original_size.height() == desired_size_in_pixel)) {
    return source_bitmap;
  }
  if (desired_size_in_pixel % original_size.width() == 0 &&
      desired_size_in_pixel % original_size.height() == 0) {
    return SampleNearestNeighbor(source_bitmap, desired_size_in_pixel);
  }
  return skia::ImageOperations::Resize(source_bitmap,
                                       skia::ImageOperations::RESIZE_LANCZOS3,
                                       desired_size_in_pixel,
                                       desired_size_in_pixel);
}

class FaviconImageSource : public gfx::ImageSkiaSource {
 public:
  FaviconImageSource() = default;

  FaviconImageSource(const FaviconImageSource&) = delete;
  FaviconImageSource& operator=(const FaviconImageSource&) = delete;

  ~FaviconImageSource() override = default;

  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    const gfx::ImageSkiaRep* rep = nullptr;
    // gfx::ImageSkia passes one of the resource scale factors. The source
    // should return:
    // 1) The ImageSkiaRep with the highest scale if all available
    // scales are smaller than |scale|.
    // 2) The ImageSkiaRep with the smallest one that is larger than |scale|.
    // Note: Keep this logic consistent with the PNGImageSource in
    // ui/gfx/image.cc.
    // TODO(oshima): consolidate these logic into one place.
    for (std::vector<gfx::ImageSkiaRep>::const_iterator iter =
             image_skia_reps_.begin();
         iter != image_skia_reps_.end(); ++iter) {
      if ((*iter).scale() == scale)
        return (*iter);
      if (!rep || rep->scale() < (*iter).scale())
        rep = &(*iter);
      if (rep->scale() >= scale)
        break;
    }
    DCHECK(rep);
    return rep ? *rep : gfx::ImageSkiaRep();
  }

  void AddImageSkiaRep(const gfx::ImageSkiaRep& rep) {
    image_skia_reps_.push_back(rep);
  }

 private:
  std::vector<gfx::ImageSkiaRep> image_skia_reps_;
};

}  // namespace

const float kSelectFaviconFramesInvalidScore = -1.0f;

gfx::ImageSkia CreateFaviconImageSkia(
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_sizes,
    int desired_size_in_dip,
    float* score) {
  DCHECK_EQ(bitmaps.size(), original_sizes.size());

  const std::vector<float>& favicon_scales = favicon_base::GetFaviconScales();
  std::vector<int> desired_sizes;

  if (desired_size_in_dip == 0) {
    desired_sizes.push_back(0);
  } else {
    for (auto iter = favicon_scales.begin(); iter != favicon_scales.end();
         ++iter) {
      desired_sizes.push_back(
          static_cast<int>(ceil(desired_size_in_dip * (*iter))));
    }
  }

  std::vector<SelectionResult> results;
  GetCandidateIndicesWithBestScores(original_sizes,
                                    desired_sizes,
                                    score,
                                    &results);
  if (results.size() == 0)
    return gfx::ImageSkia();

  if (desired_size_in_dip == 0) {
    size_t index = results[0].index;
    return gfx::ImageSkia::CreateFromBitmap(bitmaps[index], 1.0f);
  }

  auto image_source = std::make_unique<FaviconImageSource>();

  for (size_t i = 0; i < results.size(); ++i) {
    size_t index = results[i].index;
    image_source->AddImageSkiaRep(
        gfx::ImageSkiaRep(GetResizedBitmap(bitmaps[index],
                                           original_sizes[index],
                                           desired_sizes[i]),
                          favicon_scales[i]));
  }
  return gfx::ImageSkia(std::move(image_source),
                        gfx::Size(desired_size_in_dip, desired_size_in_dip));
}

void SelectFaviconFrameIndices(const std::vector<gfx::Size>& frame_pixel_sizes,
                               const std::vector<int>& desired_sizes,
                               std::vector<size_t>* best_indices,
                               float* match_score) {
  std::vector<SelectionResult> results;
  GetCandidateIndicesWithBestScores(
      frame_pixel_sizes, desired_sizes, match_score, &results);

  if (!best_indices)
    return;

  std::set<size_t> already_added;
  for (size_t i = 0; i < results.size(); ++i) {
    size_t index = results[i].index;
    // GetCandidateIndicesWithBestScores() will return duplicate indices if the
    // bitmap data with |frame_pixel_sizes[index]| should be used for multiple
    // scale factors. Remove duplicates here such that |best_indices| contains
    // no duplicates.
    if (already_added.find(index) == already_added.end()) {
      already_added.insert(index);
      best_indices->push_back(index);
    }
  }
}
