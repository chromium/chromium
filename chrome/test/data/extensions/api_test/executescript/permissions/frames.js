// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// document.write is not pretty, but it saves us from having to wait for the
// DOM to load before adding iframes.  This means one less callback to wait on.
function addIFrame(host) {
  document.write(['<iframe src="http://', host, ':', location.port,
                  '/extensions/api_test/executescript',
                  '/permissions/empty.html"> </iframe>'].join(''));
}
addIFrame("a.com");
addIFrame("b.com");
addIFrame("c.com");
