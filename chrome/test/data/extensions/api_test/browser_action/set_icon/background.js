// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getImageData() {
  var canvas = new OffscreenCanvas(10, 100);
  var ctx = canvas.getContext("2d");

  ctx.fillStyle = "green";
  ctx.fillRect(10, 10, 100, 100);

  return ctx.getImageData(50, 50, 100, 100);
}

chrome.tabs.query({active: true}, function(tabs) {
  // When the browser action is clicked, add an icon.
  chrome.browserAction.onClicked.addListener(function(tab) {
    chrome.browserAction.setIcon({
      imageData: getImageData()
    });
    chrome.test.notifyPass();
  });
  chrome.test.notifyPass();
});
