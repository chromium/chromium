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
// Note: In tests using net::EmbeddedTestServer with a custom hostname not
// covered by the default test certs, using this switch is usually incorrect.
// Strongly prefer to use ServerCertificateConfig with `dns_names` (if possible,
// by calling the helper SetCertHostnames() on the EmbeddedTestServer instance),
// to configure the test server with a valid certificate instead of ignoring
// all certificate errors. If the test fixture inherits from
// content::BrowserTestBase, consider using the `embedded_https_test_server()`
// it provides, which is configured by default with a valid certificate for a
// handful of hostnames commonly used in tests.
// TODO(crbug.com/40147519): Retire this switch. This switch is an attractive
// nuisance that doesn't do the right thing.
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
