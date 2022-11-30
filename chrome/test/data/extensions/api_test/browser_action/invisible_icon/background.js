// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// First get an empty canvas.
var canvas = new OffscreenCanvas(21, 21);

var invisibleImageData = canvas.getContext('2d').getImageData(0, 0, 21, 21);

// Fill the "canvas" element with some color.
var ctx = canvas.getContext('2d');
ctx.fillStyle = '#FF0000';
ctx.fillRect(0, 0, 21, 21);

var visibleImageData = ctx.getImageData(0, 0, 21, 21);

function setIcon(imageData) {
  return new Promise(function(resolve) {
    chrome.browserAction.setIcon({imageData: imageData}, function() {
      resolve(chrome.runtime.lastError ?
                  chrome.runtime.lastError.message : '');
    });
  });
}

chrome.test.notifyPass();
