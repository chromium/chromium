// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  chrome.test.runTests([
    function onVolumeListChanged() {
      chrome.fileSystem.getVolumeList(
         chrome.test.callbackPass(function(volumeList) {
           chrome.test.assertEq(5, volumeList.length);

           // Confirm that adding a newly mounted volume emits an event, and
           // that the volume list is updated.
           chrome.fileSystem.onVolumeListChanged.addListener(
               chrome.test.callbackPass(function(event) {
                 chrome.test.assertEq(6, event.volumes.length);
                 chrome.fileSystem.getVolumeList(
                     chrome.test.callbackPass(function(volumeList) {
                       chrome.test.assertEq(6, volumeList.length);
                     }));
               }));
      }));
    }
  ]);
}

chrome.app.runtime.onLaunched.addListener(runTests);
