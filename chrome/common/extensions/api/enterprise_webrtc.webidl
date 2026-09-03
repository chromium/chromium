// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Status of a WebRTC capture session. getCaptureStatus cannot fail, so the
// promise always resolves with this result and never carries an error.
dictionary StatusResult {
  // Whether this extension has an active capture session in this profile,
  // not whether some other client is capturing elsewhere in the browser.
  required boolean active;
};

// Restricts what a capture session observes. An omitted or empty `origins`
// list matches every origin in the profile.
dictionary CaptureFilter {
  sequence<DOMString> origins;
};

// Programmatic access to WebRTC diagnostic information equivalent to
// chrome://webrtc-internals. Extensions using this API in incognito mode MUST
// declare "incognito": "split" in their manifest. Spanning-mode extensions
// will not observe peer connections from the profile they are not running in.
[implemented_in="chrome/browser/extensions/api/enterprise_webrtc/enterprise_webrtc_api.h"]
interface Webrtc {
  // Starts a capture session recording WebRTC diagnostic events (the
  // data visible in chrome://webrtc-internals) for this extension in
  // this profile.
  // |filter|: Restricts the session to the given origins; omitted or
  // empty matches every origin in the profile.
  // |Returns|: Rejects if the session cannot be started, for example if
  // one is already active or the filter is invalid.
  static Promise<undefined> startCapture(optional CaptureFilter filter);

  // Returns the current capture status for this extension in this
  // profile.
  // |PromiseValue|: result
  static Promise<StatusResult> getCaptureStatus();
};

partial interface Enterprise {
  static attribute Webrtc webrtc;
};
partial interface Browser {
  static attribute Enterprise enterprise;
};
