// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_CONFIG_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_CONFIG_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/video_codecs.h"
#include "ui/gfx/geometry/rect.h"

namespace cast_streaming {

// Note: embedders are required to implement the following codecs to be
// compatible with Cast Streaming senders: h.264, vp8, aac, opus.
class ReceiverConfig {
 public:
  // Information about the display the receiver is attached to.
  struct Display {
    // The display limitations of the actual screen, used to provide upper
    // bounds on streams. For example, we will never send 60FPS if it is going
    // to be displayed on a 30FPS screen.  Note that we may exceed the display
    // width and height for standard content sizes like 720p or 1080p.
    gfx::Rect dimensions;

    // Maximum maintainable frame rate.  If left unset, will use the
    // implementation default.
    std::optional<int> max_frame_rate;

    // Whether the embedder is capable of scaling content. If set to false, the
    // sender will manage the aspect ratio scaling.
    bool can_scale_content = false;
  };

  // Codec-specific audio limits for playback.
  struct AudioLimits {
    // Audio codec these limits apply to. If left empty, this instance is
    // assumed to apply to all codecs.
    std::optional<media::AudioCodec> codec;

    // Maximum audio sample rate.If left unset, will use the implementation
    // default.
    std::optional<int> max_sample_rate;

    // Maximum audio channels.
    media::ChannelLayout channel_layout = media::CHANNEL_LAYOUT_STEREO;

    // Minimum and maximum bitrates. Generally capture is done at the maximum
    // bit rate, since audio bandwidth is much lower than video for most
    // content.  If left unset, will use the implementation defaults.
    std::optional<int> min_bit_rate;
    std::optional<int> max_bit_rate;

    // Max playout delay in milliseconds.
    base::TimeDelta max_delay = base::Milliseconds(1500);
  };

  // Codec-specific video limits for playback.
  struct VideoLimits {
    // Video codec these limits apply to. If left empty, this instance is
    // assumed to apply to all codecs.
    std::optional<media::VideoCodec> codec;

    // Maximum pixels per second. Value is the standard amount of pixels for
    // 1080P at 30FPS.
    int max_pixels_per_second = 1920 * 1080 * 30;

    // Maximum dimensions. Minimum dimensions try to use the same aspect ratio
    // and are generated from the spec.
    gfx::Rect max_dimensions = {1920, 1080};

    // Maximum maintainable frame rate. If left unset, will use the
    // implementation default.
    std::optional<int> max_frame_rate;

    // Minimum and maximum bitrates. Default values are based on default min and
    // max dimensions, embedders that support different display dimensions
    // should strongly consider setting these fields.  If left unset, will use
    // the implementation defaults.
    std::optional<int> min_bit_rate;
    std::optional<int> max_bit_rate;

    // Max playout delay in milliseconds.
    base::TimeDelta max_delay = base::Milliseconds(1500);
  };

  // This struct is used to provide constraints for setting up and running
  // remoting streams. These properties are based on the current control
  // protocol and allow remoting with current senders.
  struct RemotingConstraints {
    RemotingConstraints();
    ~RemotingConstraints();

    RemotingConstraints(RemotingConstraints&&) noexcept;
    RemotingConstraints(const RemotingConstraints&);
    RemotingConstraints& operator=(RemotingConstraints&&) noexcept;
    RemotingConstraints& operator=(const RemotingConstraints&);

    // Current remoting senders assume that the receiver supports 4K for all
    // video codecs supplied in |video_codecs|, or none of them.
    bool supports_4k = false;

    // Current remoting senders take an "all or nothing" support for audio
    // codec support. While Opus and AAC support is handled in our Constraints'
    // |audio_codecs| property, support for the following codecs must be
    // enabled or disabled all together in the underlying remoting protocol. In
    // the event that some of these codecs are marked as supported and others
    // are not, then remoting requests will be accepted for unsupported codecs
    // and then the remoting session will fall back to mirroring.
    bool supports_ogg_vorbis = true;

    bool supports_flac = true;

    // Includes Mu-Law, S16BE, S24BE, and ALAW variants.
    bool supports_pcm = true;

    bool supports_mp3 = false;

    bool supports_mpegh = false;

    // Includes narrow band (NB) and wide band (WB) variants.
    bool supports_amr = false;

    // GSM Mobile Station (MS).
    bool supports_gsm = false;

    // Apple Lossless.
    bool supports_alac = false;

    // Dolby Digital Plus.
    bool supports_eac3 = false;

    // Dolby Digital.
    bool supports_ac3 = false;

    // Dolby True Sound. Includes DTS XP2 and DTSE
    bool supports_dts = false;
  };

  ReceiverConfig();
  ReceiverConfig(std::vector<media::VideoCodec> video_codecs,
                 std::vector<media::AudioCodec> audio_codecs);
  ReceiverConfig(std::vector<media::VideoCodec> video_codecs,
                 std::vector<media::AudioCodec> audio_codecs,
                 std::vector<AudioLimits> audio_limits,
                 std::vector<VideoLimits> video_limits,
                 std::optional<Display> description);
  ~ReceiverConfig();

  ReceiverConfig(ReceiverConfig&&) noexcept;
  ReceiverConfig(const ReceiverConfig&);
  ReceiverConfig& operator=(ReceiverConfig&&) noexcept;
  ReceiverConfig& operator=(const ReceiverConfig&);

  // Audio and video codec constraints. Should be supplied in order of
  // preference, e.g. if `video_codecs` has vp8 and h.264 in that order, we will
  // generally select a vp8 offer over an h.264 offer. If a codec is omitted
  // from these fields it will never be selected in the OFFER/ANSWER
  // negotiation.
  std::vector<media::VideoCodec> video_codecs;
  std::vector<media::AudioCodec> audio_codecs;

  // Optional limitation fields that help the sender provide a delightful Cast
  // Streaming experience. Although optional, highly recommended.  NOTE:
  // embedders that wish to apply the same limits for all codecs can pass a
  // vector of size 1 with the |codec| field empty.
  std::vector<AudioLimits> audio_limits;
  std::vector<VideoLimits> video_limits;
  std::optional<Display> display_description;

  // Libcast remoting support is opt-in: embedders wishing to field remoting
  // offers may provide a set of remoting constraints, or leave nullptr for all
  // remoting OFFERs to be rejected in favor of continuing streaming.
  std::optional<RemotingConstraints> remoting;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_RECEIVER_CONFIG_H_
