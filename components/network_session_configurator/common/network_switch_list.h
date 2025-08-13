// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file deliberately has no header guard, as it's inlined in a number of
// files.
// no-include-guard-because-multiply-included

// Disables the QUIC protocol.
NETWORK_SWITCH(kDisableQuic, "disable-quic")

// Disables the HTTP/2 protocol.
NETWORK_SWITCH(kDisableHttp2, "disable-http2")

// Enables Alternate-Protocol when the port is user controlled (> 1024).
NETWORK_SWITCH(kEnableUserAlternateProtocolPorts,
               "enable-user-controlled-alternate-protocol-ports")

// Enables the QUIC protocol.  This is a temporary testing flag.
NETWORK_SWITCH(kEnableQuic, "enable-quic")

// Ignores certificate-related errors.
NETWORK_SWITCH(kIgnoreCertificateErrors, "ignore-certificate-errors")

// Specifies a comma separated list of host-port pairs to force use of QUIC on.
NETWORK_SWITCH(kOriginToForceQuicOn, "origin-to-force-quic-on")

// Disables known-root checks for outgoing WebTransport connections.
NETWORK_SWITCH(kWebTransportDeveloperMode, "webtransport-developer-mode")

// Specifies a comma separated list of QUIC connection options to send to
// the server.
NETWORK_SWITCH(kQuicConnectionOptions, "quic-connection-options")

// Specifies a comma separated list of QUIC client connection options.
NETWORK_SWITCH(kQuicClientConnectionOptions, "quic-client-connection-options")

// Specifies the maximum length for a QUIC packet.
NETWORK_SWITCH(kQuicMaxPacketLength, "quic-max-packet-length")

// Specifies the version of QUIC to use.
NETWORK_SWITCH(kQuicVersion, "quic-version")

// Allows for forcing socket connections to http/https to use fixed ports.
NETWORK_SWITCH(kTestingFixedHttpPort, "testing-fixed-http-port")
NETWORK_SWITCH(kTestingFixedHttpsPort, "testing-fixed-https-port")

// Enable "greasing" HTTP/2 frame types, that is, sending frames of reserved
// types.  See https://tools.ietf.org/html/draft-bishop-httpbis-grease-00 for
// more detail.
NETWORK_SWITCH(kHttp2GreaseFrameType, "http2-grease-frame-type")

// If request has no body, close the stream not by setting END_STREAM flag on
// the HEADERS frame, but by sending an empty DATA frame with END_STREAM
// afterwards.  Only affects HTTP/2 request streams, not proxy or bidirectional
// streams.
NETWORK_SWITCH(kHttp2EndStreamWithDataFrame, "http2-end-stream-with-data-frame")
