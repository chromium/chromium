// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getVolumeList() {
    chrome.fileSystem.getVolumeList(
        chrome.test.callbackPass(function(volumeList) {
          // Drive is not available in a real kiosk session. Hhowever, this test
          // runs in a normal session (with a user marked as kiosk user) since
          // executing a real real kiosk session is tests is very complicated.
          // Whether Drive is available in the real kiosk session is tested
          // separately in:
          // chrome/browser/ash/login/app_mode/kiosk_browsertest.cc.
          chrome.test.assertEq(5, volumeList.length);
          chrome.test.assertTrue(/^downloads:.*/.test(volumeList[0].volumeId));
          chrome.test.assertTrue(volumeList[0].writable);
          chrome.test.assertEq('drive:drive-user', volumeList[1].volumeId);
          chrome.test.assertTrue(volumeList[1].writable);
          chrome.test.assertEq(
              'system_internal:ShareCache', volumeList[2].volumeId);
          chrome.test.assertFalse(volumeList[2].writable);

          chrome.test.assertEq('testing:read-only', volumeList[3].volumeId);
          chrome.test.assertFalse(volumeList[3].writable);
          chrome.test.assertEq('testing:writable', volumeList[4].volumeId);
          chrome.test.assertTrue(volumeList[4].writable);
        }));
  }
]);
