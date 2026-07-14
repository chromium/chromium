# browser_actuator transport

Server→client push transport for browser actuation, receiving a stream of
proto messages from a Google (OnePlatform/ESF) endpoint over a plain HTTPS
GET. Upstream (client→server) traffic is separate, individual HTTP POSTs,
deliberately not coupled to this stream.

## Wire format

The stream is a OnePlatform server-streaming RPC fetched with `alt=proto`
(`Accept: application/x-protobuf`): the response body is one never-ending
serialized proto,

```proto
message StreamBody {
  repeated bytes message = 1;    // One per streamed response message.
  google.rpc.Status status = 2;  // At most once, at the end of the RPC.
  repeated bytes noop = 15;      // Keep-alive padding.
}
```

so each complete top-level length-delimited field is one delivery — there
is no other chunking layer. Messages arrive as serialized protos
(protobuf-lite parses them above the transport), noops only prove the
connection is alive, and the status ends the RPC (no auto-reconnect).

OnePlatform can also encode this RPC as SSE (`alt=sse`), which the seams
below could support as a second implementation; `alt=proto` is used here
because the frontends JSON-ify protos in SSE `data:` lines, a poor fit
for binary payloads.

## Architecture

Per //docs/security/rule-of-2.md, C++ never interprets raw stream bytes:
all framing decisions about the untrusted byte stream are made by a pure
Rust crate (`allow_unsafe = false`, no deps, fuzzed) behind a small `cxx`
glue crate. A C++ client built on
`network::SimpleURLLoader::DownloadAsStream` owns connection lifecycle:
reconnect with exponential backoff, a stall watchdog fed by any received
bytes, permanent failure on HTTP rejection or malformed framing.

Seams:

* `MessageStreamClient` is the wire-format-agnostic consumer interface;
  an alternative encoding of the same RPC (e.g. `alt=sse`) would be a
  second implementation.
* `StreamConnectionDelegate` isolates resume and auth policy from the
  connection machinery: extracting resume state from payloads and echoing
  it on reconnect, asynchronous request preparation (OAuth token
  minting), and HTTP-failure retry policy. Delegates compose — an auth
  decorator can wrap a resume-state delegate.

The wire protocol has no built-in resume mechanism: resume state, if any,
lives inside message payloads and rides back on the next connection
attempt (a `last_received_sequence_number` query parameter), managed by
the delegate.
