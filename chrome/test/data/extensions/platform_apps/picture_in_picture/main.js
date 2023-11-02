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
  video.requestPictureInPicture()
      .catch(error => { window.domAutomationController.send(false); });

  video.addEventListener('enterpictureinpicture', function(pipWindow) {
    window.domAutomationController.send(
        pipWindow.width != 0 && pipWindow.height != 0);
  }, { once: true });
}

function exitPictureInPicture() {
  document.exitPictureInPicture()
      .catch(error => { window.domAutomationController.send(false); });

  video.addEventListener('leavepictureinpicture', function() {
    window.domAutomationController.send(true);
  }, { once: true });
}
