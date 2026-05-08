# Cast Mirroring Component (`//components/mirroring`)

This component provides the service implementation and browser integration for
Cast Mirroring.

The Mirroring Service runs in its own sandboxed utility process. It uses Mojo
message pipes to communicate with the privileged browser process to:

* Acquire media inputs, such as screen capture video or tab audio capture.
* Communicate with remote Cast devices using Cast Channel messaging (see
  [`//components/cast_channel`](../cast_channel/)).
* Open UDP network sockets for Cast Streaming packets.
* Switch between screen mirroring and content remoting modes.

The Service encapsulates all session-management logic and interfaces with
[`//media/cast`](../../media/cast/) to encode and packetize media in real-time.
Through the [`ResourceProvider`](mojom/resource_provider.mojom) Mojo interface,
it requests necessary resources (like GPU access, video/audio capture streams,
and network contexts) from the browser process.

*** note
For details on the Cast Streaming session protocol, see the
[Cast Streaming Specification](../../third_party/openscreen/src/cast/streaming/README.md)
in the Open Screen library.
***

## Directory Breakdown

* `browser/`: Browser-side implementation and integration. Additional browser
  integration code can be found in
  [`//chrome/browser/media/cast_mirroring_service_host.h`](../../chrome/browser/media/cast_mirroring_service_host.h)
  and [`//chrome/browser/media/router/providers/cast/`](../../chrome/browser/media/router/providers/cast/).

* `mojom/`: Mojo interfaces for communication between the Mirroring Service and
  the browser process (e.g.,
  [`MirroringService`](mojom/mirroring_service.mojom),
  [`ResourceProvider`](mojom/resource_provider.mojom),
  [`SessionObserver`](mojom/session_observer.mojom)).

* `service/`: The core Mirroring Service implementation. This directory contains
  all logic for session management, media capturing, content remoting, and
  packetization. See the [**Service README**](service/README.md) for detailed
  information on internal data flow, session negotiation, and core classes.
