// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The structs/enums in this file represent corresponding structs/enums in
// starboard. By using these types instead of Starboard's, we can abstract away
// minor changes (e.g. function renames) in Starboard without having to change
// the rest of the MediaPipelineBackend. Additionally, these types can be
// converted to different version-specific Starboard types.
//
// Users of this header should interact with Starboard via a StarboardApiWrapper
// created via GetStarboardApiWrapper(). For testing purposes, a mock
// StarboardApiWrapper can be used instead.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_API_WRAPPER_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_API_WRAPPER_H_

#include <cstdint>
#include <cstring>
#include <memory>

namespace chromecast {
namespace media {

// Copy of SbMediaSupportType from starboard.
enum StarboardMediaSupportType {
  kStarboardMediaSupportTypeNotSupported,
  kStarboardMediaSupportTypeMaybe,
  kStarboardMediaSupportTypeProbably,
};

// Copy of SbMediaType from starboard.
enum StarboardMediaType {
  kStarboardMediaTypeAudio,
  kStarboardMediaTypeVideo,
};

// Copy of SbMediaAudioCodec from starboard.
enum StarboardAudioCodec {
  kStarboardAudioCodecNone,
  kStarboardAudioCodecAac,
  kStarboardAudioCodecAc3,
  kStarboardAudioCodecEac3,
  kStarboardAudioCodecOpus,
  kStarboardAudioCodecVorbis,
  kStarboardAudioCodecMp3,
  kStarboardAudioCodecFlac,
  kStarboardAudioCodecPcm,
};

// Copy of SbMediaVideoCodec from starboard.
enum StarboardVideoCodec {
  kStarboardVideoCodecNone,
  kStarboardVideoCodecH264,
  kStarboardVideoCodecH265,
  kStarboardVideoCodecMpeg2,
  kStarboardVideoCodecTheora,
  kStarboardVideoCodecVc1,
  kStarboardVideoCodecAv1,
  kStarboardVideoCodecVp8,
  kStarboardVideoCodecVp9,
};

// Copy of SbPlayerSampleSideDataType from starboard.
enum StarboardSampleSideDataType {
  kStarboardSampleSideDataTypeMatroskaBlockAdditional,
};

// Copy of SbPlayerDecoderState from starboard.
enum StarboardDecoderState {
  kStarboardDecoderStateNeedsData,
};

// Copy of SbPlayerOutputMode from starboard.
enum StarboardPlayerOutputMode {
  kStarboardPlayerOutputModeDecodeToTexture,
  kStarboardPlayerOutputModePunchOut,
  kStarboardPlayerOutputModeInvalid,
};

// Copy of SbPlayerState from starboard.
enum StarboardPlayerState {
  kStarboardPlayerStateInitialized,
  kStarboardPlayerStatePrerolling,
  kStarboardPlayerStatePresenting,
  kStarboardPlayerStateEndOfStream,
  kStarboardPlayerStateDestroyed,
};

// Copy of SbPlayerError from starboard.
enum StarboardPlayerError {
  kStarboardPlayerErrorDecode,
  kStarboardPlayerErrorCapabilityChanged,
  kStarboardPlayerErrorMax,
};

// Copy of SbDrmEncryptionScheme from starboard.
enum StarboardDrmEncryptionScheme {
  kStarboardDrmEncryptionSchemeAesCtr,
  kStarboardDrmEncryptionSchemeAesCbc,
};

// Copy of SbDrmStatus from starboard.
enum StarboardDrmStatus {
  kStarboardDrmStatusSuccess,
  kStarboardDrmStatusTypeError,
  kStarboardDrmStatusNotSupportedError,
  kStarboardDrmStatusInvalidStateError,
  kStarboardDrmStatusQuotaExceededError,
  kStarboardDrmStatusUnknownError,
};

// Copy of SbDrmSessionRequestType from starboard.
enum StarboardDrmSessionRequestType {
  kStarboardDrmSessionRequestTypeLicenseRequest,
  kStarboardDrmSessionRequestTypeLicenseRenewal,
  kStarboardDrmSessionRequestTypeLicenseRelease,
  kStarboardDrmSessionRequestTypeIndividualizationRequest,
};

// Copy of SbDrmKeyStatus from starboard.
enum StarboardDrmKeyStatus {
  kStarboardDrmKeyStatusUsable,
  kStarboardDrmKeyStatusExpired,
  kStarboardDrmKeyStatusReleased,
  kStarboardDrmKeyStatusRestricted,
  kStarboardDrmKeyStatusDownscaled,
  kStarboardDrmKeyStatusPending,
  kStarboardDrmKeyStatusError,
};

// Copy of SbMediaMasteringMetadata from starboard.
struct StarboardMediaMasteringMetadata {
  float primary_r_chromaticity_x;
  float primary_r_chromaticity_y;
  float primary_g_chromaticity_x;
  float primary_g_chromaticity_y;
  float primary_b_chromaticity_x;
  float primary_b_chromaticity_y;
  float white_point_chromaticity_x;
  float white_point_chromaticity_y;
  float luminance_max;
  float luminance_min;
};

// Copy of SbMediaMasteringMetadata from starboard.
struct StarboardColorMetadata {
  unsigned int bits_per_channel;
  unsigned int chroma_subsampling_horizontal;
  unsigned int chroma_subsampling_vertical;
  unsigned int cb_subsampling_horizontal;
  unsigned int cb_subsampling_vertical;
  unsigned int chroma_siting_horizontal;
  unsigned int chroma_siting_vertical;
  StarboardMediaMasteringMetadata mastering_metadata;
  unsigned int max_cll;
  unsigned int max_fall;
  // See SbMediaPrimaryId from starboard.
  int primaries;
  // See SbMediaTransferId from starboard.
  int transfer;
  // See SbMediaMatrixId from starboard.
  int matrix;
  // See SbMediaRangeId from starboard.
  int range;
  float custom_primary_matrix[12];
};

// Copy of SbMediaAudioSampleInfo from starboard.
struct StarboardAudioSampleInfo {
  StarboardAudioCodec codec;
  const char* mime;
  uint16_t format_tag;
  uint16_t number_of_channels;
  uint32_t samples_per_second;
  uint32_t average_bytes_per_second;
  uint16_t block_alignment;
  uint16_t bits_per_sample;
  uint16_t audio_specific_config_size;
  const void* audio_specific_config;
};

// Copy of SbMediaVideoSampleInfo from starboard.
struct StarboardVideoSampleInfo {
  StarboardVideoCodec codec;
  const char* mime;
  const char* max_video_capabilities;
  bool is_key_frame;
  int frame_width;
  int frame_height;
  StarboardColorMetadata color_metadata;
};

// Copy of SbPlayerSampleSideData from starboard.
struct StarboardSampleSideData {
  StarboardSampleSideDataType type;
  const uint8_t* data;
  size_t size;
};

// Copy of SbDrmEncryptionPattern from starboard.
struct StarboardDrmEncryptionPattern {
  uint32_t crypt_byte_block;
  uint32_t skip_byte_block;
};

// Copy of SbDrmSubSampleMapping from starboard.
struct StarboardDrmSubSampleMapping {
  // How many bytes of the corresponding subsample are not encrypted
  int32_t clear_byte_count;

