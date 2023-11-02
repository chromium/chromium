// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.contentSettings.javascript.set({
  primaryPattern: '<all_urls>',
  setting: 'block'
}, function() {
  window.console.log('Blocking all JavaScript');
});

