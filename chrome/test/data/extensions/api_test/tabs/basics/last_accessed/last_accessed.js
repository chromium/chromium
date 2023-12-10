// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TEST_URL_1 = 'https://example.com/1';
var TEST_URL_2 = 'https://example.com/2';

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function createAndSelectTab(url) {
  return new Promise((resolve) => {
    chrome.tabs.create({ url: url }, (tab) => {
      chrome.tabs.update(tab.id, { active: true }, function(selectedTab) {
        resolve(selectedTab);
      });
    });
  });
}

loadScript.then(async function() {
chrome.test.runTests([
  async function testLastAccessedForTwoTabs() {
    let firstTabCreatedTime = Date.now();

    const firstTab = await createAndSelectTab(TEST_URL_1);
    assertTrue(firstTab.active);
    assertTrue(firstTabCreatedTime < firstTab.lastAccessed);

    let secondTabCreatedTime = Date.now();
    // Adding a short delay to ensure different access times for each tab.
    await sleep(1);

    const secondTab = await createAndSelectTab(TEST_URL_2);
    assertTrue(secondTab.active);
    assertTrue(secondTabCreatedTime < secondTab.lastAccessed);

    assertTrue(firstTab.lastAccessed < secondTab.lastAccessed);
    chrome.test.succeed();
  },
]);
});