  // How many bytes of the corresponding subsample are encrypted.
  int32_t encrypted_byte_count;
};

// Copy of SbDrmSampleInfo from starboard.
struct StarboardDrmSampleInfo {
  StarboardDrmEncryptionScheme encryption_scheme;

  // The encryption pattern of this sample.
  StarboardDrmEncryptionPattern encryption_pattern;

  // The Initialization Vector needed to decrypt this sample.
  uint8_t initialization_vector[16];
  int initialization_vector_size;

  // The ID of the license (or key) required to decrypt this sample. For
  // PlayReady, this is the license GUID in packed little-endian binary form.
  uint8_t identifier[16];
  int identifier_size;

  // The number of subsamples in this sample, must be at least 1.
  int32_t subsample_count;

  // The clear/encrypted mapping of each subsample in this sample. This must be
  // an array of |subsample_count| mappings.
  const StarboardDrmSubSampleMapping* subsample_mapping;
};

// Copy of SbPlayerSampleInfo from starboard.
struct StarboardSampleInfo {
  // See SbMediaType.
  int type;
  // Points to the buffer containing the sample data.
  const void* buffer;
  // Size of the data pointed to by |buffer|.
  int buffer_size;
  // The timestamp of the sample.
  int64_t timestamp;
  // Points to an array of side data for the input, when available.
  StarboardSampleSideData* side_data;
  // The number of side data pointed by |side_data|.  It should be set to 0 if
  // there is no side data for the input.
  int side_data_count;
  union {
    // Information about an audio sample. This value can only be used when
    // |type| is kSbMediaTypeAudio.
    StarboardAudioSampleInfo audio_sample_info;
    // Information about a video sample. This value can only be used when |type|
    // is kSbMediaTypeVideo.
    StarboardVideoSampleInfo video_sample_info;
  };
  // The DRM system related info for the media sample. This value is required
  // for encrypted samples. Otherwise, it must be |NULL|.
  const StarboardDrmSampleInfo* drm_info;
};

// Copy of SbPlayerCreationParam from starboard.
struct StarboardPlayerCreationParam {
  // Note: a DRM system is not included in this struct. Due to the architecture
  // of cast, the MediaPipelineBackend does not have direct access to the CDM.
  // So instead we pass a global to Starboard (in starboard_media_api.cc).

