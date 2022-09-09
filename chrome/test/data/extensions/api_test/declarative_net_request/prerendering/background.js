// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let testServerPort = 0;
let matchedRuleInfos = [];
let expectedCallback = null;

const onRuleMatchedDebugCallback = info => {
  if (expectedCallback)
    expectedCallback(info);
};

function navigateAndWaitForUrlMatch(navigatePath, waitPath) {
  matchedRuleInfos = [];
  const baseUrl = `http://a.test:${testServerPort}/extensions/api_test/` +
      `declarative_net_request/prerendering/`;
  const waitUrl = baseUrl + waitPath;

  return new Promise(resolve => {
    expectedCallback = info => {
      matchedRuleInfos.push(info);
      if (info.request.url == waitUrl)
        resolve();
    };
    chrome.tabs.update({url: baseUrl + navigatePath});
  });
}

async function setup() {
  chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(
      onRuleMatchedDebugCallback);
  // Wait for a round trip to ensure the listener is properly added in the
  // browser process before initiating any requests.
  chrome.test.waitForRoundTrip('msg', () => {
    // Setup the dynamic rules for testing. We don't use static rules as we
    // don't have a clear approach to wait until the rules are set up.
    chrome.declarativeNetRequest.updateDynamicRules(
        {
          addRules: [
            {
              'id': 1,
              'priority': 1,
              'condition': {
                'urlFilter': 'block_main',
                'resourceTypes': ['main_frame']

              },
              'action': {'type': 'block'}
            },
            {
              'id': 2,
              'priority': 1,
              'condition':
                  {'urlFilter': 'block_sub', 'resourceTypes': ['sub_frame']},
              'action': {'type': 'block'}
            },
            {
              'id': 3,
              'priority': 1,
              'condition':
                  {'urlFilter': 'block_image', 'resourceTypes': ['image']},
              'action': {'type': 'block'}
            }
          ]
        },
        chrome.test.succeed);
  });
}

async function testBlockPrerendering() {
  await navigateAndWaitForUrlMatch('prerender_blocked_url.html', 'block_main');
  chrome.test.assertEq(1, matchedRuleInfos.length);
  chrome.test.succeed();
}

async function testBlockSubframeRequestFromPrerenderedPage() {
  await navigateAndWaitForUrlMatch(
      'prerender_allowed_url_with_iframes.html', 'block_sub');
  chrome.test.assertEq(1, matchedRuleInfos.length);
  chrome.test.succeed();
}

async function testBlockImageRequestFromPrerenderedPage() {
  await navigateAndWaitForUrlMatch(
      'prerender_allowed_url_with_image.html', 'block_image');
  chrome.test.assertEq(1, matchedRuleInfos.length);
  chrome.test.succeed();
}

chrome.test.getConfig(async config => {
  testServerPort = config.testServer.port;
  chrome.test.runTests([
    setup,
    testBlockPrerendering,
    testBlockSubframeRequestFromPrerenderedPage,
    testBlockImageRequestFromPrerenderedPage,
  ]);
});
