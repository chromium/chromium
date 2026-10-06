// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Experimental API to handle data collection in the browser process for
// AI features.
enum SnapshotType {
  "apc",
  "screenshot",
  "apc_and_screenshot"
};

dictionary ApcOptions {
  // Selects the data to return. Defaults to apc. Screenshot-only requests still
  // extract APC internally to support sensitive-field redaction.
  SnapshotType type;

  // Requests content-only APC, omitting interaction details, geometry, and
  // stacking order. Defaults to false.
  boolean excludeActionableDetails;

  // Excludes APC content from frames outside the top-level page's site.
  // Defaults to false.
  boolean excludeCrossSiteFrames;

  // Excludes nodes and subtrees identified as advertising.
  // Defaults to false.
  boolean excludeAdRelated;

  // Maximum number of named HTML meta elements to include. Must be nonnegative.
  // Zero or omission excludes meta elements.
  long maxMetaElements;
};

// A snapshot is the result of one capture request. It contains structured
// page content (APC), a screenshot (an image of the visible viewport), or both.
dictionary ApcSnapshot {
  // Base64-encoded optimization_guide.proto.AnnotatedPageContent bytes.
  // Present for apc and apc_and_screenshot snapshots.
  DOMString apcBase64;

  // Base64-encoded PNG bytes of the visible viewport.
  // Present for screenshot and apc_and_screenshot snapshots.
  DOMString screenshotBase64;
};

[implemented_in="chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"]
interface ExperimentalAiData {
  // |PromiseValue|: data
  static Promise<ArrayBuffer> getAiData(long domNodeId,
                                       DOMString frameId,
                                       DOMString userInput,
                                       long tabId);
  // |PromiseValue|: data
  static Promise<ArrayBuffer> getAiDataWithSpecifier(
      long tabId,
      ArrayBuffer aiDataSpecifier);

  // Synchronously resolves an ID or array of IDs in the calling frame's
  // document. Unknown, invalid, detached, or other-document nodes return null.
  // Arrays preserve input order and return null for each invalid entry.
  // For child-frame IDs, invoke this API in each iframe's isolated world.
  // Only available to the APC Debugging Extension in content scripts.
  [nocompile] static any getNodeForDomNodeId(any domNodeIdOrIds);

  // Synchronously returns an ID or array of IDs for live DOM Nodes in the
  // calling frame's document. Non-Nodes and detached nodes return null.
  // Only available to the APC Debugging Extension in content scripts.
  [nocompile] static any getDomNodeId(any nodeOrNodes);

  // Captures Annotated Page Content (APC), a viewport screenshot, or both for
  // debugging. Uses the same extension and channel restrictions as getAiData.
  // Rejects requests if any frame in the page is blocked by enterprise runtime
  // host policy, honoring allowed-host exceptions. Screenshot restrictions
  // also apply.
  // |PromiseValue|: snapshot
  static Promise<ApcSnapshot> getApcSnapshot(long tabId, ApcOptions options);
};

partial interface Browser {
  static attribute ExperimentalAiData experimentalAiData;
};
