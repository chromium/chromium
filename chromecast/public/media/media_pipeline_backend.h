// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_BACKEND_H_
#define CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_BACKEND_H_

#include <stdint.h>

#include <string>

#include "cast_key_status.h"
#include "chromecast_export.h"
#include "decoder_config.h"

namespace chromecast {
class TaskRunner;
struct Size;

namespace media {
class CastDecoderBuffer;

// Interface for platform-specific output of media.
// A new MediaPipelineBackend will be instantiated for each media player
// instance and raw audio stream.  If a backend has both video and audio
// decoders, they must be synchronized.
// If more backends are requested than the platform supports, the unsupported
// extra backends may return nullptr for CreateAudioDecoder/CreateVideoDecoder.
// The basic usage pattern is:
//   * Decoder objects created and delegates set, then Initialize called
//   * Start/Stop/Pause/Resume used to manage playback state
//   * Decoder objects are used to pass actual stream data buffers
//   * Backend must make appropriate callbacks on the provided Delegate
// All functions will be called on the media thread.  Delegate callbacks
// must be made on this thread also (using provided TaskRunner if necessary).
class MediaPipelineBackend {
 public:
  // Return code for PushBuffer
  enum BufferStatus {
    kBufferSuccess,
    kBufferFailed,
    kBufferPending,
  };

  class Decoder {
   public:
    using BufferStatus = MediaPipelineBackend::BufferStatus;

    // Delegate methods must be called on the main CMA thread.
    class Delegate {
     public:
      using BufferStatus = MediaPipelineBackend::BufferStatus;

      // See comments on PushBuffer.  Must not be called with kBufferPending.
      virtual void OnPushBufferComplete(BufferStatus status) = 0;

      // Must be called after an end-of-stream buffer has been rendered (ie, the
      // last real buffer has been sent to the output hardware).
      virtual void OnEndOfStream() = 0;

      // May be called if a decoder error occurs. No more calls to PushBuffer()
      // should be made after this is called.
      virtual void OnDecoderError() = 0;

      // Must be called when a decryption key status changes.
      virtual void OnKeyStatusChanged(const std::string& key_id,
                                      CastKeyStatus key_status,
                                      uint32_t system_code) = 0;

      // Must be called when video resolution change is detected by the decoder.
      // Only relevant for video decoders.
      virtual void OnVideoResolutionChanged(const Size& size) = 0;

     protected:
      virtual ~Delegate() {}
    };

    // Provides the delegate for this decoder. Called once before the backend
    // is initialized; is never called after the backend is initialized.
    virtual void SetDelegate(Delegate* delegate) = 0;

    // Pushes a buffer of data for decoding and output.  If the implementation
    // cannot push the buffer now, it must store the buffer, return
    // |kBufferPending| and execute the push at a later time when it becomes
    // possible to do so.  The implementation must then invoke
    // Delegate::OnPushBufferComplete once the push has been completed.  Pushing
    // a pending buffer should be aborted if Stop is called;
    // OnPushBufferComplete need not be invoked in this case.
    // If |kBufferPending| is returned, the pipeline will stop pushing any
    // further buffers until OnPushBufferComplete is invoked.
    // OnPushBufferComplete should be only be invoked to indicate completion of
    // a pending buffer push - not for the immediate |kBufferSuccess| return
    // case.
    // The buffer's lifetime is managed by the caller code - it MUST NOT be
    // deleted by the MediaPipelineBackend implementation, and MUST NOT be
    // dereferenced after completion of buffer push (i.e.
    // returning kBufferSuccess/kBufferFailure for synchronous completion,
    // calling OnPushBufferComplete() for kBufferPending case).
    virtual BufferStatus PushBuffer(CastDecoderBuffer* buffer) = 0;

   protected:
    virtual ~Decoder() {}
  };

  class AudioDecoder : public Decoder {
   public:
    // Info on pipeline latency: amount of data in pipeline not rendered yet,
    // and timestamp of system clock (must be CLOCK_MONOTONIC_RAW) at which
    // delay measurement was taken. Both times in microseconds.
    struct RenderingDelay {
      RenderingDelay()
          : delay_microseconds(0), timestamp_microseconds(INT64_MIN) {}
      RenderingDelay(int64_t delay_microseconds_in,
                     int64_t timestamp_microseconds_in)
          : delay_microseconds(delay_microseconds_in),
            timestamp_microseconds(timestamp_microseconds_in) {}
      int64_t delay_microseconds;
      int64_t timestamp_microseconds;
    };

