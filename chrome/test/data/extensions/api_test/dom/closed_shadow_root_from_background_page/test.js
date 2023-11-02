// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function() {
  var host = document.getElementById('bghost');
  var shadowRoot1 = host.attachShadow({mode: 'closed'});
  shadowRoot1.appendChild(document.createElement('div'));

  let shadowRoot2 = chrome.dom.openOrClosedShadowRoot(host);
  chrome.test.assertEq(shadowRoot2.childElementCount, 1);
  chrome.test.succeed();
}]);
