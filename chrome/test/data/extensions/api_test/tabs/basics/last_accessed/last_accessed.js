// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TEST_URL_1 = 'https://example.com/1';
var TEST_URL_2 = 'https://example.com/2';
var TEST_URL_3 = 'https://example.com/3';

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
  async function testLastAccessedForMovedTab() {
    let firstTabCreatedTime = Date.now();
    // Adding a short delay to ensure different access times for each tab.
    await sleep(1);

    const firstTab = await createAndSelectTab(TEST_URL_1);
    assertTrue(firstTab.active);
    assertTrue(firstTabCreatedTime < firstTab.lastAccessed);

    let secondTabCreatedTime = Date.now();
    await sleep(1);

    const secondTab = await createAndSelectTab(TEST_URL_2);
    assertTrue(secondTab.active);
    assertTrue(secondTabCreatedTime < secondTab.lastAccessed);

    let thirdTabCreatedTime = Date.now();
    await sleep(1);

    const thirdTab = await createAndSelectTab(TEST_URL_3);
    assertTrue(thirdTab.active);
    assertTrue(thirdTabCreatedTime < thirdTab.lastAccessed);

    assertTrue(firstTab.lastAccessed < secondTab.lastAccessed);
    assertTrue(secondTab.lastAccessed < thirdTab.lastAccessed);

    // Store the lastAccessed times before moving the tab.
    const firstTabLastAccessedBeforeMove = firstTab.lastAccessed;
    const secondTabLastAccessedBeforeMove = secondTab.lastAccessed;
    const thirdTabLastAccessedBeforeMove = thirdTab.lastAccessed;

    // Adding a delay to ensure that move operation happens after some time.
    await sleep(1);

    // Move second tab to the first index.
    await new Promise((resolve) => {
      chrome.tabs.move(secondTab.id, { index: 0 }, (movedTab) => {
        resolve(movedTab);
      });
    });

    // Adding a short delay to ensure the move operation completes.
    await sleep(1);

    // Refresh the tab info to get the updated lastAccessed times.
    const updatedTabs = await new Promise((resolve) => {
      chrome.tabs.query({}, resolve);
    });

    const updatedFirstTab = updatedTabs.find(tab => tab.id === firstTab.id);
    const updatedSecondTab = updatedTabs.find(tab => tab.id === secondTab.id);
    const updatedThirdTab = updatedTabs.find(tab => tab.id === thirdTab.id);

    // Check if the move was successful.
    const firstIndexTab = updatedTabs.find(tab => tab.index === 0);
    assertTrue(firstIndexTab.id === secondTab.id);

    // Check that the lastAccessed times have not changed.
    assertEq(updatedFirstTab.lastAccessed, firstTabLastAccessedBeforeMove);
    assertEq(updatedSecondTab.lastAccessed, secondTabLastAccessedBeforeMove);
    assertEq(updatedThirdTab.lastAccessed, thirdTabLastAccessedBeforeMove);

    chrome.test.succeed();
  },
]);
});
