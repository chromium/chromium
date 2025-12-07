// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains matchers for starboard-related structs that can be used in tests.
//
// If more fine-grained matchers are needed in multiple tests, they can be moved
// from the .cc file to this header.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_TEST_MATCHERS_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_TEST_MATCHERS_H_

#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

// Returns a matcher that compares against a given StarboardSampleInfo. Note
// that the "buffer" field (a void*) is checked for equality by address. The
// individual elements are not compared. This was done because the lifetime of
// the buffer is important, so the actual address is usually relevant.
//
// Also note that when resampling occurs for PCM data, the address in "buffer"
// may not match the one provided by a mock (since a new buffer is created
// internally).
::testing::Matcher<StarboardSampleInfo> MatchesStarboardSampleInfo(
    const StarboardSampleInfo& expected);

// Returns a matcher that compares against a given StarboardAudioSampleInfo.
::testing::Matcher<StarboardAudioSampleInfo> MatchesAudioSampleInfo(
    const StarboardAudioSampleInfo& expected);

// Returns a matcher that compares against a given StarboardVideoSampleInfo.
::testing::Matcher<StarboardVideoSampleInfo> MatchesVideoSampleInfo(
    const StarboardVideoSampleInfo& expected);

// Returns a matcher that compares against a given StarboardDrmSampleInfo.
::testing::Matcher<StarboardDrmSampleInfo> MatchesDrmInfo(
    const StarboardDrmSampleInfo& expected);

// Returns a matcher that compares against a given StarboardPlayerCreationParam.
::testing::Matcher<StarboardPlayerCreationParam> MatchesPlayerCreationParam(
    const StarboardPlayerCreationParam& expected);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_TEST_MATCHERS_H_
