// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Closes the given mojo endpoint once the page is unloaded.
 * Reference b/176139064.
 * @param {{$: {close: function()}}} endpoint The mojo endpoint.
 */
export function closeWhenUnload(endpoint) {
  const closeMojoConnection = () => {
    endpoint.$.close();
    window.removeEventListener('unload', closeMojoConnection);
  };
  window.addEventListener('unload', closeMojoConnection);
}
