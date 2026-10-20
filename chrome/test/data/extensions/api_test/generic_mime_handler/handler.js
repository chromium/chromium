// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function getStreamInfoAndFetchStream() {
    const info = await chrome.mimeHandler.getStreamInfo();

    // The ?action query of the PDF URL picks which setMimeHandlerOptions() call
    // the handler makes.
    const optionsByAction = {
      'disable': {enabled: false},
      'disable-embedded': {canEmbed: false},
    };
    const options =
        optionsByAction[new URL(info.originalUrl).searchParams.get('action')];
    if (options) {
      await chrome.mimeHandler.setMimeHandlerOptions(
          'application/pdf', options);
      const storedOptions =
          await chrome.mimeHandler.getMimeHandlerOptions('application/pdf');
      for (const [member, value] of Object.entries(options)) {
        chrome.test.assertEq(value, storedOptions[member]);
      }
      chrome.test.succeed();
      return;
    }

    chrome.test.assertEq('application/pdf', info.mimeType);
    chrome.test.assertTrue(info.tabId >= 0);
    chrome.test.assertTrue(info.originalUrl.length > 0);
    chrome.test.assertTrue(info.streamUrl.length > 0);

    const response = await fetch(info.streamUrl);
    chrome.test.assertEq(200, response.status);

    // Record what the stream fetch exposed. The browser-side test decides
    // which names are allowed.
    window.streamFetchHeaderNames = [...response.headers.keys()];

    const buffer = await response.arrayBuffer();
    const header = new Uint8Array(buffer, 0, 5);
    const magic = String.fromCharCode(...header);
    chrome.test.assertEq('%PDF-', magic);
    chrome.test.succeed();
  },
]);
