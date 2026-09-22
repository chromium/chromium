// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_URL_UTIL_H_
#define CHROME_BROWSER_UI_STARTUP_URL_UTIL_H_

class GURL;

namespace startup {

// Validates a URL passed during startup or external system events
// (Command-Line, macOS Intents, Android JNI Intents).
//
// This represents a WebUnsafe/OS context. It permits standard web safe schemes
// PLUS a strict subset of privileged system targets required for native OS
// integrations (such as local file: access and approved settings reset pages).
bool ValidateLaunchUrlWebUnsafe(const GURL& url);

// Validates a URL originating from an untrusted source, such as a custom
// scheme redirect from a web page or a synchronized startup preference.
//
// This represents a WebSafe/In-Browser context. It is an explicit allowlist of
// http, https and about:blank. Note that this is deliberately stricter than
// ChildProcessSecurityPolicy::IsWebSafeScheme(), which also admits data:, ws:,
// wss: and schemes registered via navigator.registerProtocolHandler().
//
// Banned nested schemes (filesystem: and blob:) are explicitly rejected to
// prevent web pages from laundering privileged origins through WebSafe schemes.
bool ValidateLaunchUrlWebSafe(const GURL& url);

}  // namespace startup

#endif  // CHROME_BROWSER_UI_STARTUP_URL_UTIL_H_
