// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('popup_loaded', message => {
  chrome.permissions.request({ origins: ['https://www.google.com/']});
});
