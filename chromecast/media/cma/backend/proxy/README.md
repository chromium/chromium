# Objective

As part of supporting a Chromium runtime for chromecast (as defined below), the
`CmaBackendProxy` class and related code contained in the
[`chromecast/media/cma/backend/proxy/`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/backend/proxy/)
directory was added. This infrastructure exists to proxy audio data across the
CastRuntimeAudioChannel gRPC Service without impacting local playback of video
or audio media, and requires significant complexity in order to do so. This
document provides a high-level overview of the `CmaBackendProxy` implementation
and the pipeline into which it calls. It is intended to be used as a reference
for understanding this pipeline, to simplify both the code review and the
onboarding process.

# Background

## Existing Chromecast Media Pipeline

The chromecast pipeline, as exists today in Chromium's
[`chromecast/`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/)
directory, is rather complex, so only the high-level will be discussed here.
When playing out media, an implementation of the
[`MediaPipelineBackend`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/public/media/media_pipeline_backend.h;l=36?q=mediapipelinebackend&ss=chromium%2Fchromium%2Fsrc:chromecast%2F)
API will be created, and this will be wrapped by a platform-specific (e.g.
Android, Fuchsia, Desktop, etc…)
[`CmaBackend`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/api/cma_backend.h;l=24?q=cmabackend&sq=&ss=chromium%2Fchromium%2Fsrc:chromecast%2F)
as
[exists today](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/backend/)
in the Chromium repo. The `CmaBackend` will create an `AudioDecoder` and
`VideoDecoder` instance as needed, which is responsible for operations such as
playback control, queueing up media playout, and decrypting DRM (if needed),
then passing these commands to the `MediaPipelineBackend`.

## gRPC