  // Contains a populated SbMediaAudioSampleInfo if |audio_sample_info.codec|
  // isn't |kSbMediaAudioCodecNone|.  When |audio_sample_info.codec| is
  // |kSbMediaAudioCodecNone|, the video doesn't have an audio track.
  StarboardAudioSampleInfo audio_sample_info;

  // Contains a populated SbMediaVideoSampleInfo if |video_sample_info.codec|
  // isn't |kSbMediaVideoCodecNone|.  When |video_sample_info.codec| is
  // |kSbMediaVideoCodecNone|, the video is audio only.
  StarboardVideoSampleInfo video_sample_info;

  // Selects how the decoded video frames will be output.  For example,
  // |kSbPlayerOutputModePunchOut| indicates that the decoded video frames will
  // be output to a background video layer by the platform, and
  // |kSbPlayerOutputDecodeToTexture| indicates that the decoded video frames
  // should be made available for the application to pull via calls to
  // SbPlayerGetCurrentFrame().
  StarboardPlayerOutputMode output_mode;
};

// Copy of SbPlayerInfo2 from starboard.
struct StarboardPlayerInfo {
  // The position of the playback head, as precisely as possible, in
  // microseconds.
  int64_t current_media_timestamp_micros;
  // The known duration of the currently playing media stream, in microseconds.
  int64_t duration_micros;
  // The result of getStartDate for the currently playing media stream, in
  // microseconds since the epoch of January 1, 1601 UTC.
  int64_t start_date;
  // The width of the currently displayed frame, in pixels, or 0 if not provided
  // by this player.
  int frame_width;
  // The height of the currently displayed frame, in pixels, or 0 if not
  // provided by this player.
  int frame_height;
  // Whether playback is currently paused.
  bool is_paused;
  // The current player volume in [0, 1].
  double volume;
  // The number of video frames sent to the player since the creation of the
  // player.
  int total_video_frames;
  // The number of video frames decoded but not displayed since the creation of
  // the player.
  int dropped_video_frames;
  // The number of video frames that failed to be decoded since the creation of
  // the player.
  int corrupted_video_frames;
  // The rate of playback. The video is played back in a speed that is
  // proportional to this. By default it is 1.0 which indicates that the
  // playback is at normal speed. When it is greater than one, the video is
  // played in a faster than normal speed. When it is less than one, the video
  // is played in a slower than normal speed. Negative speeds are not supported.
  double playback_rate;
};

// Copy of SbDrmKeyId from starboard.
struct StarboardDrmKeyId {
  uint8_t identifier[16];
  int identifier_size;
};

// Copy of SbPlayerDecoderStatusFunc from starboard.
using StarboardPlayerDecoderStatusFunc =
    void (*)(void* player,
             void* context,
             StarboardMediaType type,
             StarboardDecoderState decoder_state,
             int ticket);

// Copy of SbPlayerDeallocateSampleFunc from starboard.
using StarboardPlayerDeallocateSampleFunc = void (*)(void* player,
                                                     void* context,
                                                     const void* sample_buffer);

// Copy of SbPlayerStatusFunc from starboard.
using StarboardPlayerStatusFunc = void (*)(void* player,
                                           void* context,
                                           StarboardPlayerState state,
                                           int ticket);

// Copy of SbPlayerErrorFunc from starboard.
using StarboardPlayerErrorFunc = void (*)(void* player,
                                          void* context,
                                          StarboardPlayerError error,
                                          const char* message);

// Copy of SbDrmSessionUpdateRequestFunc from starboard.
using StarboardDrmSessionUpdateRequestFunc =
    void (*)(void* drm_system,
             void* context,
             int ticket,
             StarboardDrmStatus status,
             StarboardDrmSessionRequestType type,
             const char* error_message,
             const void* session_id,
             int session_id_size,
             const void* content,
             int content_size,
             const char* url);

// Copy of SbDrmSessionUpdatedFunc from starboard.
using StarboardDrmSessionUpdatedFunc = void (*)(void* drm_system,
                                                void* context,
                                                int ticket,
                                                StarboardDrmStatus status,
                                                const char* error_message,
                                                const void* session_id,
                                                int session_id_size);

// Copy of SbDrmSessionKeyStatusesChangedFunc from starboard.
using StarboardDrmSessionKeyStatusesChangedFunc =
    void (*)(void* drm_system,
             void* context,
             const void* session_id,
             int session_id_size,
             int number_of_keys,
             const StarboardDrmKeyId* key_ids,
             const StarboardDrmKeyStatus* key_statuses);

// Copy of SbDrmServerCertificateUpdatedFunc from starboard.
using StarboardDrmServerCertificateUpdatedFunc =
    void (*)(void* drm_system,
             void* context,
             int ticket,
             StarboardDrmStatus status,
             const char* error_message);

// Copy of SbDrmSessionClosedFunc from starboard.
using StarboardDrmSessionClosedFunc = void (*)(void* drm_system,
                                               void* context,
                                               const void* session_id,
                                               int session_id_size);

// A wrapper for the player-related callbacks that starboard calls.
struct StarboardPlayerCallbackHandler {
  // The context that will be passed to all functions.
  void* context;
  StarboardPlayerDecoderStatusFunc decoder_status_fn;
  StarboardPlayerDeallocateSampleFunc deallocate_sample_fn;
  StarboardPlayerStatusFunc player_status_fn;
  StarboardPlayerErrorFunc player_error_fn;
};

// A wrapper for the DRM-related callbacks that starboard calls.
struct StarboardDrmSystemCallbackHandler {
  // The context that will be passed to all functions.
  void* context;
  StarboardDrmSessionUpdateRequestFunc update_request_fn;
  StarboardDrmSessionUpdatedFunc session_updated_fn;
  StarboardDrmSessionKeyStatusesChangedFunc key_statuses_changed_fn;
  StarboardDrmServerCertificateUpdatedFunc server_certificate_updated_fn;
  StarboardDrmSessionClosedFunc session_closed_fn;
};

// A wrapper around the Starboard API, allowing a mock to be used for testing.
// It also abstracts away details specific to certain versions of Starboard, by
// utilizing the structs defined above. Internally, those structs will be
// converted to the relevant starboard structs for the given starboard version.
class StarboardApiWrapper {
 public:
  virtual ~StarboardApiWrapper();

