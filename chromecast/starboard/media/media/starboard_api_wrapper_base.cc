// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/starboard_api_wrapper_base.h"

#include <starboard/drm.h>
#include <starboard/media.h>
#include <starboard/player.h>

#include <cstring>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

namespace {

// Ensure that our internal starboard structs use arrays of the same size as the
// real starboard structs. Later in this code we'll just copy the entire array
// over (via span's copy_from_nonoverlapping()).
static_assert(sizeof(SbDrmKeyId::identifier) ==
              sizeof(StarboardDrmKeyId::identifier));
static_assert(sizeof(SbDrmSampleInfo::initialization_vector) ==
              sizeof(StarboardDrmSampleInfo::initialization_vector));
static_assert(sizeof(SbDrmSampleInfo::identifier) ==
              sizeof(StarboardDrmSampleInfo::identifier));

constexpr size_t kDrmKeyIdentifierSize = std::size(SbDrmKeyId{}.identifier);
constexpr size_t kDrmSampleInfoIvSize =
    std::size(SbDrmSampleInfo{}.initialization_vector);
constexpr size_t kDrmSampleInfoIdentifierSize =
    std::size(SbDrmSampleInfo{}.identifier);

// Helper function to convert a session ID to a string. Returns an empty string
// if session_id is null or the size is invalid (<=0).
std::string SessionIdToString(const void* session_id, int session_id_size) {
  if (session_id && session_id_size > 0) {
    return std::string(reinterpret_cast<const char*>(session_id),
                       session_id_size);
  }
  return "";
}

// Called by starboard when a sample can be safely deallocated.
void DeallocateSample(SbPlayer player,
                      void* context,
                      const void* sample_buffer) {
  const auto* handler =
      static_cast<const StarboardPlayerCallbackHandler*>(context);
  handler->deallocate_sample_fn(player, handler->context, sample_buffer);
}

// Called by starboard to notify cast of a decoder state change.
void OnDecoderStatus(SbPlayer player,
                     void* context,
                     SbMediaType type,
                     SbPlayerDecoderState state,
                     int ticket) {
  const auto* handler =
      static_cast<const StarboardPlayerCallbackHandler*>(context);
  handler->decoder_status_fn(player, handler->context,
                             static_cast<StarboardMediaType>(type),
                             static_cast<StarboardDecoderState>(state), ticket);
}

// Called by starboard to notify cast of a player state change.
void OnPlayerStatus(SbPlayer player,
                    void* context,
                    SbPlayerState state,
                    int ticket) {
  const auto* handler =
      static_cast<const StarboardPlayerCallbackHandler*>(context);
  handler->player_status_fn(player, handler->context,
                            static_cast<StarboardPlayerState>(state), ticket);
}

// Called by starboard to notify cast of a player error.
void OnPlayerError(SbPlayer player,
                   void* context,
                   SbPlayerError error,
                   const char* message) {
  const auto* handler =
      static_cast<const StarboardPlayerCallbackHandler*>(context);
  handler->player_error_fn(player, handler->context,
                           static_cast<StarboardPlayerError>(error),
                           std::string(message ? message : ""));
}

// Converts starboard's representation of a key ID to cast's version-agnostic
// representation.
StarboardDrmKeyId ToStarboardDrmKeyId(const SbDrmKeyId& in_key_id) {
  StarboardDrmKeyId out_key_id = {};

  static_assert(sizeof(in_key_id.identifier) == sizeof(out_key_id.identifier),
                "StarboardDrmKeyId.identifier and SbDrmKeyId.identifier must "
                "be arrays of the same size");

  base::span<uint8_t, kDrmKeyIdentifierSize>(out_key_id.identifier)
      .copy_from_nonoverlapping(in_key_id.identifier);
  out_key_id.identifier_size = in_key_id.identifier_size;

  return out_key_id;
}

// Called by starboard when a generated session update request is ready to be
// sent to cast.
void OnUpdateRequest(SbDrmSystem drm_system,
                     void* context,
                     int ticket,
                     SbDrmStatus status,
                     SbDrmSessionRequestType type,
                     const char* error_message,
                     const void* session_id,
                     int session_id_size,
                     const void* content,
                     int content_size,
                     const char* url) {
  const auto* handler =
      static_cast<const StarboardDrmSystemCallbackHandler*>(context);
  std::vector<uint8_t> content_vec;
  if (content && content_size > 0) {
    // SAFETY: This function is a callback defined in a C API (starboard):
    // https://github.com/youtube/cobalt/blob/31fef3564db8ecfe67fca0a7868c9bf44c14d151/starboard/drm.h#L150
    // We have to assume that the caller (Starboard) passed the correct value
    // for content_size.
    const uint8_t* content_ptr = reinterpret_cast<const uint8_t*>(content);
    UNSAFE_BUFFERS(content_vec.assign(content_ptr, content_ptr + content_size));
  }

  handler->update_request_fn(drm_system, handler->context, ticket,
                             static_cast<StarboardDrmStatus>(status),
                             static_cast<StarboardDrmSessionRequestType>(type),
                             std::string(error_message ? error_message : ""),
                             SessionIdToString(session_id, session_id_size),
                             std::move(content_vec),
                             std::string(url ? url : ""));
}

// Called by starboard to notify cast that a session has been updated.
void OnSessionUpdated(SbDrmSystem drm_system,
                      void* context,
                      int ticket,
                      SbDrmStatus status,
                      const char* error_message,
                      const void* session_id,
                      int session_id_size) {
  const auto* handler =
      static_cast<const StarboardDrmSystemCallbackHandler*>(context);
  handler->session_updated_fn(drm_system, handler->context, ticket,
                              static_cast<StarboardDrmStatus>(status),
                              std::string(error_message ? error_message : ""),
                              SessionIdToString(session_id, session_id_size));
}

// Called by starboard to notify cast that key statuses have changed.
//
// There is unsafe buffer usage here because the Starboard API passes us raw
// ptrs and an int representing the number of elements in those arrays.
UNSAFE_BUFFER_USAGE void OnKeyStatusesChanged(
    SbDrmSystem drm_system,
    void* context,
    const void* session_id,
    int session_id_size,
    int number_of_keys,
    const SbDrmKeyId* key_ids,
    const SbDrmKeyStatus* key_statuses) {
  std::vector<StarboardDrmKeyId> internal_key_ids;
  std::vector<StarboardDrmKeyStatus> internal_key_statuses;

  internal_key_ids.reserve(number_of_keys);
  internal_key_statuses.reserve(number_of_keys);

  for (int i = 0; i < number_of_keys; ++i) {
    // SAFETY: This function is a callback defined in a C API (starboard):
    // https://github.com/youtube/cobalt/blob/31fef3564db8ecfe67fca0a7868c9bf44c14d151/starboard/drm.h#L194
    // We have to assume that the caller (Starboard) passed the correct value
    // for number_of_keys.
    internal_key_ids.push_back(ToStarboardDrmKeyId(UNSAFE_BUFFERS(key_ids[i])));

    // SAFETY: see above
    internal_key_statuses.push_back(
        static_cast<StarboardDrmKeyStatus>(UNSAFE_BUFFERS(key_statuses[i])));
  }

  const auto* handler =
      static_cast<const StarboardDrmSystemCallbackHandler*>(context);
  handler->key_statuses_changed_fn(
      drm_system, handler->context,
      SessionIdToString(session_id, session_id_size),
      std::move(internal_key_ids), std::move(internal_key_statuses));
}

// Called by starboard when a server certificate has been updated.
void OnServerCertificateUpdated(SbDrmSystem drm_system,
                                void* context,
                                int ticket,
                                SbDrmStatus status,
                                const char* error_message) {
  const auto* handler =
      static_cast<const StarboardDrmSystemCallbackHandler*>(context);
  handler->server_certificate_updated_fn(
      drm_system, handler->context, ticket,
      static_cast<StarboardDrmStatus>(status),
      std::string(error_message ? error_message : ""));
}

// Called by starboard when a DRM session has closed.
void OnSessionClosed(SbDrmSystem drm_system,
                     void* context,
                     const void* session_id,
                     int session_id_size) {
  const auto* handler =
      static_cast<const StarboardDrmSystemCallbackHandler*>(context);

  handler->session_closed_fn(drm_system, handler->context,
                             SessionIdToString(session_id, session_id_size));
}

}  // namespace

