// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_timestamp_estimator.h"

#include <algorithm>

namespace speech {

namespace {

using MediaTimestamp = SpeechTimestampEstimator::MediaTimestamp;
using SpeechTimestamp = SpeechTimestampEstimator::SpeechTimestamp;
using PlaybackDuration = SpeechTimestampEstimator::PlaybackDuration;
using MediaRanges = SpeechTimestampEstimator::MediaRanges;
using PlaybackChunk = SpeechTimestampEstimator::PlaybackChunk;

PlaybackDuration CalculateDuration(SpeechTimestamp start, SpeechTimestamp end) {
  CHECK_LT(start, end);
  return PlaybackDuration(end.value() - start.value());
}

MediaTimestamp IncreaseTimestamp(MediaTimestamp timestamp,
                                 PlaybackDuration duration) {
  CHECK(duration->is_positive());
  return MediaTimestamp(timestamp.value() + duration.value());
}

}  // namespace

SpeechTimestampEstimator::SpeechTimestampEstimator() = default;

SpeechTimestampEstimator::~SpeechTimestampEstimator() = default;

SpeechTimestampEstimator::PlaybackChunk::PlaybackChunk(
    MediaTimestamp media_start,
    SpeechTimestamp current_speech_time)
    : media_start(media_start), speech_start(current_speech_time) {}

void SpeechTimestampEstimator::PlaybackChunk::TrimStart(
    PlaybackDuration duration) {
  CHECK_LE(duration, playback_duration);
  *media_start += duration.value();
  *speech_start += duration.value();

  *playback_duration -= duration.value();
  // Note: `playback_duration` might 0 at this point, which is allowed.
}

void SpeechTimestampEstimator::PlaybackChunk::AddDuration(
    PlaybackDuration duration) {
  *playback_duration += duration.value();
}

MediaTimestamp SpeechTimestampEstimator::PlaybackChunk::MediaEnd() const {
  return MediaTimestamp(media_start.value() + playback_duration.value());
}

SpeechTimestamp SpeechTimestampEstimator::PlaybackChunk::SpeechEnd() const {
  return SpeechTimestamp(speech_start.value() + playback_duration.value());
}

void SpeechTimestampEstimator::AddPlaybackStart(
    MediaTimestamp media_start_pts) {
  playback_chunks_.emplace_back(media_start_pts, current_speech_time_);

  // Silence is only saved to estimate the current media timestamp. A new
  // playback should "drop" the running silence, since no audio was sent to the
  // speech service during that time.
  running_silence_duration_.reset();
}

void SpeechTimestampEstimator::AppendDuration(PlaybackDuration duration) {
  CHECK(duration->is_positive());

  // Adjust for silences before moving the current speech time.
  if (running_silence_duration_) {
    AdjustLastMediaTimestampForSilence(current_speech_time_);
  }

  *current_speech_time_ += duration.value();

  if (playback_chunks_.empty()) {
    return;
  }

  playback_chunks_.back().AddDuration(duration);
}

void SpeechTimestampEstimator::OnSilentMediaDropped(PlaybackDuration duration) {
  CHECK(duration->is_positive());

  // We've never received a media timestamp to adjust... No need to keep track
  // of silence.
  if (playback_chunks_.empty()) {
    return;
  }

  if (!running_silence_duration_) {
    running_silence_duration_ = duration;
    return;
  }

  *(running_silence_duration_.value()) += duration.value();
}

void SpeechTimestampEstimator::AdjustLastMediaTimestampForSilence(
    SpeechTimestamp current_speech_time) {
  CHECK(!playback_chunks_.empty());
  CHECK(running_silence_duration_.has_value());

  // If this method is called, we have received audible frames after a period of
  // silence. Calculate this first non-silent media timestamp from the last
  // non-silent media timestamp and elapsed silence duration.
  MediaTimestamp first_audible_timestamp = IncreaseTimestamp(
      playback_chunks_.back().MediaEnd(), running_silence_duration_.value());

  // "Forward" the last media timestamp by `running_silence_duration_`.
  playback_chunks_.emplace_back(first_audible_timestamp, current_speech_time);

  running_silence_duration_.reset();
}

void SpeechTimestampEstimator::PopFrontUntil(
    base::circular_deque<PlaybackChunk>& chunks,
    SpeechTimestamp end_timestamp) {
  CHECK(!chunks.empty());
  CHECK_LE(end_timestamp, current_speech_time_);
  CHECK_EQ(chunks.back().SpeechEnd(), current_speech_time_);

  // Remove all chunks that have ended before `end_timestamp`.
  while (chunks.front().SpeechEnd() < end_timestamp) {
    chunks.pop_front();
  }

  // We should always have a leftover chunk, even if `end_timestamp` is equal to
  // `current_speech_time_`.
  CHECK(!chunks.empty());

  // Partially discard the front chunk until `end_timestamp` (exclusively),
  // if it starts before `end_timestamp`.
  PlaybackChunk& front_chunk = chunks.front();
  if (front_chunk.speech_start < end_timestamp) {
    PlaybackDuration duration =
        CalculateDuration(front_chunk.speech_start, end_timestamp);
    front_chunk.TrimStart(duration);
  }

  // We should always have a leftover chunk, potentially with 0 duration, since
  // we popped [0, `end_timestamp`), not [0, `end_timestamp`].
  CHECK(!chunks.empty());
}

std::vector<SpeechTimestampEstimator::PlaybackChunk>
SpeechTimestampEstimator::TakeFrontUntil(
    base::circular_deque<PlaybackChunk>& chunks,
    SpeechTimestamp end_timestamp) {
  CHECK(!chunks.empty());
  CHECK_LE(end_timestamp, current_speech_time_);
  CHECK_EQ(chunks.back().SpeechEnd(), current_speech_time_);

  std::vector<PlaybackChunk> results;

  // Take all complete chunks before `end_timestamp`.
  while (chunks.front().SpeechEnd() < end_timestamp) {
    // Don't save chunks with no duration.
    if (!chunks.front().playback_duration->is_zero()) {
      results.push_back(std::move(chunks.front()));
    }
    chunks.pop_front();
  }

  // We should always have a leftover chunk, even if `end_timestamp` is equal to
  // `current_speech_time_`.
  CHECK(!chunks.empty());

  // Split the front chunk at `end_timestamp`, and save the results before
  // `end_timestamp`.
  PlaybackChunk& front_chunk = chunks.front();
  if (front_chunk.speech_start < end_timestamp) {
    PlaybackDuration duration =
        CalculateDuration(front_chunk.speech_start, end_timestamp);

    PlaybackChunk front_copy = front_chunk;
    front_copy.playback_duration = duration;
    results.push_back(std::move(front_copy));

    front_chunk.TrimStart(duration);
  }

  // We should always have a leftover chunk, potentially with 0 duration, since
  // we took [0, `end_timestamp`), not [0, `end_timestamp`].
  CHECK(!chunks.empty());

  return results;
}

MediaRanges SpeechTimestampEstimator::TakeTimestampsInRange(
    SpeechTimestamp start,
    SpeechTimestamp end) {
  // Verify the timestamps and chunks.
  if (start >= end || playback_chunks_.empty()) {
    return MediaRanges();
  }

  // Clamp inputs.
  constexpr auto kSpeechTimeZero = SpeechTimestamp(base::Seconds(0));
  start = std::clamp(start, kSpeechTimeZero, current_speech_time_);
  end = std::clamp(end, kSpeechTimeZero, current_speech_time_);

  // Clamping values can collapse them to the same point.
  if (start == end) {
    return MediaRanges();
  }

  // Trim the front of the queue, from [0, `start`).
  PopFrontUntil(playback_chunks_, start);

  // `playback_chunks_` no longer contain any timestamps before `start`.
  // Take all playback chunks from [0, `end`), which is now [`start`, `end`).
  auto playbacks = TakeFrontUntil(playback_chunks_, end);

  // We should always have one chunk leftover (sometimes with 0 duration),
  // since we removed [0, `end`), not [0, `end`].
  CHECK(!playback_chunks_.empty());

  return ConvertToMediaRanges(playbacks);
}

MediaRanges SpeechTimestampEstimator::PeekTimestampsInRange(
    SpeechTimestamp start,
    SpeechTimestamp end) {
  // Verify the timestamps and chunks.
  if (start >= end || playback_chunks_.empty()) {
    return MediaRanges();
  }

  // Clamp inputs.
  constexpr auto kSpeechTimeZero = SpeechTimestamp(base::Seconds(0));
  start = std::clamp(start, kSpeechTimeZero, current_speech_time_);
  end = std::clamp(end, kSpeechTimeZero, current_speech_time_);

  // Clamping values can collapse them to the same point.
  if (start == end) {
    return MediaRanges();
  }

  // Create a copy of the chunks to avoid modifying the original state.
  auto chunks_copy = playback_chunks_;

  // Operate on the copy.
  PopFrontUntil(chunks_copy, start);
  auto playbacks = TakeFrontUntil(chunks_copy, end);

  return ConvertToMediaRanges(playbacks);
}

MediaRanges SpeechTimestampEstimator::ConvertToMediaRanges(
    const std::vector<PlaybackChunk>& playbacks) {
  MediaRanges results;
  results.reserve(playbacks.size());

  // Convert playback chunks into media start/end presentation timestamps.
  std::ranges::transform(
      playbacks, std::back_inserter(results),
      [](const PlaybackChunk& chunk) -> media::MediaTimestampRange {
        return {.start = chunk.media_start.value(),
                .end = chunk.MediaEnd().value()};
      });

  return results;
}

}  // namespace speech