[gRPC](https://grpc.io/) is a modern open source high performance RPC framework
that can run in any environment. It can efficiently connect services in and
across data centers with pluggable support for load balancing, tracing, health
checking and authentication. gRPC was chosen as the RPC framework to use for the
Chromium runtime to simplify the upgrade story for when both sides of the RPC
channel can upgrade independently.

## Chromium Runtime

A runtime to be used by hardware devices to play out audio and video data as
received from a Cast session. It is expected to run on an embedded device.

# High Level Architecture

At a high level, the `CmaBackendProxy` ("proxy backend")  sits on top of another
`CmaBackend`, which for the purposes of this document will be referred to as the
delegated backend. The proxy backend acts to send all audio and video data to
the delegated backend on which it sits, while also sending the same audio to the
CastRuntimeAudioChannel gRPC Channel used to proxy data, along with
synchronization data to keep playback between the two in-line.
The architecture used for this pipeline can be seen as
[follows](https://charleshan.users.x20web.corp.google.com/www/nomnoml/nomnoml/index.html#view/%5B%3Cabstract%3ECmaBackend%7C%0A%20%20CreateAudioDecoder()%3B%0A%20%20CreateVideoDecoder()%3B%0A%20%20Initialize()%3B%0A%20%20Start(int64_t%20start_pts)%3B%0A%20%20Stop()%3B%0A%20%20Pause()%3B%0A%20%20Resume()%3B%0A%20%20GetCurrentPts()%3B%0A%20%20SetPlaybackRate()%5D%0A%5B%3Cabstract%3ECmaBackend%3A%3AAudioDecoder%7C%0A%20%20SetDelegate()%3B%0A%20%20PushBuffer()%3B%0A%20%20SetConfig()%3B%0A%20%20SetVolume()%3B%0A%20%20GetRenderingDelay()%3B%0A%20%20GetStatistics()%3B%0A%20%20RequiresDecryption()%3B%0A%20%20SetObserver()%5D%0A%5B%3Cabstract%3ECmaBackend%3A%3ADecoder%3A%3ADelegate%7C%0A%20%20OnPushBufferComplete()%3B%0A%20%20OnEndOfStream()%3B%0A%20%20OnDecoderError()%3B%0A%20%20OnKeyStatusChanged()%3B%0A%20%20OnVideoResolutionChanged()%5D%0A%20%20%0A%5BAudioDecoderPipelineNode%5D%0A%5BCmaBackendProxy%5D%0A%5BBufferIdManager%7C%0A%20%20AssignBufferId()%3B%0A%20%20GetCurrentlyProcessingBufferInfo()%3B%0A%20%20UpdateAndGetCurrentlyProcessingBufferInfo()%7C%0A%20%20%5BBufferId%5D%3B%0A%20%20%5BTargetBufferInfo%7Cbuffer_id%3A%20BufferId%3B%20timestamp_micros%3A%20int64_t%5D%5D%0A%5B%3Cabstract%3ECmaProxyHandler%7C%0A%20%20Initialize()%3B%0A%20%20Start()%3B%0A%20%20Stop()%3B%0A%20%20Pause()%3B%0A%20%20Resume()%3B%0A%20%20SetPlaybackRate()%3B%0A%20%20SetVolume()%3B%0A%20%20SetConfig()%3B%0A%20%20PushBuffer()%5D%0A%5BProxyCallTranslator%5D%0A%5B%3Cabstract%3EMultizoneAudioDecoderProxy%7C%0A%20%20Initialize()%3B%0A%20%20Start()%3B%0A%20%20Stop()%3B%0A%20%20Pause()%3B%0A%20%20Resume()%3B%0A%20%20SetPlaybackRate()%3B%0A%20%20LogicalPause()%3B%0A%20%20LogicalResume()%3B%0A%20%20GetCurrentPts()%20const%5D%0A%5BMultizoneAudioDecoderProxyImpl%5D%0A%5BMediaPipelineBufferExtension%5D%0A%5B%3Cabstract%3ECastRuntimeAudioChannelBroker%7C%0A%20%20InitializeAsync()%3B%0A%20%20SetVolumeAsync()%3B%0A%20%20SetPlaybackAsync()%3B%0A%20%20GetMediaTimeAsync()%3B%0A%20%20StartAsync()%3B%0A%20%20StopAsync()%3B%0A%20%20PauseAsync()%3B%0A%20%20ResumeAsync()%5D%0A%5B%3Cabstract%3EAudioChannelPushBufferHandler%7C%0A%20%20PushBuffer()%3B%0A%20%20HasBufferedData()%20const%3B%0A%20%20GetBufferedData()%5D%0A%5BPushBufferQueue%5D%0A%5BPushBufferPendingHandler%5D%0A%5BCastRuntimeAudioChannelEndpointManager%7C%0A%20%20GetAudioChannelEndpoint()%5D%0A%0A%5BCmaBackend%5D%3C%3A--1%5BCmaBackendProxy%5D%0A%5BCmaBackendProxy%5D1-%5BMultizoneAudioDecoderProxy%5D%0A%5BCmaBackend%3A%3AAudioDecoder%5D%3C%3A--%5BAudioDecoderPipelineNode%5D%0A%5BCmaBackend%3A%3ADecoder%3A%3ADelegate%5D%3C%3A--%5BAudioDecoderPipelineNode%5D%0A%5BAudioDecoderPipelineNode%5D%3C%3A--%5BMediaPipelineBufferExtension%5D%0A%5BAudioDecoderPipelineNode%5D%3C%3A--%5BMultizoneAudioDecoderProxy%5D%0A%5BMultizoneAudioDecoderProxy%5D%3C%3A--%5BMultizoneAudioDecoderProxyImpl%5D%0A%5BMultizoneAudioDecoderProxyImpl%5D1-%5BCmaProxyHandler%5D%0A%5BMultizoneAudioDecoderProxyImpl%5D1-%5BBufferIdManager%5D%0A%5BCmaProxyHandler%5D%3C%3A--%5BProxyCallTranslator%5D%0A%5BProxyCallTranslator%5D1-%5BCastRuntimeAudioChannelBroker%5D%0A%5BAudioChannelPushBufferHandler%5D%3C%3A-%5BPushBufferPendingHandler%5D%0A%5BAudioChannelPushBufferHandler%5D%3C%3A-%5BPushBufferQueue%5D%0A%5BProxyCallTranslator%5D1-%5BPushBufferPendingHandler%5D%0A%5BPushBufferPendingHandler%5D1-%5BPushBufferQueue%5D%0A%5BMultizoneAudioDecoderProxyImpl%5D1-%5BMediaPipelineBufferExtension%5D%0A%5BCmaBackend%3A%3AAudioDecoder%5D-1%5BMediaPipelineBufferExtension%5D%0A%5BCastRuntimeAudioChannelBroker%5D%3C%3A--%5BAudioChannelBrokerImpl%5D%0A%5BAudioChannelBrokerImpl%5D%3C%3A--%5BCastRuntimeAudioChannelEndpointManager%5D%0A):

![image](https://services.google.com/fh/files/misc/cmabackendproxy_infra.png)

# Implementation Specifics

The pipeline can be roughly divided into two parts:

-  Local Media Playout, or how the `CmaBackendProxy` gets audio and video
   data it receives to the platform-specific `MediaPipelineBackend`, resulting
   in playout of the local device's speakers .
-  Audio Data Proxying, or how the audio data is streamed across the
   [CastRuntimeAudioChannel](https://source.chromium.org/chromium/chromium/src/+/main:third_party/cast_core/public/src/proto/runtime/cast_audio_decoder_service.proto;l=231?q=castruntimeaudiochannel&sq=&ss=chromium%2Fchromium%2Fsrc)
   gRPC Service in parallel to the local playout,

Both can be summarized in this
[diagram](https://sequencediagram.googleplex.com/view/5704799657918464), with
further details to follow:

![image](https://services.google.com/fh/files/misc/cmabackendproxy_pushbuffer_flow.png)

## Local Media Playout

The `CmaBackendProxy`, by virtue of being a `BackendProxy`, can create an
AudioDecoder and a VideoDecoder. VideoDecoder creation and all video processing
is delegated to the underlying delegated backend with no changes, so it will not
be discussed further. The AudioDecoder is more interesting - there are 3
`CmaBackend::AudioDecoder` types relevant in this architecture through which
audio data will pass - two `proxy` specific and the delegated backend's decoder.
In processing order, they are:

1. [`MultizoneAudioDecoderProxy`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy.h):
   This class is the starting-point for sending data over the
   CastRuntimeAudioChannel gRPC, using the pipeline described in the [following
   section](#heading=h.gkcc76texr45).
1. `MediaPipelineBufferExtension`: This class performs no processing itself,
   but instead just acts to store the data from PushBuffer calls locally, so
   that an extra few seconds of data may be queued up in addition to what the
   local decoder stores.
1. _Delegated backend's audio decoder: _The `CmaBackend::AudioDecoder` for
   this specific platform, as described above. This is owned by the Delegated
   backend itself.

All audio data passes through these three layers so that it may be played out
locally. For local playback as described here, this functionality is for the
most part uninteresting - by design, the end user should notice no difference in
audio playout. At an architecture level, the only noticeable difference for an
embedder is that extra data is queued locally in the
`MediaPipelineBufferExtension`.

As an abstraction on top of the above, the `AudioDecoderPipelineNode` was
introduced as a parent for `MultizoneAudioDecoderProxy` and
`MediaPipelineBufferExtension`, though its existence can for the most part be
ignored. All functions in this base class act as pass-through methods, directly
calling into the lower layer's method of the same name. Meaning that this class
functions purely for software-engineering reasons, as a way to reduce code
complexity of the child classes.

## Audio Data Proxying

The `MultizoneAudioDecoderProxy`, as mentioned above, functions to proxy data
across the CastRuntimeAudioChannel gRPC channel. The pipeline used to do uses
the following classes,  in order:

1. [`MultizoneAudioDecoderProxyImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy_impl.h):
   As mentioned above, all `CmaBackend::AudioDecoder` methods call into here
   first, as well as methods corresponding to the public calls on `CmaBackend`.
   This class mostly acts as a pass-through, immediately calling into the below
   for all above method calls, although the `BufferIdManager` owned by this
   class adds a bit of complexity, in that it assigns Buffer Ids to PushBuffer
   calls, to be used for audio timing synchronization between both sides of the
   CastRuntimeAudioChannel gRPC channel.
1. [`ProxyCallTranslator`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/backend/proxy/proxy_call_translator.h):
   As the name suggests, this class exists to convert between the
   media-pipeline understood types, as called into the above layer, with those
   supported by CastRuntimeAudioChannel gRPC.To this end, `PushBufferQueue`
   helps to turn the "Push" model used by the `CmaBackend::AudioDecoder`'s
   `PushBuffer()` method into a "Pull" model that better aligns with the gRPC
   async model. The `PendingPushBufferHelper` works with the above, to support
   returning a `kBufferPending` response as required by the `PushBuffer()`
   method's contract.  In addition, this class is also responsible for handling
   threading assumptions made by the above and below classes, such as how data
   returned by the below layer must be processed on the correct thread in the
   above layer.
1. [`CastRuntimeAudioChannelBroker`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_broker.h):
   Implementations of this abstract class take the CastRuntimeAudioChannel gRPC
   types supplied by the above, then use them to make calls against the gRPC
   Service.

# Configuring the Pipeline

In order to support varied user scenarios, a number of build flags have been
introduced to customize the functionality of this pipeline:

#### enable_chromium_runtime_cast_renderer

This flag enables or disables support for sending data across the
CastRuntimeAudioChannel. When enabled, the
[`CmaBackendFactoryImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/backend/cma_backend_factory_impl.cc;l=32)
class will wrap the implementation-specific `CmaBackend` with a
`CmaBackendProxy` instance, as described above. This flag defaults to `False`.
