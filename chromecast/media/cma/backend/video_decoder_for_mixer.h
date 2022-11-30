// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_DECODER_FOR_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_DECODER_FOR_MIXER_H_

#include <memory>

#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

// This class represents a video decoder that exposes additional functionality
// that allows a caller to control the rate and state of the video playback
// with enough granularity to be able to sync it to the audio.
//
// The default implementation of this is in VideoDecoderNull. On no-video
// platforms, that implementation is used.
//
// On video platforms that need to use the mixer, you may override this class
// and link in an implementation of VideoDecoderForMixer::Create.
class VideoDecoderForMixer : public MediaPipelineBackend::VideoDecoder {
 public:
  class Observer {
   public:
    // Notifies the observer that the video playback is ready to start. After
    // this is called, SetPts may be reliably called to start playback at the
    // desired time.
    virtual void VideoReadyToPlay() = 0;

   protected:
    virtual ~Observer() {}
  };

  static std::unique_ptr<VideoDecoderForMixer> Create(
      const MediaPipelineDeviceParams& params);

  // On some platforms, graphics needs to be initialized before this component
  // will function appropriately in tests. Add the initialization here if
  // necessary.
  static void InitializeGraphicsForTesting();

  ~VideoDecoderForMixer() override {}

  // Initializes the VideoDecoderForMixer. Called after allocation and before
  // Start is called. Gives the implementation a chance to initialize any
  // resources.
  virtual bool Initialize() = 0;

  // Sets the observer to be notified when the video is ready to play.
  virtual void SetObserver(Observer* observer) = 0;

  // When called, playback is expected to start from |start_pts|.
  //
  // start_pts: the pts to start playing at.
  // need_avsync: deprecated. Don't use or implement.
  // TODO(almasrymina): remove deprecated.
  virtual bool Start(int64_t start_pts, bool need_avsync) = 0;

  // Stop playback.
  virtual void Stop() = 0;

  // Pause playback. The video decoder must retain its playback rate after
  // resume.
  virtual bool Pause() = 0;

  // Resume playback. The video decoder must resume playback at the same
  // playback rate prior to pausing.
  virtual bool Resume() = 0;

  // Returns the current video PTS. This will typically be the pts of the last
  // video frame displayed.
  virtual bool GetCurrentPts(int64_t* timestamp, int64_t* pts) const = 0;

  // Set the playback rate. This is used to sync the audio to the video. This
  // call will change the rate of play of video in the following manner:
  //
  // rate = 1.0 -> 1 second of video pts is played for each 1 second of
  // wallclock time.
  // rate = 1.5 -> 1.5 seconds of video pts is played for each 1 second of
  // wallclock time.
  // etc.
  virtual bool SetPlaybackRate(float rate) = 0;

  // Sets the current pts to the provided value. If |pts| is greater than the
  // current pts, all video frames in between will be dropped. If |pts| is less
  // than the current pts, all video frames in this pts range will be repeated.
  // Implementation is encouraged to smooth out this transition, such that
  // minimal jitter in the video is shown, but that is not necessary.
  virtual bool SetPts(int64_t timestamp, int64_t pts) = 0;

  // Returns number of frames dropped since the last call to Start(). This is
  // used to estimate video playback smoothness.
  // This is different from VideoDecoder::Statistics::dropped_frames. That
  // value is the number of *decoded* dropped frames. The value returned here
  // must be the total number of dropped frames, whether the frames have been
  // decoded or not.
  virtual int64_t GetDroppedFrames() = 0;

  // Returns number of frames repeated since the last call to Start(). This is
  // used to estimate video playback smoothness. Note that repeated frames could
  // be due to changes in the rate of playback, setting the PTS, or simply due
  // to frame rate conversion. This number should be the sum of all of these
  // factors.
  //
  // For example, if the current OutputRefreshRate is 60hz, and the current
  // content frame rate is 24fps, it is expected to repeat 36fps.
  virtual int64_t GetRepeatedFrames() = 0;

  // Returns the output refresh rate on this platform, in mHz (millihertz). On
  // display devices, this will be the display refresh rate. On HDMI devices,
  // this will be the refresh rate of the HDMI connection.
  virtual int64_t GetOutputRefreshRate() = 0;

  // Returns the current content refresh rate in mHz (millihertz).
  virtual int64_t GetCurrentContentRefreshRate() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_DECODER_FOR_MIXER_H_
