// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cameraStream;
var micStream;

function requestNotification() {
  Notification.requestPermission();
}

async function requestCamera() {
  var constraints = { video: true };
  cameraStream = await navigator.mediaDevices.getUserMedia(constraints);
}

async function requestMicrophone() {
  var constraints = { audio: true };
  micStream = await navigator.mediaDevices.getUserMedia(constraints);
}

async function requestCameraAndMicrophone() {
  var constraints = { audio: true, video: true };
  await navigator.mediaDevices.getUserMedia(constraints);
}

function stopCamera() {
  const track = cameraStream.getVideoTracks()[0];
  track.stop();
}

function stopMic() {
  const track = micStream.getAudioTracks()[0];
  track.stop();
}
