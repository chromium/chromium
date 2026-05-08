# Mirroring Service Implementation (`//components/mirroring/service`)

This directory contains the internal implementation of the Cast Mirroring
Service.

This service is designed to run in a sandboxed utility process. It handles the
heavy lifting of Cast streaming: session negotiation, capturing media from the
browser, encoding, packetizing, and transmitting it to a Cast receiver.

## Key Assumptions & Invariants

* **Sandboxing & Resources:** Because this code runs in a sandboxed utility
  process (specifically `kMirroringSandbox` or `kHardwareVideoEncoding`), it
  **cannot** make direct system calls to open sockets, capture screens, or
  access the GPU. All external resources must be requested asynchronously over
  Mojo via the [`mojom::ResourceProvider`](../mojom/resource_provider.mojom)
  interface (implemented by the browser process).

* **Threading/Sequences:** The service operates primarily on a single main
  sequence. The `SEQUENCE_CHECKER` macro is heavily used to enforce this. The
  network and media encoding may utilize background threads or the
  `io_task_runner`, but state mutations and Mojo message handling must happen
  on the main sequence.

* **Open Screen Environment:** The service bridges Chromium's
  [`base::TaskRunner`](../../../base/task/task_runner.h) and
  [`base::Time`](../../../base/time/time.h) into the
  [`openscreen::cast::Environment`](../../../third_party/openscreen/src/cast/streaming/public/environment.h)
  required by the Open Screen library. This bridging is implemented by the
  Chromium-wide [`//components/openscreen_platform/`](../../openscreen_platform/)
  component and is instantiated during session creation in
  [`OpenscreenSessionHost`](openscreen_session_host.h).

## Session Negotiation (OFFER/ANSWER Exchange)

The Mirroring Service manages the Cast Streaming session negotiation, relying
heavily on the [Open Screen library](../../../third_party/openscreen/src/README.md)
which implements the underlying Cast protocols and the OFFER/ANSWER exchange
mechanisms.

To prioritize performance and power efficiency, the Mirroring Service may send
up to two offers when establishing a session:

1. **Hardware-First Offer:** The service initially sends an `OFFER` containing
   only hardware-accelerated video codecs.

2. **Software Fallback:** If the receiver rejects this initial offer (indicated
   by a `kNoStreamSelected` error from Open Screen), the service gracefully
   falls back by sending a second `OFFER` that includes software-based video
   codecs (e.g., software VP8 or VP9).

*** note
This two-stage negotiation ensures hardware encoding is prioritized while
maintaining compatibility with receivers that might only support specific
software codecs.
***

## Places to Start (Core Classes)

If you are new to the Mirroring Service, these are the key classes to
understand, in order of execution:

1. [`MirroringService`](mirroring_service.h)
   * **The Front Door:** This is the implementation of the
     [`mojom::MirroringService`](../mojom/mirroring_service.mojom) interface.
     It is the entry point that the browser process calls to `Start()` a session.
     It primarily manages the lifecycle of the
     [`OpenscreenSessionHost`](openscreen_session_host.h).

2. [`OpenscreenSessionHost`](openscreen_session_host.h)
   * **The Brain:** This class orchestrates the entire session. It creates the
     [`openscreen::cast::SenderSession`](../../../third_party/openscreen/src/cast/streaming/public/sender_session.h),
     handles the two-stage OFFER/ANSWER negotiation, sets up the network via
     Mojo, and wires up the capture/audio sources to the RTP streams. It uses a
     relatively simple state model, transitioning between `kInitializing`,
     `kMirroring`, `kRemoting`, and `kStopped`.

3. [`VideoCaptureClient`](video_capture_client.h) &
   [`CapturedAudioInput`](captured_audio_input.h)
   * **The Ingest:** These classes talk to the browser via Mojo to receive raw
     [`media::VideoFrame`](../../../media/base/video_frame.h) and audio PCM
     data from the user's screen or tab.

4. [`RtpStream`](rtp_stream.h)
   * **The Pipeline:** Wraps a
     [`media::cast::VideoSender`](../../../media/cast/sender/video_sender.h)
     (or `AudioSender`) and handles the actual encoding and pushing of media
     frames to the Open Screen
     [`SenderPacketRouter`](../../../third_party/openscreen/src/cast/streaming/sender_packet_router.h).

5. [`MediaRemoter`](media_remoter.h)
   * **The Alternate Path:** If a user plays a video (e.g., YouTube or Vimeo)
     in a mirrored tab, the session may switch from "Mirroring" (sending pixels)
     to "Remoting" (sending the URL and media controls). This class manages
     that transition and the resulting RPC communication.

6. [`mirroring_features.h`](mirroring_features.h)
   * **The Flags:** Contains the declarations for all `base::Feature` flags
     owned by the Mirroring Service. This is the central location for
     kill-switches and experimental feature toggles.

