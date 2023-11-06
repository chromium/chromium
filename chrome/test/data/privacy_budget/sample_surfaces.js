// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Scrapes the attributes of the `Screen` object and sends it back to the test
// via `sendValueToTest`.
window.addEventListener('load', () => {
  sendValueToTest(JSON.stringify({
    "Screen": {
      "availWidth"  : window.screen.availWidth,
      "availHeight" : window.screen.availHeight,
      "width"       : window.screen.width,
      "height"      : window.screen.height,
      "colorDepth"  : window.screen.colorDepth,
      "pixelDepth"  : window.screen.pixelDepth,
      "availLeft"   : window.screen.availLeft,
      "availTop"    : window.screen.availTop,
      "left"        : window.screen.left,
      "top"         : window.screen.top
    },
    "Navigator": {
      "doNotTrack"  : navigator.doNotTrack
    }
  }));
});