StarboardApiWrapperBase::StarboardApiWrapperBase() = default;

StarboardApiWrapperBase::~StarboardApiWrapperBase() {
  if (initialized_) {
    initialized_ = false;
    chromecast::CastStarboardApiAdapter::GetInstance()->Unsubscribe(this);
  }
}

bool StarboardApiWrapperBase::EnsureInitialized() {
  // TODO: crbug.com/357265940 - The Starboard API is only explicitly subscriber
  // in one thread. Currently, CastMediaStarboard will use the Starboard API to
  // check MIME type compatibility in another process.
  if (!initialized_) {
    initialized_ = true;
    chromecast::CastStarboardApiAdapter::GetInstance()->Subscribe(this,
                                                                  nullptr);
  }

  return initialized_;
}

void* StarboardApiWrapperBase::CreatePlayer(
    const StarboardPlayerCreationParam* creation_param,
    const StarboardPlayerCallbackHandler* callback_handler) {
  SbWindow window =
      chromecast::CastStarboardApiAdapter::GetInstance()->GetWindow(nullptr);

  SbPlayerCreationParam sb_creation_param =
      ToSbPlayerCreationParam(*creation_param);

  return SbPlayerCreate(
      window, &sb_creation_param, &DeallocateSample, &OnDecoderStatus,
      &OnPlayerStatus, &OnPlayerError,
      /*context=*/
      static_cast<void*>(
          const_cast<StarboardPlayerCallbackHandler*>(callback_handler)),
      nullptr);
}

