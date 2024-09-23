# Background

The `cast_streaming` component provides a wrapper around all Cast mirroring and
Cast remoting receiver functionality in a platform-agnostic way. The Cast
Streaming transport/protocol is
[implemented](https://source.chromium.org/chromium/chromium/src/+/main:third_party/openscreen/src/cast/streaming/README.md;l=1)
by the
[Openscreen library](https://source.chromium.org/chromium/chromium/src/+/main:third_party/openscreen/src/README.md),
and media rendering is handled by the Chromium media stack, so this component
acts as an intermediary between the two. It is currently used both as part of
Fuchsia WebEngine and the `//components/cast_receiver` Chromecast
implementation, which can be used to run a Cast Streaming receiver on Linux.

This component does NOT provide any Cast sender support, although some code in
`//media/cast/openscreen` is shared with the sender.

# Using the Component with `//content`

### The following flags are required when building with the `cast_streaming` component:

```
enable_cast_receiver = true
cast_streaming_enable_remoting = true
```

### Browser Process

Starting the `cast_streaming` component in the browser process has two parts:

1. Calling
[`SetNetworkContextGetter()`](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/public/network_context_getter.h;l=26;drc=12be03159fe22cd4ef291e9561762531c2589539)
to set the network context to use for the streaming session.
2. [Creating](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/public/receiver_session.h;l=76;drc=7eb26cecf3a3c92e25c68b8ca4f0fc467ea89af7)
a new `ReceiverSession` object and calling
`ReceiverSession::StartStreamingAsync()` on it.

### Renderer Process

In the `ContentRendererClient` for the service, a
[new instance](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/public/resource_provider_factory.h;l=14;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd)
of `cast_streaming::ResourceProvider` must be
[returned](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/cast_core/runtime/renderer/cast_runtime_content_renderer_client.cc;l=18;drc=ed2ae87b412008e5d52766ab1c8bbc96203f052b)
by `CreateCastStreamingResourceProvider()`
[function](https://source.chromium.org/chromium/chromium/src/+/main:content/public/renderer/content_renderer_client.h;l=430;drc=adac219925ef5d9c9a954d189c2e4b8852a4bbed)
overload.

The remainder of integration is already taken care of in the `//content` layer.

# Code Breakdown

The code for this component can roughly be broken down into a few parts, as
reflected by the component’s directory structure:

### Frame

This section of code is responsible for sending frame data from Openscreen to
the media pipeline. This is located in `/browser/frame`, `/renderer/frame`, and
`/common/frame`.

### Control

This section of code is responsible for sending `media::Renderer` commands to
the embedder-specific Renderer on top of which `cast_streaming` is running. It
is located at `/browser/control`, `/renderer/control`, and `/common/control`.
Selection of this `Renderer` is as with vanilla Chromium - through the
[`MediaFactory`](https://source.chromium.org/chromium/chromium/src/+/main:content/renderer/media/media_factory.cc;l=547;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24).
No `cast_streaming`-specific changes are required.

### Remoting

A subset of Control which is responsible for translating control commands to and
from the `cast_streaming` media remoting protocol, a proto-based communication.
Code is located within the `/control` directory, e.g
 `/browser/control/remoting`.

### Web Codecs

An alternative implementation of the Renderer-process side of the frame
implementation. This section is minimal, as its implementation has largely been
integrated into the standard Frame flow to avoid code duplication.

# Initialization Flow

In the diagrams below, note the following:

*   Blue boxes represent external code (e.g. Openscreen, Embedder-controlled infra)
*   Yellow boxes are used to represent the Renderer-process classes
*   White boxes represent the Browser-process classes
*   Gray boxes represent remoting-only classes
*   Purple lines represent mojo calls


## Starting a Cast session

The startup process for the `cast_streaming` component can most easily be
visualized by splitting up the Browser and Renderer processes. Very few
communications are made between the two, so they can largely be viewed
independently.

### Browser Process

On the browser side

1. `ReceiverSessionImpl`
[creates](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/receiver_session_impl.h;l=109;drc=7eb26cecf3a3c92e25c68b8ca4f0fc467ea89af7)
and starts a `CastStreamingSession`, which in turn
[creates](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/cast_streaming_session.cc;l=98;drc=821de11ab399e78d0b8d4894ec07fe6d306cd896)
a` PlaybackCommandDispatcher` and
[starts an `openscreen::cast::ReceiverSession`](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/cast_streaming_session.cc;l=93;drc=821de11ab399e78d0b8d4894ec07fe6d306cd896).
2. The `openscreen::ReceiverSession`
[negotiates](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/cast_streaming_session.cc;l=360;drc=821de11ab399e78d0b8d4894ec07fe6d306cd896)
a session and passes it to `CastStreamingSession`, If it’s a remoting session,
wait for the “real” configs to be sent from the sender side.
3. In either case, `CastStreamingSession::StartStreamingSession()` then
[creates the remainder](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/cast_streaming_session.cc;l=314;drc=821de11ab399e78d0b8d4894ec07fe6d306cd896)
of the requisite objects.
4. Then, the first frame is “preloaded”. The first frame of data is
[read](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/frame/demuxer_stream_data_provider.h;l=137;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
and its timestamp is used to call `StartPlayingFrom()` on the associated
`media::Renderer` instance in the renderer process. In the case of remoting,
more frames must also be
[requested](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/control/remoting/rpc_demuxer_stream_handler.cc;l=275;drc=821de11ab399e78d0b8d4894ec07fe6d306cd896)
from the remote renderer running on the streaming sender.

![Cast Streaming initialization flow for browser process](/docs/images/cast_streaming_init_browser.png "browser process")

The files involved in this section are located mainly at the top-level of each
directory (i.e. `//components/cast_streaming/browser` or
`//components/cast_streaming/renderer`), which make calls into frame and control
as necessary.

### Renderer Process

Starting of the Control and Frame pathways in the Renderer process is entirely
separate, so the two can be examined separately:

For the former of these, the setup is triggered by the `MediaFactory` which owns
the
[root-level singleton](https://source.chromium.org/chromium/chromium/src/+/main:content/renderer/media/media_factory.h;l=200;drc=e672a665ffa8fe4901184f03922e2cc548399da5)
objects responsible for enabling the flow. In order to create a
`media::Renderer` mojo connection between the browser and renderer processes,
`ResourceProviderImpl` is
[used as an intermediary](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/resource_provider_impl.h;l=44;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
to avoid timing issues. The receiver side of this pipe is
[passed](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/control/playback_command_forwarding_renderer_factory.cc;l=14;drc=9b95f32e6c0fe938fdec7dd800358619d4103ba1)
to the `PlaybackCommandForwardingRenderer` during its creation, which will
[act in response to](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/control/playback_command_forwarding_renderer.cc;l=58;drc=9b95f32e6c0fe938fdec7dd800358619d4103ba1)
these commands by forwarding them to the underlying embedder-specific
`media::Renderer` instance, as well as
[forwarding back](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/control/playback_command_forwarding_renderer.cc;l=257;drc=9b95f32e6c0fe938fdec7dd800358619d4103ba1)
any `RendererClient` events.

![Cast Streaming initialization flow for renderer process control channel](/docs/images/cast_streaming_init_renderer_control.png "renderer process control channel")


Creation of the Frame channel is slightly more complex. First, an override from
the embedder-specific `ContentBrowserClient` triggers usage of the
`FrameInjectingDemuxer`. The renderer-process being “ready” is signified by both
of:

*   [Receipt of a call](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/frame/demuxer_connector.cc;l=103;drc=9b95f32e6c0fe938fdec7dd800358619d4103ba1)
by the `DemuxerConnector`
*   [Initialization](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/frame/demuxer_connector.cc;l=34;drc=9b95f32e6c0fe938fdec7dd800358619d4103ba1)
of the `FrameInjectingDemuxer`

At that point, the browser process will
[send](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/receiver_session_impl.cc;l=137;drc=7eb26cecf3a3c92e25c68b8ca4f0fc467ea89af7)
the `OnStreamsInitialized()` call which
[provides the connection information](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/frame/demuxer_connector.cc;l=115;drc=9b95f32e6c0fe938fdec7dd800358619d4103ba1)
to create all remaining objects and begin pulling frames from the browser
process.

![Cast Streaming initialization flow for renderer process frame channel](/docs/images/cast_streaming_init_renderer_frame.png "renderer process frame channel")

# Important Scenarios

## Streaming Frame Data

At a high level, sending frame data to the media pipeline in the renderer
process works as follows:

1. Following a `DemuxerStream::Read()`
[call](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/frame/frame_injecting_demuxer.cc;l=153;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24),
the `FrameInjectingDemuxerStream`
[triggers](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/common/buffer_requester.h;l=89;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
a `GetBuffer()` mojo call.
2. The `DemuxerStreamDataProvider`
[receives this call](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/frame/demuxer_stream_data_provider.h;l=165;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24),
and
[makes a request](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/frame/demuxer_stream_data_provider.h;l=193;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
to the `StreamConsumer` to get a frame.
3. If none exist,
[wait for them](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/frame/stream_consumer.cc;l=156;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24).
In the remoting case, also
[send a notification](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/frame/stream_consumer.cc;l=158;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
to the remoting sender to have more frames sent
[through](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/control/remoting/rpc_demuxer_stream_handler.cc;l=275;drc=821de11ab399e78d0b8d4894ec07fe6d306cd896)
the `RpcDemuxerStreamHandler`.
4. Once frames are received, parse the `DecoderBuffer`, write the `data()` field
to a pipe and return the remainder to the `DemuxerStreamDataProvider`.
5. The `DemuxerStreamDataProvider` receives and then
[sends this frame](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/frame/demuxer_stream_data_provider.h;l=119;drc=790df4d5983e38ad1d1d00fbc10ef941070eed24)
data to the Renderer, where it gets
[combined](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/public/decoder_buffer_reader.cc;l=76;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03)
back with its data and
[provided](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/public/decoder_buffer_reader.cc;l=59;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03)
to the `FrameInjectingDemuxerStream`.

![Cast Streaming frame playback scenario](/docs/images/cast_streaming_frame_playback_flow.png "frame playback")

## Changing of the `AudioDecoderConfig` / `VideoDecoderConfig`

A config change can be triggered in two ways:

*   Changing of the config
[during an ongoing Remoting session](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/control/remoting/rpc_demuxer_stream_handler.cc;l=230;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03;bpv=0;bpt=0).
*   [Re-configuration](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/cast_streaming_session.cc;l=333;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03?q=cast_Streaming_session.cc&ss=chromium%2Fchromium%2Fsrc)
of the streaming session.

### Changing of the config during an ongoing Remoting session

Changing the config during an ongoing remoting session occurs in a number of
steps:

1. Steps 1-3 above send a request to the remoting sender to send more data.
2. Instead of (or in addition to) frames, a new `AudioDecoderConfig` /
`VideoDecoderConfig` are [sent](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/control/remoting/rpc_demuxer_stream_handler.cc;l=230;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03;bpv=0;bpt=0).
3. The receiver will then “reset its state” by:
    1. [Stopping](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/frame/demuxer_stream_data_provider.h;l=86;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03?q=demuxer_stream_data_provider.h&ss=chromium%2Fchromium%2Fsrc)
    all flowing data (so nothing incorrect gets displayed).
    2. [Flushing](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/cast_streaming_session.cc;l=375;bpv=0;bpt=0)
    any data still in the media pipeline.
    3. [Clearing](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/control/playback_command_dispatcher.cc;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03;bpv=0;bpt=0;l=115)
    any remoting state.
    4. [Re-initializing](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/cast_streaming_session.cc;l=234;bpv=0;bpt=0)
    the streaming session with `StartStreamingSession()`.
4. The pre-load flow will
[again be used](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/receiver_session_impl.cc;l=214;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03;bpv=0;bpt=0?q=receiver_session_impl&ss=chromium%2Fchromium%2Fsrc)
to start playback of the streaming session. In an ideal world, it would be safe
to wait for a `StartPlayingFrom()` call from the remote sender, but in practice
that will only be sent intermittently and cannot be relied on.
5. The `DemuxerStreamDataProvider` will
[send the new config](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/frame/demuxer_stream_data_provider.h;l=75;drc=c65a603f7748a9f09e8740ab3f25a8ae00077a03;bpv=0;bpt=0)
to the Renderer process.
6. The `FrameInjectingDemuxerStream` will
[receive a new](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/renderer/frame/frame_injecting_demuxer.cc;l=129;bpv=0;bpt=0) `StreamConsumer`
associated with this new stream, and may then will return the config as part of
the next (or ongoing) `Read()` call.

![Cast Streaming new config scenario](/docs/images/cast_streaming_new_config_flow.png "new config")

### Reconfiguration of the mirroring session

This situation occurs either when an ongoing mirroring session re-configures
itself (e.g. as the result of Chrome changing the quality of an ongoing session)
or when the user changes between mirroring and remoting.

As with previous sections, this scenario is largely the same as the remoting
section, except that instead of a new config being received through the
`ReadUntil()` call, the `OnNegotiated()` function will
[immediately provide the
new config](https://source.chromium.org/chromium/chromium/src/+/main:components/cast_streaming/browser/cast_streaming_session.cc;l=336;bpv=0;bpt=0) and “reset the
state” of the pipeline.

## Web Codecs Receiver

TODO(crbug.com/40765922): Add these details

# Appendix

## Media Pipeline Object Lifetimes (Renderer Process)

In the Renderer process, the same `media::DemuxerStream` and `media::Renderer`
instances are used, even when the stream is re-initialized. Rather than
re-creating the entire pipeline, `Flush()` and `StartPlayingFrom()` commands are
sent and the same instances are used.

## Why Preloading?

Preloading was a concept added relatively late into the development of the
`cast_streaming` component to solve a number of edge cases:

*   When mirroring, the sender does not always start from `pts = 0 ms`. In such
cases, naively calling `StartPlayingFrom(0 ms)` as has historically been
[relied upon](https://source.chromium.org/chromium/chromium/src/+/main:fuchsia_web/cast_streaming/data/receiver.html;l=40)
in WebEngine does not work.
*   When remoting, a `StartPlayingFrom()` command is not always sent.

In order to account for such cases, a `StartPlayingFrom()` must be “injected
in”, for which the timestamp of the first frame is required. It is also true
that this approach decreases the playback delay between the sender and receiver,
but that is more of a happy coincidence than the original goal of this workflow.
