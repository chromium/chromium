// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function setIconAndShow() {
    var canvas = document.createElement('canvas');
    canvas.width = 20;
    canvas.height = 20;

    var canvas_context = canvas.getContext('2d');
    canvas_context.clearRect(0, 0, 20, 20);
    canvas_context.fillStyle = '#00FF00';
    canvas_context.fillRect(5, 5, 15, 15);
    var data = canvas_context.getImageData(0, 0, 19, 19);
    chrome.systemIndicator.setIcon(
        { imageData: data },
        chrome.test.callbackPass(function() {
          chrome.systemIndicator.enable();
          chrome.systemIndicator.disable();
        }));
  }
]);
