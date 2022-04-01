#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_OUTPUT_STREAM_DUMMY
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_OUTPUT_STREAM_DUMMY

#include "chromecast/public/media/mixer_output_stream.h"

namespace chromecast {
namespace media {

// Dummy MixerOutputStream implementation.
class MixerOutputStreamDummy : public MixerOutputStream {
 public:
  MixerOutputStreamDummy();
  ~MixerOutputStreamDummy() override;
  MixerOutputStreamDummy(const MixerOutputStreamDummy&) = delete;
  MixerOutputStreamDummy& operator=(const MixerOutputStreamDummy&) = delete;

  // MixerOutputStream implementation:
  bool Start(int requested_sample_rate, int channels) override;
  int GetNumChannels() override;
  int GetSampleRate() override;
  MediaPipelineBackend::AudioDecoder::RenderingDelay GetRenderingDelay()
      override;
  int OptimalWriteFramesCount() override;
  bool Write(const float* data,
             int data_size,
             bool* out_playback_interrupted) override;
  void Stop() override;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_OUTPUT_STREAM_DUMMY
