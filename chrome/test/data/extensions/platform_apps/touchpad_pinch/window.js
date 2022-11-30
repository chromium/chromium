// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = () => {
  document.body.addEventListener('wheel', (e) => {
    chrome.test.sendMessage('Seen wheel event');
  });

  chrome.test.sendMessage('Launched');
};