void StarboardApiWrapperBase::SetPlayerBounds(void* player,
                                              int z_index,
                                              int x,
                                              int y,
                                              int width,
                                              int height) {
  SbPlayerSetBounds(static_cast<SbPlayer>(player), z_index, x, y, width,
                    height);
}

void StarboardApiWrapperBase::WriteEndOfStream(void* player,
                                               StarboardMediaType type) {
  SbPlayerWriteEndOfStream(static_cast<SbPlayer>(player),
                           static_cast<SbMediaType>(type));
}

void StarboardApiWrapperBase::SetVolume(void* player, double volume) {
  SbPlayerSetVolume(static_cast<SbPlayer>(player), volume);
}

bool StarboardApiWrapperBase::SetPlaybackRate(void* player,
                                              double playback_rate) {
  return SbPlayerSetPlaybackRate(static_cast<SbPlayer>(player), playback_rate);
}

void StarboardApiWrapperBase::DestroyPlayer(void* player) {
  SbPlayerDestroy(static_cast<SbPlayer>(player));
}

void* StarboardApiWrapperBase::CreateDrmSystem(
    const char* key_system,
    const StarboardDrmSystemCallbackHandler* callback_handler) {
  LOG(INFO) << "Creating SbDrmSystem";
  return SbDrmCreateSystem(
      key_system,
      /*context=*/
      static_cast<void*>(
          const_cast<StarboardDrmSystemCallbackHandler*>(callback_handler)),
      &OnUpdateRequest, &OnSessionUpdated, &OnKeyStatusesChanged,
      &OnServerCertificateUpdated, &OnSessionClosed);
}

void StarboardApiWrapperBase::DrmGenerateSessionUpdateRequest(
    void* drm_system,
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  SbDrmGenerateSessionUpdateRequest(static_cast<SbDrmSystem>(drm_system),
                                    ticket, type, initialization_data,
                                    initialization_data_size);
}

void StarboardApiWrapperBase::DrmUpdateSession(void* drm_system,
                                               int ticket,
                                               const void* key,
                                               int key_size,
                                               const void* session_id,
                                               int session_id_size) {
  SbDrmUpdateSession(static_cast<SbDrmSystem>(drm_system), ticket, key,
                     key_size, session_id, session_id_size);
}

void StarboardApiWrapperBase::DrmCloseSession(void* drm_system,
                                              const void* session_id,
                                              int session_id_size) {
  SbDrmCloseSession(static_cast<SbDrmSystem>(drm_system), session_id,
                    session_id_size);
}

void StarboardApiWrapperBase::DrmUpdateServerCertificate(
    void* drm_system,
    int ticket,
    const void* certificate,
    int certificate_size) {
  SbDrmUpdateServerCertificate(static_cast<SbDrmSystem>(drm_system), ticket,
                               certificate, certificate_size);
}

bool StarboardApiWrapperBase::DrmIsServerCertificateUpdatable(
    void* drm_system) {
  return SbDrmIsServerCertificateUpdatable(
      static_cast<SbDrmSystem>(drm_system));
}

void StarboardApiWrapperBase::DrmDestroySystem(void* drm_system) {
  LOG(INFO) << "Destroying SbDrmSystem";
  SbDrmDestroySystem(static_cast<SbDrmSystem>(drm_system));
}

