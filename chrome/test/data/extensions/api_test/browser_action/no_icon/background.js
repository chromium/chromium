// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var canvas = new OffscreenCanvas(21, 21);
var canvasHD = new OffscreenCanvas(42, 42);

// Fill the canvases element with some color.
var ctx = canvas.getContext('2d');
ctx.fillStyle = '#FF0000';
ctx.fillRect(0, 0, 21, 21);

var ctxHD = canvasHD.getContext('2d');
ctxHD.fillStyle = '#FF0000';
ctxHD.fillRect(0, 0, 42, 42);

var image = ctx.getImageData(0, 0, 21, 21);
var imageHD = ctxHD.getImageData(0, 0, 42, 42);

var setIconParamQueue = [
  {imageData: image},
  {path: 'icon.png'},
  {imageData: {'21': image, '42': imageHD}},
  {path: {'21': 'icon.png', '42': 'icon2.png'}},
  {imageData: {'21': image}},
  {path: {'21': 'icon.png'}},
  {imageData: {'42': imageHD}},
  {imageData: {}},
  {path: {}},
];

// Called when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(windowId) {
  if (setIconParamQueue.length == 0) {
    chrome.test.notifyFail('Queue of params for test cases unexpectedly empty');
    return;
  }

  try {
    chrome.browserAction.setIcon(setIconParamQueue.shift(), function() {
      chrome.test.notifyPass();});
  } catch (error) {
    console.log(error.message);
    chrome.test.notifyFail(error.message);
  }
});

chrome.test.notifyPass();
