// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A dictionary containing additional context about the error.
dictionary ErrorInfo {
  // The error message.
  required DOMString message;

  // URL where the error occurred.
  // Must be the full URL, containing the protocol (e.g.
  // http://www.example.com).
  required DOMString url;

  // Name of the product where the error occurred.
  // Defaults to the product variant of Chrome that is hosting the extension.
  // (e.g. "Chrome" or "Chrome_ChromeOS").
  DOMString product;

  // Version of the product where the error occurred.
  // Defaults to the version of Chrome that is hosting the extension (e.g.
  // "73.0.3683.75").
  DOMString version;

  // Line number where the error occurred.
  long lineNumber;

  // Column number where the error occurred.
  long columnNumber;

  // Used to map the obfuscated source code back to a source map. If present,
  // must match the debug_id used to upload the source map.
  DOMString debugId;

  // String containing the stack trace for the error.
  // Defaults to the empty string.
  DOMString stackTrace;
};

// Private API for Chrome component extensions to report errors.
[platforms=("chromeos")]
interface CrashReportPrivate {
  // Report and upload an error to Crash.
  // |info|: Information about the error.
  // |Returns|: Called when the error has been uploaded.
  static Promise<undefined> reportError(ErrorInfo info);
};

partial interface Browser {
  static attribute CrashReportPrivate crashReportPrivate;
};
