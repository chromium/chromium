// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Event listeners need to be added synchronously at startup in order to survive
// a service worker shutdown.
chrome.webAuthenticationProxy.onRemoteSessionStateChange.addListener(() => {
  chrome.test.succeed();
});
