// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const video = document.createElement('video');

// Load video and toggle Picture-in-Picture when user clicks browser action.
chrome.browserAction.onClicked.addListener(function(tab) {
  if (video.readyState === 0) {
    loadVideo();
    return;
  }
  if (video !== document.pictureInPictureElement) {
    enterPictureInPicture();
  } else {
    exitPictureInPicture();
  }
});

function loadVideo() {
  chrome.test.getConfig(function(config) {
    video.src = 'http://localhost:' + config.testServer.port +
        '/media/bigbuck.webm';
    video.load();
    video.addEventListener('loadedmetadata', function() {
      chrome.test.sendMessage("video_loaded");
    });
  });
}

function enterPictureInPicture() {
  video.requestPictureInPicture()
  .then(pipWindow => {
    if (pipWindow.width === 0 || pipWindow.height === 0) {
      return Promise.reject('Picture-in-Picture window size is not set.');
    }
    chrome.test.sendMessage("entered_pip");
  })
  .catch(error => { console.log(error.message); });
}

function exitPictureInPicture() {
  document.exitPictureInPicture()
  .catch(error => { console.log(error.message); });
}
