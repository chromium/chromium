// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_COMMON_THUMBNAIL_SCORE_H_
#define COMPONENTS_HISTORY_CORE_COMMON_THUMBNAIL_SCORE_H_

#include <stdint.h>

#include <string>
#include "base/time/time.h"

namespace history {

// A set of metadata about a Thumbnail.
struct ThumbnailScore {
  // Initializes the ThumbnailScore to the absolute worst possible values
  // except for time, which is set to Now(), and redirect_hops_from_dest which
  // is set to 0.
  ThumbnailScore();

  // Builds a ThumbnailScore with the passed in values, and sets the
  // thumbnail generation time to Now().
  ThumbnailScore(double score, bool clipping, bool top);

  // Builds a ThumbnailScore with the passed in values.
  ThumbnailScore(double score, bool clipping, bool top, const base::Time& time);
  ~ThumbnailScore();

  // Tests for equivalence between two ThumbnailScore objects.
  bool Equals(const ThumbnailScore& rhs) const;

  // Returns string representation of this object.
  std::string ToString() const;

  // How "boring" a thumbnail is. The boring score is the 0,1 ranged
  // percentage of pixels that are the most common luma. Higher boring
  // scores indicate that a higher percentage of a bitmap are all the
  // same brightness (most likely the same color).
  //
  // The score should only be used for comparing two thumbnails taken from
  // the same page to see which one is more boring/interesting. The
  // absolute score is not suitable for judging whether the thumbnail is
  // actually boring or not. For instance, www.google.com is very
  // succinct, so the boring score can be as high as 0.9, depending on the
  // browser window size.
  double boring_score;

  // Whether the thumbnail was taken with height greater than
  // width or width greater than height and the aspect ratio less than
  // kTooWideAspectRatio. In cases where we don't have `good_clipping`,
  // the thumbnails are either clipped from the horizontal center of the
  // window, or are otherwise weirdly stretched.
  bool good_clipping;

  // Whether this thumbnail was taken while the renderer was
  // displaying the top of the page. Most pages are more recognizable
  // by their headers then by a set of random text half way down the
  // page; i.e. most MediaWiki sites would be indistinguishable by
  // thumbnails with `at_top` set to false.
  bool at_top;

  // Whether this thumbnail was taken after load was completed.
  // Thumbnails taken while page loading may only contain partial
  // contents.
  bool load_completed;

  // Record the time when a thumbnail was taken. This is used to make
  // sure thumbnails are kept fresh.
  base::Time time_at_snapshot;

  // The number of hops from the final destination page that this thumbnail was
  // taken at. When a thumbnail is taken, this will always be the redirect
  // destination (value of 0).
  //
  // For the most visited view, we'll sometimes get thumbnails for URLs in the
  // middle of a redirect chain. In this case, the top sites component will set
  // this value so the distance from the destination can be taken into account
  // by the comparison function.
  //
  // If "http://google.com/" redirected to "http://www.google.com/", then
  // a thumbnail for the first would have a redirect hop of 1, and the second
  // would have a redirect hop of 0.
  int redirect_hops_from_dest;

  // How bad a thumbnail needs to be before we completely ignore it.
  static const double kThumbnailMaximumBoringness;

  // Time before we take a worse thumbnail (subject to
  // kThumbnailMaximumBoringness) over what's currently in the database
  // for freshness.
  static const base::TimeDelta kUpdateThumbnailTime;

  // Penalty of how much more boring a thumbnail should be per hour.
  static const double kThumbnailDegradePerHour;

  // If a thumbnail is taken with the aspect ratio greater than or equal to
  // this value, `good_clipping` is to false.
  static const double kTooWideAspectRatio;

  // Checks whether we should consider updating a new thumbnail based on
  // this score. For instance, we don't have to update a new thumbnail
  // if the current thumbnail is new and interesting enough.
  bool ShouldConsiderUpdating();
};

// Checks whether we should replace one thumbnail with another.
bool ShouldReplaceThumbnailWith(const ThumbnailScore& current,
                                const ThumbnailScore& replacement);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_COMMON_THUMBNAIL_SCORE_H_
