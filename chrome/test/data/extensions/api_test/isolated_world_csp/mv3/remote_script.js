// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(config => {
  const port = config.testServer.port;

  const script = document.createElement('script');
  script.src = `http://example.com:${port}/script.js`;
  script.onload = () => {
    chrome.test.notifyFail('Script load succeeded unexpectedly');
  };
  script.onerror = () => {
    chrome.test.notifyPass();
  };

  document.body.appendChild(script);
});