## Sub-systems and Data Flow

### The Network Flow

1. [`OpenscreenSessionHost`](openscreen_session_host.h) asks the browser's
   `ResourceProvider` for a
   [`network::mojom::NetworkContext`](../../../services/network/public/mojom/network_context.mojom).

2. It uses this to create a UDP socket.

3. This socket is wrapped in an
   [`openscreen::cast::Environment`](../../../third_party/openscreen/src/cast/streaming/public/environment.h)
   and passed into the Open Screen `SenderSession`.

### The Media Flow (Mirroring)

1. [`VideoCaptureClient`](video_capture_client.h) receives a raw
   [`media::VideoFrame`](../../../media/base/video_frame.h) from the browser.

2. The frame is passed into [`RtpStream`](rtp_stream.h).

3. `RtpStream` pushes it into
   [`media::cast::VideoSender`](../../../media/cast/sender/video_sender.h)
   (which encodes it via hardware or software).

4. The encoded frame is passed to the Open Screen
   [`cast::Sender`](../../../third_party/openscreen/src/cast/streaming/public/sender.h)
   to be packetized and sent over the UDP socket.

## Testing and Debugging

There are several ways to test changes to the Mirroring Service, ranging from
fast local unit tests to full end-to-end integration.

### 1. Unit Tests

The Mirroring Service has a comprehensive suite of unit tests located within
this directory (e.g.,
[`openscreen_session_host_unittest.cc`](openscreen_session_host_unittest.cc)).
These tests use mock browser interactions and mock network contexts to validate
the state machine without needing a real Cast device.

```bash
autoninja -C out/Default mirroring_unittests && ./out/Default/mirroring_unittests
```

### 2. Standalone Open Screen Receiver

For end-to-end local testing without a physical Chromecast, you can use the
Open Screen `cast_receiver` executable.

1. **Build the receiver** within your Chromium checkout:

   ```bash
   autoninja -C out/Default cast_receiver
   ```

2. **Generate a developer certificate** and start the receiver. Find a valid
   network interface (e.g., `eth0` or `en0`) and run:

   ```bash
   ./out/Default/cast_receiver -g <interface_name>
   ```

   *Note: This generates `generated_root_cast_receiver.crt` in your current
   directory and starts the receiver listening on that interface.*

3. **Launch Chrome** pointing to this developer certificate:

   ```bash
   ./out/Default/chrome --cast-developer-certificate-path=/path/to/generated_root_cast_receiver.crt
   ```

The `cast_receiver` will now appear as a Cast destination in Chrome's Cast
dialog, allowing you to mirror tabs or your desktop directly to the local
terminal window. See the
[Open Screen USING.md](../../../third_party/openscreen/src/cast/docs/USING.md)
document for more detailed instructions.

### 3. Production Cast Receiver

For final validation, test against a physical, production Chromecast or Google
Nest Hub device on the same local network. Start Chrome normally, select
"Cast..." from the "Cast, Save, and Share" context menu, and ensure the session
establishes and mirrors / remotes successfully with your changes.

### 4. Runtime Diagnostics & Logging

When debugging a dropped connection, stuttering frames, or a failed negotiation,
rely on the following Chromium tools:

#### Verbose Logging (VLOG)

The Cast and Mirroring components are heavily instrumented with `VLOG`. To see
these logs, launch Chrome from the command line with the following flags:

```bash
./out/Default/chrome --enable-logging=stderr --vmodule="*mirroring*=2,*cast*=2,*openscreen*=2"
```

This will print detailed connection states, packet drops, and bandwidth
estimates directly to your terminal.

#### Media Router Internals

Navigate to `chrome://media-router-internals` in your browser. This page is
crucial for Cast debugging. It contains:

* A **"Record Trace"** button, which is the easiest way to capture a performance
  trace pre-configured with the correct Cast, Mirroring, and Media categories.
* The exact **OFFER / ANSWER JSON payloads** exchanged during session
  negotiation (useful for verifying codec selection).
* The status of the Cast route and discovery information.
* An archive download of the media router logs.

#### Tracing (`chrome://tracing` or Perfetto)

If you are manually recording a performance trace for latency investigations or
dropped frames (instead of using the "Record Trace" button mentioned above),
ensure you capture the following specific categories:

* `media.cast`: High-level media encoding and pipeline states.
* `cast_perf_test`: Instant trace events for specific frame lifecycle milestones
  (e.g., when a frame is received from capture, encoded, and sent).
* `openscreen`: Network-level packetization, RTP/RTCP events, and pacing from
  the underlying Open Screen library.
* `media` and `gpu`: To investigate issues with hardware encoding or video
  capture.
