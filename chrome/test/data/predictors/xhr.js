// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function requestSynchronously(url) {
  var request = new XMLHttpRequest();
  request.open('GET', url, false);
  request.send(null);
}

requestSynchronously('/handled-by-test/image.png');
requestSynchronously("/handled-by-test/style.css");
requestSynchronously("/handled-by-test/script.js");
