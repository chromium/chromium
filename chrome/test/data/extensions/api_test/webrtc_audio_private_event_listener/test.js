// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var gotNotification = false;

chrome.webrtcAudioPrivate.onSinksChanged.addListener(function () {
    gotNotification = true;
});

function reportIfGot() {
  window.domAutomationController.send(gotNotification ? "true" : "false");
}
