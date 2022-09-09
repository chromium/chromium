// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Inject an image.
var img = document.createElement('img');
img.onload = function () {
  chrome.runtime.connect().postMessage(true);
};
img.onerror = function () {
  chrome.runtime.connect().postMessage(false);
};
img.src = 'icon3.png';
document.body.appendChild(img);
