// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CAST_MEDIA_SOURCE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CAST_MEDIA_SOURCE_H_

#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "base/check.h"
#include "base/time/time.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using cast_channel::ReceiverAppType;

namespace media_router {

// Placeholder app ID advertised by the multizone leader in a receiver status
// message.
static constexpr char kMultizoneLeaderAppId[] = "MultizoneLeader";

static const constexpr char* const kMultizoneMemberAppIds[] = {
    kMultizoneLeaderAppId,
    "531A4F84",  // MultizoneLeader
    "MultizoneFollower",
    "705D30C6"  // MultizoneFollower
};

static constexpr base::TimeDelta kDefaultLaunchTimeout = base::Seconds(60);

// Class for storing a bitwise OR of enum values.
//
// TODO(crbug.com/1291715): Make values of cast_channel::CastDeviceCapability
// consecutive and store sets of values using a class like v8::base::EnumSet
// instead of this monstrosity.
template <typename E, typename T = std::underlying_type_t<E>>
class BitwiseOr {
 public:
  constexpr BitwiseOr() : bits_(0) {}
  constexpr BitwiseOr(std::initializer_list<E> values) : bits_(0) {
    for (E e : values)
      Add(e);
  }
  static constexpr BitwiseOr FromBits(T bits) { return BitwiseOr(bits); }
  bool empty() const { return bits_ == 0; }
  T bits() const { return bits_; }
  void Add(E value) { bits_ |= Mask(value); }
  void Remove(E value) { bits_ &= ~Mask(value); }
  bool Has(E value) const { return (bits_ & Mask(value)) != 0; }
  bool HasAll(const BitwiseOr& other) const {
    return (bits_ & other.bits_) == other.bits_;
  }
  bool operator==(const BitwiseOr& other) const { return bits_ == other.bits_; }
  bool operator!=(const BitwiseOr& other) const { return !(*this == other); }

 private:
  explicit constexpr BitwiseOr(T bits) : bits_(bits) {}

  static T Mask(E value) {
    const T result = static_cast<T>(value);
    DCHECK(static_cast<E>(result) == value);
    return result;
  }
  T bits_;
};

// Represents a Cast app and its capabilitity requirements.
struct CastAppInfo {
  explicit CastAppInfo(
      const std::string& app_id,
      BitwiseOr<cast_channel::CastDeviceCapability> required_capabilities);
  ~CastAppInfo();

  CastAppInfo(const CastAppInfo& other);

  static CastAppInfo ForCastStreaming();
  static CastAppInfo ForCastStreamingAudio();

  std::string app_id;

  // A bitset of capabilities required by the app.
  BitwiseOr<cast_channel::CastDeviceCapability> required_capabilities;
};

// Auto-join policy determines when the SDK will automatically connect a sender
// application to an existing session after API initialization.
enum class AutoJoinPolicy {
  // No automatic connection.  This is the default when no policy is specified.
  kPageScoped,

  // Automatically connects when the session was started with the same app ID,
  // in the same tab and page origin.
  kTabAndOriginScoped,

  // Automatically connects when the session was started with the same app ID
  // and the same page origin (regardless of tab).
  kOriginScoped,

  kMaxValue = kOriginScoped,
};

// Default action policy determines when the SDK will automatically create a
// session after initializing the API.  This also controls the default action
// for the tab in the Cast dialog.
enum class DefaultActionPolicy {
  // If the tab containing the app is being cast when the API initializes, the
  // SDK stops tab casting and automatically launches the app.  The Cast dialog
  // prompts the user to cast the app.
  kCreateSession,

  // No automatic launch is done after initializing the API, even if the tab is
  // being cast.  The Cast dialog prompts the user to mirror the tab (mirror,
  // not cast, despite the name).
  kCastThisTab,

