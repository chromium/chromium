# //components/mirroring

Service implementation and browser integration for Cast Mirroring.

The Mirroring Service is run in its own sandboxed process. It uses mojo
message pipes between its process and the privileged browser process to:

 * acquire inputs, such as screen capture video or tab audio capture.
 * communicate with remote Cast devices using Cast Channel messaging. See `../cast_channel/`.
 * open UDP network sockets for Cast Streaming packets.
 * switch between screen mirroring and content remoting modes.

The Service contains all session-management logic, and also interfaces with
`../../media/cast/` to encode and packetize media in realtime.

Specification: *TODO(jophba): Link to Cast Spec markdown.*

# Directory Breakdown

* browser/ - Browser-side implementation. Also, more can be found in
  `../../chrome/browser/media/cast_mirroring_service_host.h` and
  `../../chrome/browser/media/router/providers/cast/`.

* mojom/ - Mojo interfaces.

* service/ - Mirroring Service implementation, as described above.
