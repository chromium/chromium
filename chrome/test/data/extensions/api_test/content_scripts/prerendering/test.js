// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let testServerPort = 0;

// Obtains a full URL that match a configuration that meets the expectations for
// options described in `details`.
function getUrl(path, details) {
  let host = 'default.test';
  if (details && details.matchAboutBlank)
    host = 'match_about_blank.test';
  const url = `http://${host}:${testServerPort}/extensions/${path}`;
  return url;
}

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
      // TODO(crbug.com/40208062): `chrome.tabs.update` can not activate
      // the pre-rendered page, but takes a new navigation instead.
      const url = getUrl('test_file_with_iframe.html');
      chrome.tabs.executeScript({code: `location.href = '${url}';`});
    }
  };
  chrome.runtime.onMessage.addListener(testCallback);
  chrome.tabs.update({url: getUrl('test_file_with_prerendering.html')});
}

// Tests receiving messages from a content script to ensure pre-rendered frames
// including `about:blank` are correctly handled with or without
// `match_about_blank`. For the initiator page,
// `test_file_with_prerendering_page_with_about_blank_iframe.html`, we expect
// receiving a `top_frame_only` and an `all_frames` messages. Prerendered page,
// `test_wile_with_about_blank_iframe.html` has an `about:blank` iframe. So, we
// expect receiving a `top_frame_only` and a `all_frames` from the main page,
// and iff `match_about_blank` specified, it receives another `all_frames` from
// `about:blank` page. In total, we expect two `top_frame_only` and two or three
// `all_frames_ messages respectively for the cases with and without
// `match_about_blank`. After the final page activation, we receive `activated`.
async function testWithAboutBlankIframe(details) {
  const matchAboutBlank = details && details.matchAboutBlank;
  const expectedNumAllFramesMessages = 2 + (matchAboutBlank ? 1 : 0);
  const expectedNumTopFrameOnlyMessages = 2;
  let numAllFramesMessages = 0;
  let numTopFrameOnlyMessages = 0;
  const testCallback = (message, sender, sendResponse) => {
    if (message == 'all_frames') {
      numAllFramesMessages++;
      chrome.test.assertTrue(
          numAllFramesMessages <= expectedNumAllFramesMessages,
          'Unexpected: maybe running on about:blank');
    } else if (message == 'top_frame_only') {
      numTopFrameOnlyMessages++;
      chrome.test.assertTrue(
          numAllFramesMessages <= expectedNumTopFrameOnlyMessages,
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
            chrome.test.assertEq(
                expectedNumAllFramesMessages, numAllFramesMessages);
            chrome.test.assertEq(
                expectedNumTopFrameOnlyMessages, numTopFrameOnlyMessages);
            chrome.test.succeed();
          });
    } else {
      chrome.runtime.onMessage.removeListener(testCallback);
      chrome.test.fail('Unexpected message: ' + JSON.stringify(message));
    }
    if (numAllFramesMessages == expectedNumAllFramesMessages &&
        numTopFrameOnlyMessages == expectedNumTopFrameOnlyMessages) {
      // Navigate to the pre-rendered page.
      // TODO(crbug.com/40208062): `chrome.tabs.update` can not activate
      // the pre-rendered page, but takes a new navigation instead.
      const url = getUrl('test_file_with_about_blank_iframe.html', details);
      chrome.tabs.executeScript({code: `location.href = '${url}';`});
    }
  };
  chrome.runtime.onMessage.addListener(testCallback);
  const url = getUrl(
      'test_file_with_prerendering_page_with_about_blank_iframe.html', details);
  chrome.tabs.update({url: url});
}

chrome.test.getConfig(async config => {
  testServerPort = config.testServer.port;

  // TODO(https://crbug.com/3731231): Add more tests for
  // `match_origin_as_fallback` and manifest v3.
  chrome.test.runTests([
    testWithIframe,
    // TODO(crbug.com/40853029): These two tests are flaky and time out.
    // testWithAboutBlankIframe,
    // testWithAboutBlankIframe.bind(this, {matchAboutBlank: true}),
  ]);
});
