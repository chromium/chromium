// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.power.requestKeepAwake('system');
  chrome.test.sendMessage('launched');
};
