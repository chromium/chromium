// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let testBaseUrl = null;

// Tests receiving messages from a content script to ensure pre-rendered frames
// are correctly handled with and without `all_frames`.
// For the initiator page, `test_file_with_prerendering.html`, we expect
// receiving a `top_frame_only` and an `all_frames` messages. Prerendered page,
// `test_wile_with_iframe.html` has an iframe. So, we expect receiving a
// `top_frame_only` and two `all_frames` messages. In total, we expect two
// `top_frame_only` and three `all_frames` messages. After the final page
// activation, we receive `activated`.
async function testWithIframe() {
  let numAllFramesMessages = 0;
  let numTopFrameOnlyMessages = 0;
  const testCallback = (message, sender, sendResponse) => {
    if (message == 'all_frames') {
      numAllFramesMessages++;
      chrome.test.assertTrue(numAllFramesMessages <= 3);
    } else if (message == 'top_frame_only') {
      numTopFrameOnlyMessages++;
      chrome.test.assertTrue(
          numTopFrameOnlyMessages <= 2,
          'Unexpected: maybe wrong injection on the activation');
    } else if (message == 'activated') {
      chrome.runtime.onMessage.removeListener(testCallback);
      // Inject a second script into the now-activated frame, but run it at
      // document_idle. This ensures that any content scripts that will run on
      // the frame have already done so, since they run at document_start.
      chrome.tabs.executeScript(
          {
            code: '// Empty',
            runAt: 'document_idle',
          },
          () => {
            chrome.test.assertEq(3, numAllFramesMessages);
            chrome.test.assertEq(2, numTopFrameOnlyMessages);
            chrome.test.succeed();
          });
    } else {
      chrome.runtime.onMessage.removeListener(testCallback);
      chrome.test.fail('Unexpected message: ' + JSON.stringify(message));
    }

    if (numAllFramesMessages == 3 && numTopFrameOnlyMessages == 2) {
      // Navigate to the pre-rendered page.
      // TODO(https://crbug.com/1278141): `chrome.tabs.update` can not activate
      // the pre-rendered page, but takes a new navigation instead.
      const url = testBaseUrl + 'test_file_with_iframe.html';
      chrome.tabs.executeScript({code: `location.href = '${url}';`});
    }
  };
  chrome.runtime.onMessage.addListener(testCallback);
  chrome.tabs.update({url: testBaseUrl + 'test_file_with_prerendering.html'});
}

chrome.test.getConfig(async config => {
  const port = config.testServer.port;
  testBaseUrl = `http://localhost:${port}/extensions/`;

  // TODO(https://crbug.com/3731231): Add more tests for `match_about_blank` and
  // `match_origin_as_fallback`.
  chrome.test.runTests([
    testWithIframe,
  ]);
});
