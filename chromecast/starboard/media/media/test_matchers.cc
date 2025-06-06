// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/test_matchers.h"

#include <cstdint>
#include <tuple>

namespace chromecast {
namespace media {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::FloatEq;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Pointwise;
using ::testing::StrEq;

// Takes an arg that can be converted to a span, and matches the first
// expected.size() values to expected.
MATCHER_P(SpanMatches, expected_span, "") {
  base::span<const uint8_t> actual_span = arg;
  if (actual_span.size() < expected_span.size()) {
    *result_listener << "too few elements (expected at least "
                     << expected_span.size() << ", got " << actual_span.size()
                     << ")";
    return false;
  }
  return ExplainMatchResult(ElementsAreArray(expected_span),
                            actual_span.first(expected_span.size()),
                            result_listener);
}

auto MatchesSubsampleMapping(const StarboardDrmSubSampleMapping& expected) {
  return AllOf(
      Field("clear_byte_count", &StarboardDrmSubSampleMapping::clear_byte_count,
            Eq(expected.clear_byte_count)),
      Field("encrypted_byte_count",
            &StarboardDrmSubSampleMapping::encrypted_byte_count,
            Eq(expected.encrypted_byte_count)));
}

// Used as an argument to Pointwise. Takes a tuple of
// StarboardDrmSubSampleMapping values and returns whether they are equal
// according to MatchesSubsampleMapping.
MATCHER(MatchesSubsampleMappingTuple, "") {
  const StarboardDrmSubSampleMapping left = std::get<0>(arg);
  const StarboardDrmSubSampleMapping right = std::get<1>(arg);
  return ExplainMatchResult(MatchesSubsampleMapping(right), left,
                            result_listener);
}

auto MatchesSideData(const StarboardSampleSideData& expected) {
  return AllOf(Field("type", &StarboardSampleSideData::type, Eq(expected.type)),
               Field("data", &StarboardSampleSideData::data,
                     ElementsAreArray(expected.data)));
}

// Used as an argument to Pointwise. Takes a tuple of StarboardSampleSideData
// values and returns whether they are equal according to MatchesSideData.
MATCHER(MatchesSideDataTuple, "") {
  const StarboardSampleSideData left = std::get<0>(arg);
  const StarboardSampleSideData right = std::get<1>(arg);
  return left.type == right.type &&
         ExplainMatchResult(MatchesSideData(right), left, result_listener);
}

auto MatchesMasteringMetadata(const StarboardMediaMasteringMetadata& expected) {
  return AllOf(
      Field("primary_r_chromaticity_x",
            &StarboardMediaMasteringMetadata::primary_r_chromaticity_x,
            FloatEq(expected.primary_r_chromaticity_x)),
      Field("primary_r_chromaticity_y",
            &StarboardMediaMasteringMetadata::primary_r_chromaticity_y,
            FloatEq(expected.primary_r_chromaticity_y)),
      Field("primary_g_chromaticity_x",
            &StarboardMediaMasteringMetadata::primary_g_chromaticity_x,
            FloatEq(expected.primary_g_chromaticity_x)),
      Field("primary_g_chromaticity_y",
            &StarboardMediaMasteringMetadata::primary_g_chromaticity_y,
            FloatEq(expected.primary_g_chromaticity_y)),
      Field("primary_b_chromaticity_x",
            &StarboardMediaMasteringMetadata::primary_b_chromaticity_x,
            FloatEq(expected.primary_b_chromaticity_x)),
      Field("primary_b_chromaticity_y",
            &StarboardMediaMasteringMetadata::primary_b_chromaticity_y,
            FloatEq(expected.primary_b_chromaticity_y)),
      Field("white_point_chromaticity_x",
            &StarboardMediaMasteringMetadata::white_point_chromaticity_x,
            FloatEq(expected.white_point_chromaticity_x)),
      Field("white_point_chromaticity_y",
            &StarboardMediaMasteringMetadata::white_point_chromaticity_y,
            FloatEq(expected.white_point_chromaticity_y)),
      Field("luminance_max", &StarboardMediaMasteringMetadata::luminance_max,
            FloatEq(expected.luminance_max)),
      Field("luminance_min", &StarboardMediaMasteringMetadata::luminance_min,
            FloatEq(expected.luminance_min)));
}

auto MatchesColorMetadata(const StarboardColorMetadata& expected) {
  return AllOf(
      Field("bits_per_channel", &StarboardColorMetadata::bits_per_channel,
            Eq(expected.bits_per_channel)),
      Field("chroma_subsampling_horizontal",
            &StarboardColorMetadata::chroma_subsampling_horizontal,
            Eq(expected.chroma_subsampling_horizontal)),
      Field("chroma_subsampling_vertical",
            &StarboardColorMetadata::chroma_subsampling_vertical,
            Eq(expected.chroma_subsampling_vertical)),
      Field("cb_subsampling_horizontal",
            &StarboardColorMetadata::cb_subsampling_horizontal,
            Eq(expected.cb_subsampling_horizontal)),
      Field("cb_subsampling_vertical",
            &StarboardColorMetadata::cb_subsampling_vertical,
            Eq(expected.cb_subsampling_vertical)),
      Field("chroma_siting_horizontal",
            &StarboardColorMetadata::chroma_siting_horizontal,
            Eq(expected.chroma_siting_horizontal)),
      Field("chroma_siting_vertical",
            &StarboardColorMetadata::chroma_siting_vertical,
            Eq(expected.chroma_siting_vertical)),
      Field("mastering_metadata", &StarboardColorMetadata::mastering_metadata,
            MatchesMasteringMetadata(expected.mastering_metadata)),
      Field("max_cll", &StarboardColorMetadata::max_cll, Eq(expected.max_cll)),
      Field("max_fall", &StarboardColorMetadata::max_fall,
            Eq(expected.max_fall)),
      Field("primaries", &StarboardColorMetadata::primaries,
            Eq(expected.primaries)),
      Field("transfer", &StarboardColorMetadata::transfer,
            Eq(expected.transfer)),
      Field("matrix", &StarboardColorMetadata::matrix, Eq(expected.matrix)),
      Field("range", &StarboardColorMetadata::range, Eq(expected.range)),
      Field("custom_primary_matrix",
            &StarboardColorMetadata::custom_primary_matrix,
            Pointwise(FloatEq(), expected.custom_primary_matrix)));
}

}  // namespace

Matcher<StarboardSampleInfo> MatchesStarboardSampleInfo(
    const StarboardSampleInfo& expected) {
  auto common_checks =
      AllOf(Field("type", &StarboardSampleInfo::type, Eq(expected.type)),
            Field("buffer", &StarboardSampleInfo::buffer, Eq(expected.buffer)),
            Field("buffer_size", &StarboardSampleInfo::buffer_size,
                  Eq(expected.buffer_size)),
            Field("timestamp", &StarboardSampleInfo::timestamp,
                  Eq(expected.timestamp)),
            Field("side_data", &StarboardSampleInfo::side_data,
                  Pointwise(MatchesSideDataTuple(), expected.side_data)),
            expected.drm_info == nullptr
                ? Field("drm_info", &StarboardSampleInfo::drm_info, IsNull())
                : Field("drm_info", &StarboardSampleInfo::drm_info,
                        Pointee(MatchesDrmInfo(*expected.drm_info))));

  // Note that AllOf short circuits, so if there is a mismatch on `type` we
  // avoid reading the audio_sample_info/video_sample_info union of the object
  // being matched.
  if (expected.type == 0) {
    // Audio sample.
    return AllOf(
        common_checks,
        Field("audio_sample_info", &StarboardSampleInfo::audio_sample_info,
              MatchesAudioSampleInfo(expected.audio_sample_info)));
  } else {
    // Video sample.
    return AllOf(
        common_checks,
        Field("video_sample_info", &StarboardSampleInfo::video_sample_info,
              MatchesVideoSampleInfo(expected.video_sample_info)));
  }
}

Matcher<StarboardAudioSampleInfo> MatchesAudioSampleInfo(
    const StarboardAudioSampleInfo& expected) {
  return AllOf(
      Field("codec", &StarboardAudioSampleInfo::codec, Eq(expected.codec)),
      expected.mime == nullptr
          ? Field("mime", &StarboardAudioSampleInfo::mime, IsNull())
          : Field("mime", &StarboardAudioSampleInfo::mime,
                  StrEq(expected.mime)),
      Field("format_tag", &StarboardAudioSampleInfo::format_tag,
            Eq(expected.format_tag)),
      Field("number_of_channels", &StarboardAudioSampleInfo::number_of_channels,
            Eq(expected.number_of_channels)),
      Field("samples_per_second", &StarboardAudioSampleInfo::samples_per_second,
            Eq(expected.samples_per_second)),
      Field("average_bytes_per_second",
            &StarboardAudioSampleInfo::average_bytes_per_second,
            Eq(expected.average_bytes_per_second)),
      Field("block_alignment", &StarboardAudioSampleInfo::block_alignment,
            Eq(expected.block_alignment)),
      Field("bits_per_sample", &StarboardAudioSampleInfo::bits_per_sample,
            Eq(expected.bits_per_sample)),
      Field("audio_specific_config",
            &StarboardAudioSampleInfo::audio_specific_config,
            Eq(expected.audio_specific_config)),
      Field("audio_specific_config",
            &StarboardAudioSampleInfo::audio_specific_config,
            Eq(expected.audio_specific_config)));
}

Matcher<StarboardVideoSampleInfo> MatchesVideoSampleInfo(
    const StarboardVideoSampleInfo& expected) {
  return AllOf(
      Field("codec", &StarboardVideoSampleInfo::codec, Eq(expected.codec)),
      expected.mime == nullptr
          ? Field("mime", &StarboardVideoSampleInfo::mime, IsNull())
          : Field("mime", &StarboardVideoSampleInfo::mime,
                  StrEq(expected.mime)),
      expected.max_video_capabilities == nullptr
          ? Field("max_video_capabilities",
                  &StarboardVideoSampleInfo::max_video_capabilities, IsNull())
          : Field("max_video_capabilities",
                  &StarboardVideoSampleInfo::max_video_capabilities,
                  StrEq(expected.max_video_capabilities)),
      Field("is_key_frame", &StarboardVideoSampleInfo::is_key_frame,
            Eq(expected.is_key_frame)),
      Field("frame_width", &StarboardVideoSampleInfo::frame_width,
            Eq(expected.frame_width)),
      Field("frame_height", &StarboardVideoSampleInfo::frame_height,
            Eq(expected.frame_height)),
      Field("color_metadata", &StarboardVideoSampleInfo::color_metadata,
            MatchesColorMetadata(expected.color_metadata)));
}

Matcher<StarboardDrmSampleInfo> MatchesDrmInfo(
    const StarboardDrmSampleInfo& expected) {
  return AllOf(
      Field("encryption_scheme", &StarboardDrmSampleInfo::encryption_scheme,
            Eq(expected.encryption_scheme)),
      Field("encryption_pattern", &StarboardDrmSampleInfo::encryption_pattern,
            AllOf(Field("crypt_byte_block",
                        &StarboardDrmEncryptionPattern::crypt_byte_block,
                        Eq(expected.encryption_pattern.crypt_byte_block)),
                  Field("skip_byte_block",
                        &StarboardDrmEncryptionPattern::skip_byte_block,
                        Eq(expected.encryption_pattern.skip_byte_block)))),
      Field(
          "initialization_vector",
          &StarboardDrmSampleInfo::initialization_vector,
          SpanMatches(base::span<const uint8_t>(expected.initialization_vector)
                          .first(static_cast<size_t>(
                              expected.initialization_vector_size)))),
      Field("initialization_vector_size",
            &StarboardDrmSampleInfo::initialization_vector_size,
            Eq(expected.initialization_vector_size)),
      Field("identifier", &StarboardDrmSampleInfo::identifier,
            SpanMatches(
                base::span<const uint8_t>(expected.identifier)
                    .first(static_cast<size_t>(expected.identifier_size)))),
      Field("identifier_size", &StarboardDrmSampleInfo::identifier_size,
            Eq(expected.identifier_size)),
      Field("subsample_mapping", &StarboardDrmSampleInfo::subsample_mapping,
            Pointwise(MatchesSubsampleMappingTuple(),
                      expected.subsample_mapping)));
}

Matcher<StarboardPlayerCreationParam> MatchesPlayerCreationParam(
    const StarboardPlayerCreationParam& expected) {
  return AllOf(Field("drm_system", &StarboardPlayerCreationParam::drm_system,
                     Eq(expected.drm_system)),
               Field("audio_sample_info",
                     &StarboardPlayerCreationParam::audio_sample_info,
                     MatchesAudioSampleInfo(expected.audio_sample_info)),
               Field("video_sample_info",
                     &StarboardPlayerCreationParam::video_sample_info,
                     MatchesVideoSampleInfo(expected.video_sample_info)),
               Field("output_mode", &StarboardPlayerCreationParam::output_mode,
                     Eq(expected.output_mode)));
}

}  // namespace media
}  // namespace chromecast
