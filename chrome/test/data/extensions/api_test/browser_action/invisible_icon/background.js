// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// First get an empty canvas.
const canvas = new OffscreenCanvas(21, 21);

const invisibleImageData = canvas.getContext('2d').getImageData(0, 0, 21, 21);

// Fill the 'canvas' element with some color.
const ctx = canvas.getContext('2d');
ctx.fillStyle = '#FF0000';
ctx.fillRect(0, 0, 21, 21);

const visibleImageData = ctx.getImageData(0, 0, 21, 21);

function setIcon(imageData) {
  return new Promise(function(resolve) {
    chrome.browserAction.setIcon({imageData: imageData}, function() {
      resolve(chrome.runtime.lastError ? chrome.runtime.lastError.message : '');
    });
  });
}

chrome.test.notifyPass();
