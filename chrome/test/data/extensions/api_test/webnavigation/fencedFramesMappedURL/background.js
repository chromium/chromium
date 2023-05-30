// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = "_test_resources/api_test/webnavigation/framework.js";
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(() => {
  chrome.test.sendMessage("ready", async function (response) {
    let config = await promise(chrome.test.getConfig);
    let port = config.testServer.port;
    let actual_fenced_frame_url =
      `https://b.test:${port}` +
      `/extensions/api_test/webnavigation/fencedFramesMappedURL/frame.html`;

    chrome.test.runTests([
      () => {
        chrome.test.listenOnce(
          chrome.webNavigation.onBeforeNavigate,
          function (details) {
            chrome.test.assertEq(details.frameType, "fenced_frame");
            chrome.test.assertEq(details.url, actual_fenced_frame_url);
          }
        );
      },
      () => {
        chrome.test.listenOnce(
          chrome.webNavigation.onCommitted,
          function (details) {
            chrome.test.assertEq(details.frameType, "fenced_frame");
            chrome.test.assertEq(details.url, actual_fenced_frame_url);
          }
        );
      },
    ]);

    chrome.test.sendMessage("ready");
  });
});