  kMaxValue = kCastThisTab,
};

// Tests whether a sender specified by (origin1, tab_id1) is allowed by |policy|
// to join (origin2, tab_id2).
bool IsAutoJoinAllowed(AutoJoinPolicy policy,
                       const url::Origin& origin1,
                       int tab_id1,
                       const url::Origin& origin2,
                       int tab_id2);

// Returns true if |source_id| is valid for site-initiated mirroring.
bool IsSiteInitiatedMirroringSource(const MediaSource::Id& source_id);

// Represents a MediaSource parsed into structured, Cast specific data. The
// following MediaSources can be parsed into CastMediaSource:
// - Cast Presentation URLs
// - HTTP(S) Presentation URLs
// - Desktop / tab mirroring URNs
class CastMediaSource {
 public:
  // Returns the parsed form of |source|, or nullptr if it cannot be parsed.
  static std::unique_ptr<CastMediaSource> FromMediaSource(
      const MediaSource& source);
  static std::unique_ptr<CastMediaSource> FromMediaSourceId(
      const MediaSource::Id& source);

  static std::unique_ptr<CastMediaSource> FromAppId(const std::string& app_id);

  static std::unique_ptr<CastMediaSource> ForSiteInitiatedMirroring();

  CastMediaSource(const MediaSource::Id& source_id,
                  const std::vector<CastAppInfo>& app_infos,
                  AutoJoinPolicy auto_join_policy = AutoJoinPolicy::kPageScoped,
                  DefaultActionPolicy default_action_policy =
                      DefaultActionPolicy::kCreateSession);
  CastMediaSource(const CastMediaSource& other);
  ~CastMediaSource();

  // Returns true if |app_infos_| contain |app_id|.
  bool ContainsApp(const std::string& app_id) const;
  bool ContainsAnyAppFrom(const std::vector<std::string>& app_ids) const;
  // Returns true if |app_infos_| contain streaming or audio streaming app ID.
  bool ContainsStreamingApp() const;

  // Returns a list of App IDs in this CastMediaSource.
  std::vector<std::string> GetAppIds() const;

  // Returns true iff all of the following are true: this source is a streaming
  // source, the site requested audio capture, and the application is capable of
  // providing audio capture (and has the user's permission to do so).
  bool ProvidesStreamingAudioCapture() const;

  const MediaSource::Id& source_id() const { return source_id_; }
  const std::vector<CastAppInfo>& app_infos() const { return app_infos_; }
  const std::string& client_id() const { return client_id_; }
  void set_client_id(const std::string& client_id) { client_id_ = client_id; }
  base::TimeDelta launch_timeout() const { return launch_timeout_; }
  void set_launch_timeout(base::TimeDelta launch_timeout) {
    launch_timeout_ = launch_timeout;
  }
  AutoJoinPolicy auto_join_policy() const { return auto_join_policy_; }
  DefaultActionPolicy default_action_policy() const {
    return default_action_policy_;
  }
  absl::optional<base::TimeDelta> target_playout_delay() const {
    return target_playout_delay_;
  }
  void set_target_playout_delay(
      const absl::optional<base::TimeDelta>& target_playout_delay) {
    target_playout_delay_ = target_playout_delay;
  }
  // See also: ProvidesStreamingAudioCapture().
  bool site_requested_audio_capture() const {
    return site_requested_audio_capture_;
  }
  void set_site_requested_audio_capture(bool is_requested) {
    site_requested_audio_capture_ = is_requested;
  }
  const std::string& app_params() const { return app_params_; }
  void set_app_params(const std::string& app_params) {
    app_params_ = app_params;
  }
  const std::vector<ReceiverAppType>& supported_app_types() const {
    return supported_app_types_;
  }
  void set_supported_app_types(const std::vector<ReceiverAppType>& types);
  cast_channel::VirtualConnectionType connection_type() const {
    return connection_type_;
  }
  void set_connection_type(
      cast_channel::VirtualConnectionType connection_type) {
    connection_type_ = connection_type;
  }

 private:
  MediaSource::Id source_id_;
  std::vector<CastAppInfo> app_infos_;
  AutoJoinPolicy auto_join_policy_;
  DefaultActionPolicy default_action_policy_;
  base::TimeDelta launch_timeout_ = kDefaultLaunchTimeout;
  // Optional parameters.
  std::string client_id_;
  absl::optional<base::TimeDelta> target_playout_delay_;
  bool site_requested_audio_capture_ = true;
  std::vector<ReceiverAppType> supported_app_types_ = {ReceiverAppType::kWeb};
  std::string app_params_;
  cast_channel::VirtualConnectionType connection_type_ =
      cast_channel::VirtualConnectionType::kStrong;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CAST_MEDIA_SOURCE_H_
