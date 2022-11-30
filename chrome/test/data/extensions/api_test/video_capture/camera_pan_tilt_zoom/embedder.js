// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function cameraMovePermissionGrantsAccessToPanTiltZoom() {
    window.focus();
    const stream = await navigator.mediaDevices.getUserMedia({
      video: {pan: true, tilt: true, zoom: true},
    });
    chrome.test.assertTrue(!!stream);

    const [videoTrack] = stream.getVideoTracks();
    const settings = videoTrack.getSettings();
    chrome.test.assertEq(100, settings.pan);
    chrome.test.assertEq(100, settings.tilt);
    chrome.test.assertEq(100, settings.zoom);

    await videoTrack.applyConstraints({advanced: [{pan: 200}]});
    await videoTrack.applyConstraints({advanced: [{tilt: 200}]});
    await videoTrack.applyConstraints({advanced: [{zoom: 200}]});

    chrome.test.succeed();
  },
]);