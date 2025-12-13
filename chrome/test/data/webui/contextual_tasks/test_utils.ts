// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Base64 encoding of a UI handshake request message [1, 2, 3].
// Generated from btoa(String.fromCharCode(...[1, 2, 3]))
export const HANDSHAKE_REQUEST_MESSAGE_BASE64 = 'AQID';

// Byte array of a typical handshake response from the webview.
// Equivalent to base64 decoding 'CgIIAA=='
export const HANDSHAKE_RESPONSE_BYTES = new Uint8Array([10, 2, 8, 0]);
