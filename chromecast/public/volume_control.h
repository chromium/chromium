// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_VOLUME_CONTROL_H_
#define CHROMECAST_PUBLIC_VOLUME_CONTROL_H_

#include <ostream>
#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {
namespace media {

// Audio content types for volume control. Each content type has a separate
// volume and mute state.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromecast.media
enum class AudioContentType {
  kMedia,          // Normal audio playback; also used for system sound effects.
  kAlarm,          // Alarm sounds.
  kCommunication,  // Voice communication, eg assistant TTS.
  kOther,          // No content type volume control (only per-stream control).
  kNumTypes,       // Not a valid type; should always be last in the enum.
};

inline std::ostream& operator<<(std::ostream& os, AudioContentType audio_type) {
  switch (audio_type) {
    case AudioContentType::kMedia:
      return os << "MEDIA";
    case AudioContentType::kAlarm:
      return os << "ALARM";
    case AudioContentType::kCommunication:
      return os << "COMMUNICATION";
    case AudioContentType::kOther:
      return os << "OTHER";
    default:
      return os << "Add a new entry above, otherwise kNumTypes is not a valid "
                   "type.";
  }
}

// Different sources of volume changes. Used to change behaviour (eg feedback
// sounds) based on the source.
enum class VolumeChangeSource {
  kUser,              // User-initiated volume change.
  kAutomatic,         // Automatic volume change, no user involvement.
  kAutoWithFeedback,  // Automatic volume change, but we still want to have
                      // volume feedback UX.
  kUserWithNoAudioFeedback,  // User-initiated change, but audible feedback is
                             // disabled.
};

inline std::ostream& operator<<(std::ostream& os,
                                VolumeChangeSource vol_change_source) {
  switch (vol_change_source) {
    case VolumeChangeSource::kUser:
      return os << "USER";
    case VolumeChangeSource::kAutomatic:
      return os << "AUTOMATIC";
    case VolumeChangeSource::kAutoWithFeedback:
      return os << "AUTO_WITH_FEEDBACK";
    case VolumeChangeSource::kUserWithNoAudioFeedback:
      return os << "USER_NO_AUDIO_FEEDBACK";
  }
}

// Observer for volume/mute state changes. This is useful to detect volume
// changes that occur outside of cast_shell. Add/RemoveVolumeObserver() must not
// be called synchronously from OnVolumeChange() or OnMuteChange(). Note that
// no volume/mute changes will occur for AudioContentType::kOther, so no
// observer methods will be called with that type.
class VolumeObserver {
 public:
  // Called whenever the volume changes for a given stream |type|. May be called
  // on an arbitrary thread.
  virtual void OnVolumeChange(VolumeChangeSource source,
                              AudioContentType type,
                              float new_volume) = 0;

  // Called whenever the mute state changes for a given stream |type|. May be
  // called on an arbitrary thread.
  virtual void OnMuteChange(VolumeChangeSource source,
                            AudioContentType type,
                            bool new_muted) = 0;

 protected:
  virtual ~VolumeObserver() = default;
};

// Volume control is initialized once when cast_shell starts up, and finalized
// on shutdown. Revoking resources has no effect on volume control. All volume
// control methods are called on the same thread that calls Initialize().
class CHROMECAST_EXPORT VolumeControl {
 public:
  // Initializes platform-specific volume control. Only called when volume
  // control is in an uninitialized state. The implementation of this method
  // should load previously set volume and mute states from persistent storage,
  // so that the volume and mute are preserved across reboots.
  static void Initialize(const std::vector<std::string>& argv);

  // Tears down platform-specific volume control and returns to the
  // uninitialized state.
  static void Finalize();

  // Adds a volume observer.
  static void AddVolumeObserver(VolumeObserver* observer);
  // Removes a volume observer. After this is called, the implementation must
  // not call any more methods on the observer.
  static void RemoveVolumeObserver(VolumeObserver* observer);

  // Gets/sets the output volume for a given audio stream |type|. The volume
  // |level| is in the range [0.0, 1.0]. AudioContentType::kOther is not a valid
  // |type| for these methods.
  static float GetVolume(AudioContentType type);
  static void SetVolume(VolumeChangeSource source,
                        AudioContentType type,
                        float level);

  // Sets a multiplier on the attenuation level for a given audio stream type.
  // Used for stereo pair balance.
  static void SetVolumeMultiplier(AudioContentType type, float multiplier)
      __attribute__((weak));

  // Gets/sets the mute state for a given audio stream |type|.
  // AudioContentType::kOther is not a valid |type| for these methods.
  static bool IsMuted(AudioContentType type);
  static void SetMuted(VolumeChangeSource source,
                       AudioContentType type,
                       bool muted);

  // Limits the output volume for a given stream |type| to no more than |limit|.
  // This does not affect the logical volume for the stream type; the volume
  // returned by GetVolume() should not change, and no OnVolumeChange() event
  // should be sent to observers. AudioContentType::kOther is not a valid |type|
  // for this method.
  static void SetOutputLimit(AudioContentType type, float limit);

  // Called to enable power save mode when no audio is being played
  // (|power_save_on| will be true in this case), and to disable power save mode
  // when audio playback resumes (|power_save_on| will be false).
  // NOTE: This is optional (therefore a weak symbol) because most platforms
  // do not have any need to implement it.
  static void SetPowerSaveMode(bool power_save_on) __attribute__((weak));

  // Converts a volume level in the range [0.0, 1.0] to/from a volume in dB.
  // The volume in dB should be full-scale (so a volume level of 1.0 would be
  // 0.0 dBFS, and any lower volume level would be negative).
  // NOTE: Unlike the other VolumeControl methods, these may be called before
  // Initialize() or after Finalize(). May be called from multiple processes.
  static float VolumeToDbFS(float volume);
  static float DbFSToVolume(float dbfs);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_VOLUME_CONTROL_H_