SbPlayerSampleInfo StarboardApiWrapperBase::ToSbPlayerSampleInfo(
    const StarboardSampleInfo& in_info,
    std::vector<SbPlayerSampleSideData>& side_data,
    SbDrmSampleInfo& drm_info,
    std::vector<SbDrmSubSampleMapping>& subsample_mappings) {
  SbPlayerSampleInfo out_info = {};
  out_info.type = static_cast<SbMediaType>(in_info.type);
  out_info.buffer = in_info.buffer;
  out_info.buffer_size = in_info.buffer_size;
  out_info.timestamp = in_info.timestamp;

  side_data.reserve(in_info.side_data.size());
  for (const StarboardSampleSideData& in_side_data : in_info.side_data) {
    SbPlayerSampleSideData out_side_data;

    out_side_data.type =
        static_cast<SbPlayerSampleSideDataType>(in_side_data.type);
    out_side_data.data = in_side_data.data.data();
    out_side_data.size = in_side_data.data.size();

    side_data.push_back(std::move(out_side_data));
  }
  out_info.side_data = side_data.empty() ? nullptr : side_data.data();
  out_info.side_data_count = side_data.size();

  // Set the audio/video specific fields.
  switch (in_info.type) {
    case kStarboardMediaTypeAudio:
      out_info.audio_sample_info =
          ToSbMediaAudioSampleInfo(in_info.audio_sample_info);
      break;
    case kStarboardMediaTypeVideo:
      out_info.video_sample_info =
          ToSbMediaVideoSampleInfo(in_info.video_sample_info);
      break;
  }

  if (in_info.drm_info) {
    const StarboardDrmSampleInfo& in_drm_info = *in_info.drm_info;

    drm_info.encryption_scheme =
        static_cast<SbDrmEncryptionScheme>(in_drm_info.encryption_scheme);
    drm_info.encryption_pattern.crypt_byte_block =
        in_drm_info.encryption_pattern.crypt_byte_block;
    drm_info.encryption_pattern.skip_byte_block =
        in_drm_info.encryption_pattern.skip_byte_block;

    base::span<uint8_t, kDrmSampleInfoIvSize>(drm_info.initialization_vector)
        .copy_from_nonoverlapping(in_drm_info.initialization_vector);
    drm_info.initialization_vector_size =
        in_drm_info.initialization_vector_size;

    base::span<uint8_t, kDrmSampleInfoIdentifierSize>(drm_info.identifier)
        .copy_from_nonoverlapping(in_drm_info.identifier);
    drm_info.identifier_size = in_drm_info.identifier_size;

    subsample_mappings.reserve(in_drm_info.subsample_mapping.size());
    for (const auto& in_mapping : in_drm_info.subsample_mapping) {
      SbDrmSubSampleMapping mapping;
      mapping.clear_byte_count = in_mapping.clear_byte_count;
      mapping.encrypted_byte_count = in_mapping.encrypted_byte_count;
      subsample_mappings.push_back(std::move(mapping));
    }
    drm_info.subsample_count = subsample_mappings.size();
    drm_info.subsample_mapping =
        subsample_mappings.empty() ? nullptr : subsample_mappings.data();

    out_info.drm_info = &drm_info;
  } else {
    out_info.drm_info = nullptr;
  }
  return out_info;
}

void StarboardApiWrapperBase::WriteSample(
    void* player,
    StarboardMediaType type,
    base::span<const StarboardSampleInfo> sample_infos) {
  std::vector<SbPlayerSampleInfo> samples;
  std::vector<std::vector<SbPlayerSampleSideData>> side_data;
  SbDrmSampleInfo drm_info;
  std::vector<SbDrmSubSampleMapping> subsample_mappings;
  for (const StarboardSampleInfo& sample_info : sample_infos) {
    side_data.push_back({});
    samples.push_back(ToSbPlayerSampleInfo(sample_info, side_data.back(),
                                           drm_info, subsample_mappings));
  }
  CallWriteSamples(static_cast<SbPlayer>(player),
                   static_cast<SbMediaType>(type), samples);
}

StarboardMediaSupportType StarboardApiWrapperBase::CanPlayMimeAndKeySystem(
    const char* mime,
    const char* key_system) {
  return static_cast<StarboardMediaSupportType>(
      SbMediaCanPlayMimeAndKeySystem(mime, key_system));
}

}  // namespace media
}  // namespace chromecast
