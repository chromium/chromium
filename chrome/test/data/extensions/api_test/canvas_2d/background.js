// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

canvas = document.getElementById("my-canvas");
if (canvas) {
  if (canvas.getContext) {
    context = canvas.getContext("2d");
    if (context) {
      context.fillStyle = 'red';
      context.fillRect(20, 20, 40, 40);
      chrome.test.notifyPass();
    } else {
      chrome.test.notifyFail("unable to getContext('2d')");
    }
  } else {
    chrome.test.notifyFail("canvas.getContext null");
  }
} else {
  chrome.test.notifyFail("couldn't find element my-canvas");
}
