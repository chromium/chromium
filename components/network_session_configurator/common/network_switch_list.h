// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file deliberately has no header guard, as it's inlined in a number of
// files.
// no-include-guard-because-multiply-included

// Disables the QUIC protocol.
NETWORK_SWITCH(kDisableQuic, "disable-quic")

// Disables the HTTP/2 protocol.
NETWORK_SWITCH(kDisableHttp2, "disable-http2")

// Enable Websocket over HTTP/2.
NETWORK_SWITCH(kEnableWebsocketOverHttp2, "enable-websocket-over-http2")

// Enables Alternate-Protocol when the port is user controlled (> 1024).
NETWORK_SWITCH(kEnableUserAlternateProtocolPorts,
               "enable-user-controlled-alternate-protocol-ports")

// Enables the QUIC protocol.  This is a temporary testing flag.
NETWORK_SWITCH(kEnableQuic, "enable-quic")

// Ignores certificate-related errors.
NETWORK_SWITCH(kIgnoreCertificateErrors, "ignore-certificate-errors")

// Specifies a comma separated list of host-port pairs to force use of QUIC on.
NETWORK_SWITCH(kOriginToForceQuicOn, "origin-to-force-quic-on")

// Specifies a comma separated list of QUIC connection options to send to
// the server.
NETWORK_SWITCH(kQuicConnectionOptions, "quic-connection-options")

// Specifies the maximum length for a QUIC packet.
NETWORK_SWITCH(kQuicMaxPacketLength, "quic-max-packet-length")

// Specifies the version of QUIC to use.
NETWORK_SWITCH(kQuicVersion, "quic-version")

// Allows for forcing socket connections to http/https to use fixed ports.
NETWORK_SWITCH(kTestingFixedHttpPort, "testing-fixed-http-port")
NETWORK_SWITCH(kTestingFixedHttpsPort, "testing-fixed-https-port")

// Comma-separated list of rules that control how hostnames are mapped.
//
// For example:
//    "MAP * 127.0.0.1" --> Forces all hostnames to be mapped to 127.0.0.1
//    "MAP *.google.com proxy" --> Forces all google.com subdomains to be
//                                 resolved to "proxy".
//    "MAP test.com [::1]:77 --> Forces "test.com" to resolve to IPv6 loopback.
//                               Will also force the port of the resulting
//                               socket address to be 77.
//    "MAP * baz, EXCLUDE www.google.com" --> Remaps everything to "baz",
//                                            except for "www.google.com".
//
// These mappings apply to the endpoint host in a net::URLRequest (the TCP
// connect and host resolver in a direct connection, and the CONNECT in an http
// proxy connection, and the endpoint host in a SOCKS proxy connection).
//
// TODO(mmenke): Can we just remove this?  host-resolver-rules is more generally
// useful.
NETWORK_SWITCH(kHostRules, "host-rules")

// Enable "greasing" HTTP/2, that is, sending SETTINGS parameters with reserved
// identifiers and sending frames of reserved types, respectively.  See
// https://tools.ietf.org/html/draft-bishop-httpbis-grease-00 for more detail.
NETWORK_SWITCH(kHttp2GreaseSettings, "http2-grease-settings")
NETWORK_SWITCH(kHttp2GreaseFrameType, "http2-grease-frame-type")
