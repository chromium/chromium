// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/common/thumbnail_score.h"

#include "base/strings/stringprintf.h"

namespace history {

using base::Time;

// static
const double ThumbnailScore::kThumbnailMaximumBoringness = 0.94;
// static
const base::TimeDelta ThumbnailScore::kUpdateThumbnailTime = base::Days(1);
// static
const double ThumbnailScore::kThumbnailDegradePerHour = 0.01;
// static
const double ThumbnailScore::kTooWideAspectRatio = 2.0;

// Calculates a numeric score from traits about where a snapshot was
// taken. The lower the better. We store the raw components in the
// database because I'm sure this will evolve and I don't want to break
// databases.
static int GetThumbnailType(const ThumbnailScore& score) {
  int type = 0;
  if (!score.at_top)
    type += 1;
  if (!score.good_clipping)
    type += 2;
  if (!score.load_completed)
    type += 3;
  return type;
}

ThumbnailScore::ThumbnailScore()
    : boring_score(1.0),
      good_clipping(false),
      at_top(false),
      load_completed(false),
      time_at_snapshot(Time::Now()),
      redirect_hops_from_dest(0) {
}

ThumbnailScore::ThumbnailScore(double score, bool clipping, bool top)
    : boring_score(score),
      good_clipping(clipping),
      at_top(top),
      load_completed(false),
      time_at_snapshot(Time::Now()),
      redirect_hops_from_dest(0) {
}

ThumbnailScore::ThumbnailScore(double score,
                               bool clipping,
                               bool top,
                               const Time& time)
    : boring_score(score),
      good_clipping(clipping),
      at_top(top),
      load_completed(false),
      time_at_snapshot(time),
      redirect_hops_from_dest(0) {
}

ThumbnailScore::~ThumbnailScore() {
}

bool ThumbnailScore::Equals(const ThumbnailScore& rhs) const {
  return boring_score == rhs.boring_score &&
         good_clipping == rhs.good_clipping && at_top == rhs.at_top &&
         time_at_snapshot == rhs.time_at_snapshot &&
         redirect_hops_from_dest == rhs.redirect_hops_from_dest;
}

std::string ThumbnailScore::ToString() const {
  return base::StringPrintf(
      "boring_score: %f, at_top %d, good_clipping %d, "
      "load_completed: %d, "
      "time_at_snapshot: %f, redirect_hops_from_dest: %d",
      boring_score,
      at_top,
      good_clipping,
      load_completed,
      time_at_snapshot.ToDoubleT(),
      redirect_hops_from_dest);
}

bool ShouldReplaceThumbnailWith(const ThumbnailScore& current,
                                const ThumbnailScore& replacement) {
  int current_type = GetThumbnailType(current);
  int replacement_type = GetThumbnailType(replacement);
  if (replacement_type < current_type) {
    // If we have a better class of thumbnail, add it if it meets
    // certain minimum boringness.
    return replacement.boring_score <
           ThumbnailScore::kThumbnailMaximumBoringness;
  } else if (replacement_type == current_type) {
    // It's much easier to do the scaling below when we're dealing with "higher
    // is better." Then we can decrease the score by dividing by a fraction.
    const double kThumbnailMinimumInterestingness =
        1.0 - ThumbnailScore::kThumbnailMaximumBoringness;
    double current_interesting_score = 1.0 - current.boring_score;
    double replacement_interesting_score = 1.0 - replacement.boring_score;

    // Degrade the score of each thumbnail to account for how many redirects
    // they are away from the destination. 1/(x+1) gives a scaling factor of
    // one for x = 0, and asymptotically approaches 0 for larger values of x.
    current_interesting_score *= 1.0 / (current.redirect_hops_from_dest + 1);
    replacement_interesting_score *=
        1.0 / (replacement.redirect_hops_from_dest + 1);

    // Degrade the score and prefer the newer one based on how long apart the
    // two thumbnails were taken. This means we'll eventually replace an old
    // good one with a new worse one assuming enough time has passed.
    base::TimeDelta time_between_thumbnails =
        replacement.time_at_snapshot - current.time_at_snapshot;
    current_interesting_score -= time_between_thumbnails.InHours() *
                                 ThumbnailScore::kThumbnailDegradePerHour;

    if (current_interesting_score < kThumbnailMinimumInterestingness)
      current_interesting_score = kThumbnailMinimumInterestingness;
    if (replacement_interesting_score > current_interesting_score)
      return true;
  }

  // If the current thumbnail doesn't meet basic boringness
  // requirements, but the replacement does, always replace the
  // current one even if we're using a worse thumbnail type.
  return current.boring_score >= ThumbnailScore::kThumbnailMaximumBoringness &&
         replacement.boring_score < ThumbnailScore::kThumbnailMaximumBoringness;
}

bool ThumbnailScore::ShouldConsiderUpdating() {
  const base::TimeDelta time_elapsed = Time::Now() - time_at_snapshot;
  if (time_elapsed < kUpdateThumbnailTime && good_clipping && at_top &&
      load_completed) {
    // The current thumbnail is new and has good properties.
    return false;
  }
  // The current thumbnail should be updated.
  return true;
}

}  // namespace history
