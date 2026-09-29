// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Status of a WebRTC capture session. getCaptureStatus cannot fail, so the
// promise always resolves with this result and never carries an error.
dictionary CaptureStatus {
  required boolean active;
};

// A getUserMedia request observed by WebRTC-internals.
dictionary GetUserMediaRequest {
  required long rid;
  required long pid;
  required long requestId;
  required DOMString requestType;
  required DOMString origin;
  required DOMString url;
  required double timestamp;
  DOMString audio;
  DOMString video;
  DOMString streamId;
  DOMString audioTrackInfo;
  DOMString videoTrackInfo;
  DOMString error;
  DOMString errorMessage;
};

// A point-in-time snapshot of WebRTC-internals data.
dictionary CaptureSnapshot {
  required sequence<GetUserMediaRequest> getUserMedia;
  // Dictionary of PeerConnectionData objects keyed by ID (free-form;
  // compiles to additionalProperties:any).
  required object peerConnections;
  required DOMString userAgent;
  required sequence<any> userAgentData;
};

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

// A peer connection's chrome://webrtc-internals record as it stood when the
// connection was added.
dictionary PeerConnectionRecord {
  // ID of the renderer process that created the connection.
  required long rid;
  // ID of the connection, unique within its renderer.
  required long lid;
  // OS process ID of that renderer.
  required long pid;
  required DOMString rtcConfiguration;
  required DOMString url;
  required boolean isOpen;
  required boolean connected;
  required double timestamp;
};

callback OnCaptureStoppedListener = undefined ();

interface OnCaptureStoppedEvent : ExtensionEvent {
  static undefined addListener(OnCaptureStoppedListener listener);
  static undefined removeListener(OnCaptureStoppedListener listener);
  static boolean hasListener(OnCaptureStoppedListener listener);
};

// |id|: Identifies the peer connection for the rest of this session, and is
// the ID $(ref:onPeerConnectionRemoved) reports when it goes away.
// |data|: The connection's chrome://webrtc-internals record as it stood when
// it was added.
callback OnPeerConnectionAddedListener =
    undefined (DOMString id, PeerConnectionRecord data);

interface OnPeerConnectionAddedEvent : ExtensionEvent {
  static undefined addListener(OnPeerConnectionAddedListener listener);
  static undefined removeListener(OnPeerConnectionAddedListener listener);
  static boolean hasListener(OnPeerConnectionAddedListener listener);
};

// |id|: The ID $(ref:onPeerConnectionAdded) reported for this connection.
callback OnPeerConnectionRemovedListener = undefined (DOMString id);

interface OnPeerConnectionRemovedEvent : ExtensionEvent {
  static undefined addListener(OnPeerConnectionRemovedListener listener);
  static undefined removeListener(OnPeerConnectionRemovedListener listener);
  static boolean hasListener(OnPeerConnectionRemovedListener listener);
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
  // Stops the active capture session for this extension in this profile.
  // |Returns|: Rejects if there is no session to stop.
  static Promise<undefined> stopCapture();

  // Returns the WebRTC diagnostic data this extension's session has
  // captured in this profile.
  // |filter|: Narrows the snapshot further; it cannot widen the scope set
  // by $(ref:startCapture).
  // |Returns|: Rejects if there is no session to snapshot, or if the filter
  // has too many origins or one that cannot be parsed.
  // |PromiseValue|: snapshot
  static Promise<CaptureSnapshot> getSnapshot(optional CaptureFilter filter);

  // Returns the current capture status for this extension in this
  // profile.
  // |PromiseValue|: result
  static Promise<StatusResult> getCaptureStatus();

  // Fired when this extension's active capture session in this profile
  // stops.
  static attribute OnCaptureStoppedEvent onCaptureStopped;

  // Fired when a peer connection whose origin matches this session's filter
  // is added in this profile. Carries the peer connection's ID and its
  // chrome://webrtc-internals record.
  static attribute OnPeerConnectionAddedEvent onPeerConnectionAdded;

  // Fired when a peer connection is removed. Can fire for a connection this
  // session never saw $(ref:onPeerConnectionAdded) for, if the connection
  // already existed when the session started.
  static attribute OnPeerConnectionRemovedEvent onPeerConnectionRemoved;
};

partial interface Enterprise {
  static attribute Webrtc webrtc;
};
partial interface Browser {
  static attribute Enterprise enterprise;
};
