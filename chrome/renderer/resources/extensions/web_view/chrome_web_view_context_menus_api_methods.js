// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the public-facing context menus API functions for
// <webview>.

// Contains a list of API details that can return Promises. The details have the
// API name and the argument list index for their callback parameter. The
// callback index is necessary for APIs that are implemented by an internal API
// object, since there is not a way to know the expected size of the arguments
// accepted by the function.
const PROMISE_API_METHODS = [
  // Create a new context menu item.
  {name: 'create', callbackIndex: 1},

  // Remove a context menu item.
  {name: 'remove', callbackIndex: 1},

  // Remove all context menu items that were added by this <webview>.
  {name: 'removeAll', callbackIndex: 0},

  // Update a previously created context menu item.
  {name: 'update', callbackIndex: 2},
];

exports.$set('PROMISE_API_METHODS', PROMISE_API_METHODS);
