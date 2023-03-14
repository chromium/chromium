// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_COMMON_FRAME_DEMUXER_STREAM_TRAITS_H_
#define COMPONENTS_CAST_STREAMING_COMMON_FRAME_DEMUXER_STREAM_TRAITS_H_

#include <type_traits>

namespace cast_streaming {

// Helper class to wrap all type deductions needed for demuxer stream support in
// the cast streaming component.
//
// |TMojoReceiverType| is the mojom interface used for requesting data buffers.
// Currently expected to be either AudioBufferRequester or VideoBufferRequester.
template <typename TMojoApiType>
class DemuxerStreamTraits {
 private:
  // Helper class to extract a function parameter type.
  template <typename TFunctorType>
  struct ArgumentExtractor;

  template <typename R, typename Arg>
  struct ArgumentExtractor<R(Arg)> {
    typedef Arg type;
  };

 public:
  // Deduce the response type to a TMojoReceiverType::GetBuffer() call. Either
  // GetAudioBufferResponse or GetVideoBufferResponse.
  typedef typename ArgumentExtractor<
      typename TMojoApiType::GetBufferCallback::RunType>::type
      GetBufferResponseType;

  // Deduce the StreamInfo type used in the union GetBufferResponseType. Either
  // AudioStreamInfo or VideoStreamInfo.
  typedef typename std::remove_reference<
      decltype(std::declval<typename GetBufferResponseType::element_type>()
                   .get_stream_info())>::type StreamInfoType;

  // Deduce the Config type associated with this Mojo API. Either
  // media::AudioDecoderConfig or media::VideoDecoderConfig.
  typedef decltype(StreamInfoType::element_type::decoder_config) ConfigType;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_COMMON_FRAME_DEMUXER_STREAM_TRAITS_H_