    // Statistics (computed since last call to backend Start).
    struct Statistics {
      // Reported as webkitAudioBytesDecoded.  Counts number of source bytes
      // decoded (not decoder output bytes).
      uint64_t decoded_bytes;
    };

    // Android AudioTrack timestamp information.
    struct AudioTrackTimestamp {
      AudioTrackTimestamp()
          : audio_track_frame_position(0),
            audio_track_frame_position_without_silence(0),
            audio_track_nano_time(INT64_MIN) {}
      AudioTrackTimestamp(int64_t audio_track_frame_position_in,
                          int64_t audio_track_frame_position_without_silence_in,
                          int64_t audio_track_nano_time_in)
          : audio_track_frame_position(audio_track_frame_position_in),
            audio_track_frame_position_without_silence(
                audio_track_frame_position_without_silence_in),
            audio_track_nano_time(audio_track_nano_time_in) {}
      // Position in frames relative to start of an assumed audio stream in the
      // Android AudioTrack.
      int64_t audio_track_frame_position;
      // Position in frames relative to start of an assumed audio stream in the
      // Android AudioTrack, excluding frames of silence buffers which don't
      // have timestamps.
      int64_t audio_track_frame_position_without_silence;
      // Time associated with the frame in the Android audio pipeline.
      int64_t audio_track_nano_time;
    };

    // Provides the audio configuration.  Called once before the backend is
    // initialized, and again any time the configuration changes (in any state).
    // Note that SetConfig() may be called before SetDelegate() is called.
    // Returns true if the configuration is a supported configuration.
    virtual bool SetConfig(const AudioConfig& config) = 0;

    // Sets the volume multiplier for this audio stream.
    // The multiplier is in the range [0.0, 1.0].  If not called, a default
    // multiplier of 1.0 is assumed. Returns true if successful.
    // Only called after the backend has been initialized.
    virtual bool SetVolume(float multiplier) = 0;

    // Returns the pipeline latency: i.e. the amount of data
    // in the pipeline that have not been rendered yet, in microseconds.
    // Returns a RenderingDelay.timestamp = INT64_MIN if the latency is not
    // available.
    // Only called when the backend is playing.
    virtual RenderingDelay GetRenderingDelay() = 0;

    // Returns the playback statistics since last call to backend Start.  Only
    // called when playing or paused.
    virtual void GetStatistics(Statistics* statistics) = 0;

    // Returns the Android AudioTrack timestamp information.
    // Returns a AudioTrackTimestamp.audio_track_nano_time = INT64_MIN if the
    // timestamp is not available.
    virtual AudioTrackTimestamp GetAudioTrackTimestamp() = 0;

    // Returns the streaming start threshold of the current audio track.
    // Returns zero if the start threshold is not available.
    virtual int GetStartThresholdInFrames() = 0;

    // Returns the minimum amount of audio data buffered (in microseconds)
    // necessary to prevent underrun for the given |config|; ie, if the
    // rendering delay falls below this value, then underrun may occur.
    static int64_t GetMinimumBufferedTime(const AudioConfig& config)
        __attribute__((__weak__));

    // Returns true if the audio decoder requires that encrypted buffers be
    // decrypted before being passed to PushBuffer().
    CHROMECAST_EXPORT static bool RequiresDecryption()
        __attribute__((__weak__));

   protected:
    ~AudioDecoder() override {}
  };

  class VideoDecoder : public Decoder {
   public:
    // Statistics (computed since last call to backend Start).
    struct Statistics {
      // Counts number of source bytes decoded (not decoder output).
      uint64_t decoded_bytes;   // Reported as webkitVideoBytesDecoded.
      uint64_t decoded_frames;  // Reported as webkitDecodedFrames.
      uint64_t dropped_frames;  // Reported as webkitDroppedFrames.
    };