  // This function matches the EnsureInitialized function in
  // cast_starboard_api_adapter.h. It blocks until Starboard has started and
  // returns true, or returns false if initialization fails.
  //
  // This function is included in this wrapper so we can avoid calling the
  // production implementation in unit tests, thus removing a dependency on
  // Starboard.
  virtual bool EnsureInitialized() = 0;

  // Version-agnostic functions for SbPlayer. See starboard/player.h for more
  // info about the corresponding functions.
  //
  // StarboardPlayerCallbackHandler is used to wrap multiple callbacks that are
  // passed to starboard.
  virtual void* CreatePlayer(
      const StarboardPlayerCreationParam* creation_param,
      const StarboardPlayerCallbackHandler* callback_handler) = 0;
  virtual void SetPlayerBounds(void* player,
                               int z_index,
                               int x,
                               int y,
                               int width,
                               int height) = 0;
  virtual void SeekTo(void* player, int64_t time, int seek_ticket) = 0;
  virtual void WriteSample(void* player,
                           StarboardMediaType type,
                           StarboardSampleInfo* sample_infos,
                           int sample_infos_count) = 0;
  virtual void WriteEndOfStream(void* player, StarboardMediaType type) = 0;
  virtual void GetPlayerInfo(void* player,
                             StarboardPlayerInfo* player_info) = 0;
  virtual void SetVolume(void* player, double volume) = 0;
  virtual bool SetPlaybackRate(void* player, double playback_rate) = 0;
  virtual void DestroyPlayer(void* player) = 0;

  // Version-agnostic functions for SbDrmSystem. See starboard/drm.h for more
  // info about the corresponding functions.
  //
  // StarboardDrmSystemCallbackHandler is used to wrap multiple callbacks that
  // are passed to starboard.
  virtual void* CreateDrmSystem(
      const char* key_system,
      const StarboardDrmSystemCallbackHandler* callback_handler) = 0;
  virtual void DrmGenerateSessionUpdateRequest(
      void* drm_system,
      int ticket,
      const char* type,
      const void* initialization_data,
      int initialization_data_size) = 0;
  virtual void DrmUpdateSession(void* drm_system,
                                int ticket,
                                const void* key,
                                int key_size,
                                const void* session_id,
                                int session_id_size) = 0;
  virtual void DrmCloseSession(void* drm_system,
                               const void* session_id,
                               int session_id_size) = 0;
  virtual void DrmUpdateServerCertificate(void* drm_system,
                                          int ticket,
                                          const void* certificate,
                                          int certificate_size) = 0;
  virtual bool DrmIsServerCertificateUpdatable(void* drm_system) = 0;
  virtual void DrmDestroySystem(void* drm_system) = 0;

  // Version-agnostic functions for starboard/media.h.
  virtual StarboardMediaSupportType CanPlayMimeAndKeySystem(
      const char* mime,
      const char* key_system) = 0;
};

// Returns a StarboardApiWrapper that calls into libcast_starboard_api.so. This
// function is defined for each starboard version supported by cast. For
// example, there is a starboard 15 version that calls the starboard 15 APIs, a
// starboard 14 version that calls the starboard 14 APIs, etc.
std::unique_ptr<StarboardApiWrapper> GetStarboardApiWrapper();

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_API_WRAPPER_H_
