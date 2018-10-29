// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_
#define CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {
namespace media {

enum SampleFormat : int;

struct AudioOutputRedirectionConfig;
class AudioOutputRedirectorToken;
class DirectAudioSource;
class DirectAudioSourceToken;
class MediaPipelineBackend;
struct MediaPipelineDeviceParams;
class RedirectedAudioOutput;
class VideoPlane;

// Provides access to platform-specific media systems and hardware resources.
// In cast_shell, all usage is from the browser process.  An implementation is
// assumed to be in an uninitialized state initially.  When uninitialized, no
// API calls will be made except for Initialize, which brings the implementation
// into an initialized state.  A call to Finalize returns the implementation to
// its uninitialized state.  The implementation must support multiple
// transitions between these states, to support resource grant/revoke events and
// also to allow multiple unit tests to bring up the media systems in isolation
// from other tests.
class CHROMECAST_EXPORT CastMediaShlib {
 public:
  using ResultCallback =
      std::function<void(bool success, const std::string& message)>;
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

  // Initializes platform-specific media systems.  Only called when in an
  // uninitialized state.
  static void Initialize(const std::vector<std::string>& argv);

  // Tears down platform-specific media systems and returns to the uninitialized
  // state.  The implementation must release all media-related hardware
  // resources.
  static void Finalize();

  // Gets the VideoPlane instance for managing the hardware video plane.
  // While an implementation is in an initialized state, this function may be
  // called at any time.  The VideoPlane object must be destroyed in Finalize.
  static VideoPlane* GetVideoPlane();

  // Creates a media pipeline backend.  Called in the browser process for each
  // media pipeline and raw audio stream. The caller owns the returned
  // MediaPipelineBackend instance.
  static MediaPipelineBackend* CreateMediaPipelineBackend(
      const MediaPipelineDeviceParams& params);

  // Fetches the renderer clock rate (Hz).
  static double GetMediaClockRate();

  // Fetches the granularity of clock rate adjustments.
  static double MediaClockRatePrecision();

  // Fetches the possible range of clock rate adjustments.
  static void MediaClockRateRange(double* minimum_rate, double* maximum_rate);

  // Sets the renderer clock rate (Hz).
  static bool SetMediaClockRate(double new_rate);

  // Tests if the implementation supports renderer clock rate adjustments.
  static bool SupportsMediaClockRateChange();

  // Adds a loopback audio observer. An observer will not be added more than
  // once without being removed first.
  // This function is optional to implement.
  static void AddLoopbackAudioObserver(LoopbackAudioObserver* observer)
      __attribute__((__weak__));

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
  static void RemoveLoopbackAudioObserver(LoopbackAudioObserver* observer)
      __attribute__((__weak__));

  // Reset the post processing pipeline. |callback| will be called with
  // |success| = |true| if the new config loads without error.
  static void ResetPostProcessors(ResultCallback callback)
      __attribute__((__weak__));

  // Updates all postprocessors with the given |name| to have new configuration
  // |config|.
  static void SetPostProcessorConfig(const std::string& name,
                                     const std::string& config)
      __attribute__((__weak__));

  // Only used on Chromecast: set and clear an image on the video plane.
  // Image data is 8-bit ARGB format; |data| buffer byte length must be
  // |width|*|height|*4. Returns whether the image could be successfully set.
  static bool SetVideoPlaneImage(int width, int height, const uint8_t* data)
      __attribute__((__weak__));
  static void ClearVideoPlaneImage() __attribute__((__weak__));

  // Sets up a direct audio source for output. The media backend will pull audio
  // directly from |source| whenever more output data is needed; this provides
  // low-latency output. The source must remain valid until
  // OnAudioPlaybackComplete() has been called on it.
  // Returns nullptr if a direct source cannot be added. Otherwise, returns a
  // token that must be passed to RemoveDirectAudioSource() to remove the source
  // when desired.
  static DirectAudioSourceToken* AddDirectAudioSource(
      DirectAudioSource* source,
      const MediaPipelineDeviceParams& params,
      int playout_channel) __attribute__((__weak__));

  // Removes a direct audio source, given the |token| that was returned by
  // AddDirectAudioSource().
  static void RemoveDirectAudioSource(DirectAudioSourceToken* token)
      __attribute__((__weak__));

  // Sets the volume multiplier for a direct audio source, given the |token|
  // that was returned by AddDirectAudioSource().
  static void SetDirectAudioSourceVolume(DirectAudioSourceToken* token,
                                         float multiplier)
      __attribute__((__weak__));

  // Adds an audio output redirector configured according to |config|, where the
  // matching audio streams will be redirected to |output|. Returns a token that
  // may be used to remove the redirection (via RemoveAudioOutputRedirection()),
  // or nullptr if the redirection could not be added (ie, if the config is
  // invalid).
  static AudioOutputRedirectorToken* AddAudioOutputRedirection(
      const AudioOutputRedirectionConfig& config,
      std::unique_ptr<RedirectedAudioOutput> output) __attribute__((__weak__));

  // Removes an audio output redirector, given the |token| that was returned by
  // AddAudioOutputRedirection().
  static void RemoveAudioOutputRedirection(AudioOutputRedirectorToken* token)
      __attribute__((__weak__));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_
