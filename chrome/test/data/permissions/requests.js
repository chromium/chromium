// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function requestNotification() {
  Notification.requestPermission();
}

async function requestCamera() {
  var constraints = {video: true};
  const stream = await navigator.mediaDevices.getUserMedia(constraints);
}

async function requestMicrophone() {
  var constraints = {audio: true};
  const stream = await navigator.mediaDevices.getUserMedia(constraints);
}

async function requestCameraAndMicrophone() {
  var constraints = {audio: true, video: true};
  const stream = await navigator.mediaDevices.getUserMedia(constraints);
}
