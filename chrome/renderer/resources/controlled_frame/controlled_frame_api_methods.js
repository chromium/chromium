// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the public-facing API functions for <controlledframe>.

// Contains a list of API method names that do not produce asynchronous results
// for use in GuestViewContainerElement.forwardApiMethods().
const CONTROLLED_FRAME_API_METHODS = [
  // Add content scripts for the guest page.
  'addContentScripts',

  // Returns whether there is a previous history entry to navigate to.
  'canGoBack',

  // Returns whether there is a subsequent history entry to navigate to.
  'canGoForward',

  // Returns Chrome's internal process ID for the guest web page's current
  // process.
  'getProcessId',

  // Returns the user agent string used by the webview for guest page requests.
  'getUserAgent',

  // Indicates whether or not the webview's user agent string has been
  // overridden.
  'isUserAgentOverridden',

  // Loads a data URL with a specified base URL used for relative links.
  // Optionally, a virtual URL can be provided to be shown to the user instead
  // of the data URL.
  'loadDataWithBaseUrl',

  // Prints the contents of the webview.
  'print',

  // Removes content scripts for the guest page.
  'removeContentScripts',

  // Reloads the current top-level page.
  'reload',

  // Set audio mute.
  'setAudioMuted',

  // Set spatial navigation state.
  'setSpatialNavigationEnabled',

  // Override the user agent string used by the webview for guest page requests.
  'setUserAgentOverride',

  // Stops loading the current navigation if one is in progress.
  'stop',

  // Ends the current find session.
  'stopFinding',

  // Forcibly kills the guest web page's renderer process.
  'terminate'
];

// Contains a list of API details that can return Promises. The details have the
// API name and the argument list index for their callback parameter. The
// callback index is necessary for APIs that are implemented by an internal API
// object, since there is not a way to know the expected size of the arguments
// accepted by the function.
const CONTROLLED_FRAME_PROMISE_API_METHODS = [
  // Navigates to the previous history entry.
  {name: 'back', callbackIndex: 0},

  // Captures the visible region of the WebView contents into a bitmap.
  {name: 'captureVisibleRegion', callbackIndex: 1},

  // Clears browsing data for the WebView partition.
  {name: 'clearData', callbackIndex: 2},

  // Injects JavaScript code into the guest page.
  {name: 'executeScript', callbackIndex: 1},

  // Initiates a find-in-page request.
  {name: 'find', callbackIndex: 2},

  // Navigates to the subsequent history entry.
  {name: 'forward', callbackIndex: 0},

  // Returns audio state.
  {name: 'getAudioState', callbackIndex: 0},

  // Gets the current zoom factor.
  {name: 'getZoom', callbackIndex: 0},

  // Gets the current zoom mode of the webview.
  {name: 'getZoomMode', callbackIndex: 0},

  // Navigates to a history entry using a history index relative to the current
  // navigation.
  {name: 'go', callbackIndex: 1},

  // Injects CSS into the guest page.
  {name: 'insertCSS', callbackIndex: 1},

  // Returns whether audio is muted.
  {name: 'isAudioMuted', callbackIndex: 0},

  // Returns whether spatial navigation is enabled.
  {name: 'isSpatialNavigationEnabled', callbackIndex: 0},

  // Changes the zoom factor of the page.
  {name: 'setZoom', callbackIndex: 1},

  // Changes the zoom mode of the webview.
  {name: 'setZoomMode', callbackIndex: 1},
];

exports.$set('CONTROLLED_FRAME_API_METHODS', CONTROLLED_FRAME_API_METHODS);
exports.$set(
    'CONTROLLED_FRAME_PROMISE_API_METHODS',
    CONTROLLED_FRAME_PROMISE_API_METHODS);
