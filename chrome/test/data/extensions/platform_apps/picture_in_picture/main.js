// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const video = document.createElement('video');
video.src = chrome.runtime.getURL('bear.webm');
video.load();
video.addEventListener('loadedmetadata', function() {
  chrome.test.sendMessage('Launched');
});

function enterPictureInPicture() {
  return Promise.all([
    video.requestPictureInPicture(),

    new Promise(resolve => {
      video.addEventListener('enterpictureinpicture', function(pipWindow) {
        resolve(pipWindow.width != 0 && pipWindow.height != 0);
      }, { once: true });
    }),
  ])
  .then(([ignored, result]) => result);
}

function exitPictureInPicture() {
  return Promise.all([
    document.exitPictureInPicture(),

    new Promise(resolve => {
      video.addEventListener('leavepictureinpicture', function() {
        resolve(true);
      }, { once: true });
    }),
  ])
  .then(([ignored, result]) => result);
}
