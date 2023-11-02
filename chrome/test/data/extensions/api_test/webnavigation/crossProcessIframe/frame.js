// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  setTimeout(function() {
    if (location.hostname === 'a.com') {
      // Notify the parent frame, so they can navigate us.
      parent.postMessage('a.com: go to b.com', location.ancestorOrigins[0]);
    } else if (location.hostname === 'b.com') {
      // Trigger a navigation to a site in another process.
      location.hostname = 'c.com';
    } else {
      console.assert(location.hostname === 'c.com');
      // Done.
    }
  }, 0);
};