    // FrameDisplayInfoDelegate methods must be called on the main CMA thread.
    class FrameDisplayInfoDelegate {
     public:
      // OnFrameDisplayed is called either when the frame is displayed
      // successfully (with valid |display_time|), or when the frame is dropped
      // but it's meant to displayed(with |display_time|==INT64_MIN).
      // If a pushed frame is repeated on screen, OnFrameDisplayed() is called
      // only once.
      // For this API to work properly, the pts fields in CastDecoderBuffer must
      // be unique.
      virtual void OnFrameDisplayed(
          int64_t push_time,     // Time when the frame is pushed to backend,
                                 // in microseconds, relative to
                                 // CLOCK_MONOTONIC or CLOCK_MONOTONIC_RAW.
                                 // When it's not available it's INT64_MIN.
          int64_t display_time,  // Time when the frame is displayed on screen,
                                 // in microseconds, relative to
                                 // CLOCK_MONOTONIC or CLOCK_MONOTONIC_RAW.
                                 // If it's INT64_MIN, the frame is not
                                 // displayed but dropped.
          int64_t pts  // The |timestamp| within the CastDecoderBuffer that's
                       // pushed to backend, in microseconds.
          ) = 0;

     protected:
      virtual ~FrameDisplayInfoDelegate() = default;
    };

    // Provides the video configuration. Called once with the configuration for
    // the primary stream before the backend is initialized, and the
    // configuration may contain a pointer to additional configuration for a
    // secondary stream. Called again with the configuration for either the
    // primary or secondary stream when either changes after the backend is
    // initialized.
    // Note that SetConfig() may be called before SetDelegate() is called.
    // Returns true if the configuration is a supported configuration.
    virtual bool SetConfig(const VideoConfig& config) = 0;

    // Returns the playback statistics since last call to backend Start.  Only
    // called when playing or paused.
    virtual void GetStatistics(Statistics* statistics) = 0;

    // Register |frame_display_info_delegate| on |video_decoder| to receive
    // OnFrameDisplayed.
    // TODO(guohuideng): make this a virtual method on VideoDecoder at next API
    // update.
    CHROMECAST_EXPORT static void SetFrameDisplayInfoDelegate(
        FrameDisplayInfoDelegate* frame_display_info_delegate,
        VideoDecoder* video_decoder) __attribute__((weak));

    // The optional APIs below are for low latency playback performance
    // improvement and they are only available and sensible on very limited
    // implementations. Only intended users should be concerned with them.

    // Get number of frames available for display in last Vsync, in
    // |num_of_frames_available_last_vsync|. It returns false if it fails.
    CHROMECAST_EXPORT static bool GetNumberOfFramesAvailableLastVsync(
        uint32_t* num_of_frames_available_last_vsync,
        VideoDecoder* video_decoder) __attribute__((weak));
    // Get number of frames pushed but not decoded yet, in
    // |num_of_frames_enqueued_pre_decode|. It returns false if it fails.
    CHROMECAST_EXPORT static bool GetNumberOfFramesEnqueuedPreDecode(
        uint32_t* num_of_frames_enqueued_pre_decode,
        VideoDecoder* video_decoder) __attribute__((weak));
    // Change the max number of frames that output module would hold before it
    // drops frame. It returns false if it fails.
    CHROMECAST_EXPORT static bool SetFreeRunDropThreshold(
        uint32_t free_run_drop_threshold,
        VideoDecoder* video_decoder) __attribute__((weak));
    // Set max output buffer count at post decoding stage. It returns false if
    // it fails.
    CHROMECAST_EXPORT static bool SetMaxOutputBufferCount(
        uint32_t max_output_buffer_count,
        VideoDecoder* video_decoder) __attribute__((weak));

   protected:
    ~VideoDecoder() override {}
  };

  // This is created/deleted on media thread. All the methods and delegate
  // methods should be called on media thread.
  class AudioDecryptor {
   public:
    using BufferStatus = MediaPipelineBackend::BufferStatus;

    // Delegate methods must be called on media thread.
    class Delegate {
     public:
      // Called to indicate decryptor can accept more buffers, after
      // PushBufferForDecrypt returns |kBufferPending|.
      virtual void OnPushBufferForDecryptComplete(BufferStatus status) = 0;

      // Must be called for each pushed buffer (both clear and encrypted).
      // Returns false if decryption fails, e.g. license policy violation.
      virtual void OnDecryptComplete(bool success) = 0;

