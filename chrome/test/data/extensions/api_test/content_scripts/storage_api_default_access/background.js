// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function testSetAndGetValue(area) {
  const expectedEntry = {background: area};
  await new Promise((resolve) => {
    chrome.storage[area].set(expectedEntry, resolve);
  });
  const actualEntry = await new Promise((resolve) => {
    chrome.storage[area].get('background', resolve);
  });
  chrome.test.assertEq(expectedEntry, actualEntry);
}

chrome.test.runTests([
  async function setValuesInBackgroundPage() {
    await testSetAndGetValue('session');
    await testSetAndGetValue('local');
    await testSetAndGetValue('sync');
    chrome.test.succeed();
  },
]);
