// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {removeQueryAndReferenceFromURL} from '//ios/web/public/js_messaging/resources/utils.js';

// Returns the URL for the frame to be set in the FormData.
export function getFrameUrlOrOrigin(frame: Window): string {
  if ((frame === frame.top) ||
      ((frame.location.href !== 'about:blank') &&
       (frame.location.href !== 'about:srcdoc'))) {
    // If the full URL is available, use it.
    return removeQueryAndReferenceFromURL(frame.location.href);
  } else {
    // Iframes might have empty own URLs, and they do not have access to the
    // parent frame URL, only to the origin. Use it as the only available data.
    return frame.origin;
  }
}
