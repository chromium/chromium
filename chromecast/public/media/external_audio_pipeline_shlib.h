// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_EXTERNAL_AUDIO_PIPELINE_SHLIB_H_
#define CHROMECAST_PUBLIC_MEDIA_EXTERNAL_AUDIO_PIPELINE_SHLIB_H_

#include <memory>
#include <string>

#include "cast_media_shlib.h"
#include "chromecast_export.h"
#include "media_pipeline_backend.h"
#include "mixer_output_stream.h"

namespace chromecast {
namespace media {

class CHROMECAST_EXPORT ExternalAudioPipelineShlib {
 public:
  // Represents a media input source of the external media pipeline.
  enum class MediaInputSource {
    UNKNOWN_INPUT_SOURCE,
    HDMI,
    AIRPLAY,
  };

  // Represents a media playback state of the external media pipeline.
  enum class MediaPlaybackState {
    UNKNOWN_STATE,
    STOPPED,
    PAUSED,
    PLAYING,
  };

  // Observer for reporting requests for media volume change/muting from the
  // external media pipeline. The external pipeline should communicate the media
  // volume change requests through this observer and otherwise shouldn't change
  // the media volume in any way. Cast pipeline will call
  // SetExternalMediaVolume/SetExternalMediaMuted to actually change the volume
  // of the external media. The external pipeline must only apply the received
  // volume to the external (non-Cast) audio. It specifically must not be
  // applied to the mixer output stream. The received volume should also not be
  // reported again through this observer.
  class ExternalMediaVolumeChangeRequestObserver {
   public:
    // Called by the external pipeline to request a media volume change by the
    // cast pipeline.
    virtual void OnVolumeChangeRequest(float new_volume) = 0;

    // Called by the external pipeline to request muting/unmuting media volume
    // by the cast pipeline.
    virtual void OnMuteChangeRequest(bool new_muted) = 0;

   protected:
    virtual ~ExternalMediaVolumeChangeRequestObserver() = default;
  };

  // Media metadata which can be acquired from the external pipeline.
  struct ExternalMediaMetadata {
    ExternalMediaMetadata();
    ExternalMediaMetadata(const ExternalMediaMetadata& other);
    ~ExternalMediaMetadata();

    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    int track_num = -1;
    MediaInputSource source = MediaInputSource::HDMI;
    std::string source_description;
    MediaPlaybackState state = MediaPlaybackState::UNKNOWN_STATE;
  };

  // Observer for reporting media metadata change from the external media
  // pipeline, e.g., title, artist, input source, play back state, and etc.
  class ExternalMediaMetadataChangeObserver {
   public:
    // Called when media metadata is updated.
    virtual void OnExternalMediaMetadataChanged(
        const ExternalMediaMetadata& metadata) = 0;

   protected:
    virtual ~ExternalMediaMetadataChangeObserver() = default;
  };

  // Observer for audio loopback data.
  class LoopbackAudioObserver {
   public:
    // Called whenever audio data is about to be output. The |timestamp| is the
    // estimated time in microseconds (relative to CLOCK_MONOTONIC_RAW) that
    // the audio will actually be output. |length| is the length of the audio
    // |data| in bytes. The format of the data is given by |sample_format| and
    // |num_channels|.
    // This method may be called by any thread, and MUST not block or take very
    // much time (to avoid audio underruns).
    virtual void OnLoopbackAudio(int64_t timestamp,
                                 SampleFormat sample_format,
                                 int sample_rate,
                                 int num_channels,
                                 uint8_t* data,
                                 int length) = 0;

    // Called if the loopback data is not continuous (ie, does not accurately
    // represent the actual output) for any reason. For example, if there is an
    // output underflow, or if output is disabled due to no output streams.
    // This method could be called from any thread.
    virtual void OnLoopbackInterrupted() = 0;

    // Called once this observer has been fully removed by a call to
    // RemoveLoopbackAudioObserver(). After this is called, no more calls to
    // OnLoopbackAudio() or OnLoopbackInterrupted() will be made for this
    // observer unless it is added again. This method could be called from any
    // thread.
    virtual void OnRemoved() = 0;

   protected:
    virtual ~LoopbackAudioObserver() {}
  };

  // Returns whether this shlib is supported. If this returns true, it indicates
  // that the platform uses an external audio pipeline that needs to be combined
  // with Cast's media pipeline.
  static bool IsSupported();

  // Adds an external media volume observer.
  static void AddExternalMediaVolumeChangeRequestObserver(
      ExternalMediaVolumeChangeRequestObserver* observer);

  // Removes an external media volume observer. After this is called, the
  // implementation must not call any more methods on the observer.
  static void RemoveExternalMediaVolumeChangeRequestObserver(
      ExternalMediaVolumeChangeRequestObserver* observer);

  // Sets the effective volume that the external pipeline must apply. The volume
  // must be applied only to external (non-Cast) audio. Cast audio must never
  // be affected. The pipeline should not report it back through the volume
  // observer. The volume |level| is in the range [0.0, 1.0].
  static void SetExternalMediaVolume(float level);

  // Sets the effective muted state that the external pipeline must apply. The
  // mute state must be applied only to external (non-Cast) audio. Cast audio
  // must never be affected. The pipeline should not report this state back
  // through the volume observer.
  static void SetExternalMediaMuted(bool muted);

  // Adds a loopback audio observer. An observer will not be added more than
  // once without being removed first.
  static void AddExternalLoopbackAudioObserver(LoopbackAudioObserver* observer);

  // Removes a loopback audio observer. An observer will not be removed unless
  // it was previously added, and will not be removed more than once without
  // being added again first.
  // Once the observer is fully removed (ie. once it is certain that
  // OnLoopbackAudio() will not be called again for the observer), the
  // observer's OnRemoved() method must be called. The OnRemoved() method must
  // be called once for each time that RemoveLoopbackAudioObserver() is called
  // for a given observer, even if the observer was not added. The
  // implementation may call OnRemoved() from any thread.
  // This function is optional to implement.
  static void RemoveExternalLoopbackAudioObserver(
      LoopbackAudioObserver* observer);

  // Adds an external media metadata observer.
  static void AddExternalMediaMetadataChangeObserver(
      ExternalMediaMetadataChangeObserver* observer);

  // Removes an external media volume observer. After this is called, the
  // implementation must not call any more methods on the observer.
  static void RemoveExternalMediaMetadataChangeObserver(
      ExternalMediaMetadataChangeObserver* observer);

  // Returns an instance of MixerOutputStream from the shared library.
  // Caller will take ownership of the returned pointer.
  static std::unique_ptr<MixerOutputStream> CreateMixerOutputStream();
};

inline ExternalAudioPipelineShlib::ExternalMediaMetadata::
    ExternalMediaMetadata() = default;

inline ExternalAudioPipelineShlib::ExternalMediaMetadata::ExternalMediaMetadata(
    const ExternalMediaMetadata& other) = default;

inline ExternalAudioPipelineShlib::ExternalMediaMetadata::
    ~ExternalMediaMetadata() = default;

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_EXTERNAL_AUDIO_PIPELINE_SHLIB_H_
