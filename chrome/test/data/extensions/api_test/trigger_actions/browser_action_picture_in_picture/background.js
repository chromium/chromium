// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const video = document.createElement('video');

chrome.test.getConfig(function(config) {
  video.src = 'http://localhost:' + config.testServer.port +
    '/media/bigbuck.webm';
  video.load();
  video.addEventListener('loadedmetadata', function() {
    chrome.test.notifyPass();
  })
});

// Toggle Picture-in-Picture when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(tab) {
  if (video !== document.pictureInPictureElement) {
    enterPictureInPicture();
  } else {
    exitPictureInPicture();
  }
});

function enterPictureInPicture() {
  video.requestPictureInPicture()
  .then(pipWindow => {
    if (pipWindow.width === 0 || pipWindow.height === 0) {
      return Promise.reject('Picture-in-Picture window size is not set.');
    }
    chrome.test.notifyPass();
  })
  .catch(error => { chrome.test.notifyFail('Error: ' + error); })
}

function exitPictureInPicture() {
  document.exitPictureInPicture()
  .then(() => { chrome.test.notifyPass(); })
  .catch(error => { chrome.test.notifyFail('Error: ' + error); })
}
