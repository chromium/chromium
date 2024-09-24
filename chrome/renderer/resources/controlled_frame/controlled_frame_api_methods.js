// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the public-facing API functions for <controlledframe>.

// Contains a list of API method names that do not produce asynchronous results
// for use in GuestViewContainerElement.forwardApiMethods().
const CONTROLLED_FRAME_API_METHODS = [
  // Returns whether there is a previous history entry to navigate to.
  'canGoBack',

  // Returns whether there is a subsequent history entry to navigate to.
  'canGoForward',

  // Returns the user agent string used by the webview for guest page requests.
  'getUserAgent',

  // Prints the contents of the webview.
  'print',

  // Reloads the current top-level page.
  'reload',

  // Set audio mute.
  'setAudioMuted',

  // Override the user agent string used by the webview for guest page requests.
  'setUserAgentOverride',

  // Stops loading the current navigation if one is in progress.
  'stop',
];

// Contains a list of API details that can return Promises. The details have the
// API name and the argument list index for their callback parameter. The
// callback index is necessary for APIs that are implemented by an internal API
// object, since there is not a way to know the expected size of the arguments
// accepted by the function.
const CONTROLLED_FRAME_PROMISE_API_METHODS = [
  // Add content scripts for the guest page.
  {name: 'addContentScripts', callbackIndex: 1},

  // Navigates to the previous history entry.
  {name: 'back', callbackIndex: 0},

  // Captures the visible region of the WebView contents into a bitmap.
  {name: 'captureVisibleRegion', callbackIndex: 1},

  // Clears browsing data for the WebView partition.
  {name: 'clearData', callbackIndex: 2},

  // Injects JavaScript code into the guest page.
  {name: 'executeScript', callbackIndex: 1},

  // Navigates to the subsequent history entry.
  {name: 'forward', callbackIndex: 0},

  // Returns audio state.
  {name: 'getAudioState', callbackIndex: 0},

  // Gets the current zoom factor.
  {name: 'getZoom', callbackIndex: 0},

  // Navigates to a history entry using a history index relative to the current
  // navigation.
  {name: 'go', callbackIndex: 1},

  // Injects CSS into the guest page.
  {name: 'insertCSS', callbackIndex: 1},

  // Returns whether audio is muted.
  {name: 'isAudioMuted', callbackIndex: 0},

  // Removes content scripts for the guest page.
  {name: 'removeContentScripts', callbackIndex: 1},

  // Changes the zoom factor of the page.
  {name: 'setZoom', callbackIndex: 1},

  // Changes the zoom mode of the webview.
  {name: 'setZoomMode', callbackIndex: 1},
];

// Contains a list of API method names that should be deleted from the
// ControlledFrame element if they exist. These methods exist on other
// GuestView implementations but aren't supported in Controlled Frame.
const CONTROLLED_FRAME_DELETED_API_METHODS = [
  'find',
  'getProcessId',
  'getZoomMode',
  'isSpatialNavigationEnabled',
  'isUserAgentOverridden',
  'loadDataWithBaseUrl',
  'setSpatialNavigationEnabled',
  'stopFinding',
  'terminate',
];

exports.$set('CONTROLLED_FRAME_API_METHODS', CONTROLLED_FRAME_API_METHODS);
exports.$set(
    'CONTROLLED_FRAME_DELETED_API_METHODS',
    CONTROLLED_FRAME_DELETED_API_METHODS);
exports.$set(
    'CONTROLLED_FRAME_PROMISE_API_METHODS',
    CONTROLLED_FRAME_PROMISE_API_METHODS);