     protected:
      virtual ~Delegate() = default;
    };

    // Aborts all the pending operations once the object is deleted.
    virtual ~AudioDecryptor() = default;

    // Provides delegate for this decryptor. Called once before any other APIs.
    virtual void SetDelegate(Delegate* delegate) = 0;

    // Pushes a buffer of data for decrypting. Decrypted data will be put in
    // |output|. Implementation MUST check the license policy before returning
    // the clear buffer back.
    //
    // Similar to Decoder::PushBuffer, implementation can return
    // |kBufferPending| to stop caller from pushing more buffers. See comments
    // of Decoder::PushBuffer for more details on buffer pushing.
    //
    // Implementation must invoke Delegate::OnDecryptComplete once data is
    // decrypted. Both encrypted and clear buffers will be pushed.
    // Implementation should call the delegate methods in the same sequence as
    // pushing buffer.
    //
    // Once EOS buffer is pushed, implementation should decrypt and return all
    // the buffers.
    //
    // |buffer| and |output| are owned by caller. Caller must not destroy them
    // until Delegate::OnDecryptComplete is called. |output| must be long
    // enough to hold clear data. |output| may overlap with the memory carried
    // by |buffer|. The size of decrypted data should be same as encrypted data.
    virtual BufferStatus PushBufferForDecrypt(CastDecoderBuffer* buffer,
                                              uint8_t* output) = 0;
  };

  virtual ~MediaPipelineBackend() {}

  // Creates a new AudioDecoder attached to this pipeline.  MediaPipelineBackend
  // maintains ownership of the decoder object (and must not delete before it's
  // destroyed).  Will be called zero or more times, all calls made before
  // Initialize. May return nullptr if the platform implementation cannot
  // support any additional simultaneous playback at this time.
  virtual AudioDecoder* CreateAudioDecoder() = 0;

  // Creates a new VideoDecoder attached to this pipeline.  MediaPipelineBackend
  // maintains ownership of the decoder object (and must not delete before it's
  // destroyed).  Will be called zero or more times, all calls made before
  // Initialize. Note: Even if your backend only supports audio, you must
  // provide a default implementation of VideoDecoder; one way to do this is to
  // inherit from MediaPipelineBackendDefault. May return nullptr if the
  // platform implementation cannot support any additional simultaneous playback
  // at this time.
  virtual VideoDecoder* CreateVideoDecoder() = 0;

  // Initializes the backend.  This will be called once, after Decoder creation
  // but before all other functions.  Hardware resources for all decoders should
  // be acquired here.  Backend is then considered in Initialized state.
  // Returns false for failure.
  virtual bool Initialize() = 0;

  // Places pipeline into playing state.  Playback will start at given time once
  // buffers are pushed.  Called only when in Initialized state. |start_pts| is
  // the start playback timestamp in microseconds.
  virtual bool Start(int64_t start_pts) = 0;

  // Returns pipeline to 'Initialized' state.  May be called while playing or
  // paused.  Buffers cannot be pushed in Initialized state.
  virtual void Stop() = 0;

  // Pauses media playback.  Called only when in playing state.
  virtual bool Pause() = 0;

  // Resumes media playback.  Called only when in paused state.
  virtual bool Resume() = 0;

  // Gets the current playback timestamp in microseconds. Only called when in
  // the "playing" or "paused" states. Returns INT64_MIN if the PTS is not
  // available.
  virtual int64_t GetCurrentPts() = 0;

  // Sets the playback rate.  |rate| > 0.  If this is not called, a default rate
  // of 1.0 is assumed. Returns true if successful. Only called when in
  // the "playing" or "paused" states.
  virtual bool SetPlaybackRate(float rate) = 0;

  // Creates a new AudioDecryptor for extracting clear audio buffers.  Caller
  // owns the object. This will be called multiple times on media thread. When
  // the object is deleted, the implementation should abort all the pending
  // operations.
  // This function is optional. The correct implementation must return a valid
  // object. Platforms which support standard CDM decryption APIs do not need to
  // implement this function.
  CHROMECAST_EXPORT static AudioDecryptor* CreateAudioDecryptor(
      EncryptionScheme scheme,
      TaskRunner* task_runner) __attribute__((weak));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_BACKEND_H_
