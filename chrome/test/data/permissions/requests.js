// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cameraStream;
var micStream;

function requestNotification() {
  Notification.requestPermission();
}

async function requestCamera() {
  try {
    var constraints = {video: true};
    cameraStream = await navigator.mediaDevices.getUserMedia(constraints);
  } catch (error) {
  }
}

async function requestMicrophone() {
  try {
    var constraints = {audio: true};
    micStream = await navigator.mediaDevices.getUserMedia(constraints);
  } catch (error) {
  }
}

async function requestCameraAndMicrophone() {
  try {
    var constraints = {audio: true, video: true};
    await navigator.mediaDevices.getUserMedia(constraints);
  } catch (error) {
  }
}

async function requestLocation() {
  try {
    await navigator.geolocation.getCurrentPosition((position_) => {});
  } catch (error) {
  }
}

function stopCamera() {
  const track = cameraStream.getVideoTracks()[0];
  track.stop();
}

function stopMic() {
  const track = micStream.getAudioTracks()[0];
  track.stop();
}
