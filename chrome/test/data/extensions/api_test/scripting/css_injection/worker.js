// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const CSS_GREEN = 'body { background-color: green !important }';
const GREEN = 'rgb(0, 128, 0)';
const CSS_RED = 'body { background-color: red !important }';
const RED = 'rgb(255, 0, 0)';
const CSS_BLUE = 'body { background-color: blue !important }';
const BLUE = 'rgb(0, 0, 255)';
const CSS_CYAN = 'body { background-color: cyan !important }';
const CYAN = 'rgb(0, 255, 255)';
const YELLOW = 'rgb(255, 255, 0)';

function getBodyColor() {
  const hostname = (new URL(location.href)).hostname;
  return hostname + ' ' + getComputedStyle(document.body).backgroundColor;
}

async function getSingleTab(query) {
  const tabs = await chrome.tabs.query(query);
  chrome.test.assertEq(1, tabs.length);
  return tabs[0];
}

async function getBodyColorsForTab(tabId) {
  const results = await chrome.scripting.executeScript({
    target: {
      tabId: tabId,
      allFrames: true,
    },
    func: getBodyColor,
  });
  return results.map(res => res.result);
}

chrome.test.runTests([
  // NOTE: These tests re-inject into (potentially) the same frames. This isn't
  // a major problem, because more-recent stylesheets take precedent over
  // previously-inserted ones, but it does put a somewhat unfortunate slight
  // dependency between subtests. If this becomes a problem, we could reload
  // tabs to ensure a "clean slate", but it's not worth the added complexity
  // yet.
  // Instead, each test uses a different color.
  async function changeBackgroundFromString() {
    const query = {url: 'http://example.com/*'};
    const tab = await getSingleTab(query);
    const results = await chrome.scripting.insertCSS({
      target: {
        tabId: tab.id,
      },
      css: CSS_GREEN,
    });
    chrome.test.assertEq(undefined, results);
    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(1, colors.length);
    chrome.test.assertEq(`example.com ${GREEN}`, colors[0]);
    chrome.test.succeed();
  },

  async function subframes() {
    const query = {url: 'http://subframes.example/*'};
    const tab = await getSingleTab(query);
    const results = await chrome.scripting.insertCSS({
      target: {
        tabId: tab.id,
        allFrames: true,
      },
      css: CSS_RED,
    });
    chrome.test.assertEq(undefined, results);
    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(2, colors.length);
    colors.sort();
    // Note: injected only in b.com and subframes.example, not c.com (which
    // the extension doesn't have permission to).
    chrome.test.assertEq(`b.com ${RED}`, colors[0]);
    chrome.test.assertEq(`subframes.example ${RED}`, colors[1]);
    chrome.test.succeed();
  },

  async function specificFrames() {
    const query = {url: 'http://subframes.example/*'};
    const tab = await getSingleTab(query);
    const frames = await new Promise(resolve => {
      chrome.webNavigation.getAllFrames({tabId: tab.id}, resolve);
    });
    const bComFrame = frames.find(frame => {
      return (new URL(frame.url)).hostname == 'b.com';
    });
    chrome.test.assertTrue(!!bComFrame);

    const results = await chrome.scripting.insertCSS({
      target: {
        tabId: tab.id,
        frameIds: [bComFrame.frameId],
      },
      css: CSS_BLUE,
    });
    chrome.test.assertEq(undefined, results);

    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(2, colors.length);
    colors.sort();
    chrome.test.assertEq(`b.com ${BLUE}`, colors[0]);
    // NOTE: subframes.example frame is still red from the previous test.
    chrome.test.assertEq(`subframes.example ${RED}`, colors[1]);
    chrome.test.succeed();
  },

  async function changeBackgroundFromFile() {
    const query = {url: 'http://example.com/*'};
    const tab = await getSingleTab(query);
    const results = await chrome.scripting.insertCSS({
      target: {
        tabId: tab.id,
      },
      files: ['css_file.css'],
    });
    chrome.test.assertEq(undefined, results);
    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(1, colors.length);
    chrome.test.assertEq(`example.com ${YELLOW}`, colors[0]);
    chrome.test.succeed();
  },

  async function multipleFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    const tab = await getSingleTab(query);
    const target = {tabId: tab.id};
    // Inject multiple files. css_file2.css sets the background color to purple
    // and also sets font size to 1337px. Then, css_file.css sets the background
    // to yellow.
    // Since stylesheets inject in-order, the end result should be that the
    // font size is 1337px (from css_file2.css) and the background is yellow
    // (from css_file.css, the last to run).
    const results = await chrome.scripting.insertCSS({
      target: target,
      files: ['css_file2.css', 'css_file.css'],
    });
    chrome.test.assertEq(undefined, results);
    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(1, colors.length);
    chrome.test.assertEq(`example.com ${YELLOW}`, colors[0]);

    const fontSizes = await chrome.scripting.executeScript({
      target: target,
      func: function() { return getComputedStyle(document.body).fontSize; },
    });

    chrome.test.assertEq(1, fontSizes.length);
    chrome.test.assertEq('1337px', fontSizes[0].result);

    chrome.test.succeed();
  },

  async function noSuchTab() {
    const nonExistentTabId = 99999;
    await chrome.test.assertPromiseRejects(
        chrome.scripting.insertCSS({
          target: {
            tabId: nonExistentTabId,
          },
          css: CSS_CYAN,
        }),
        `Error: No tab with id: ${nonExistentTabId}`);
    chrome.test.succeed();
  },

  async function noSuchFile() {
    const noSuchFile = 'no_such_file.css';
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.insertCSS({
          target: {
            tabId: tab.id,
          },
          files: [noSuchFile],
        }),
        `Error: Could not load file: '${noSuchFile}'.`);
    chrome.test.succeed();
  },

  async function noFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.insertCSS({
          target: {
            tabId: tab.id,
          },
          files: [],
        }),
        'Error: At least one file must be specified.');
    chrome.test.succeed();
  },

  async function duplicateFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.insertCSS({
          target: {
            tabId: tab.id,
          },
          files: ['css_file.js', 'css_file.js'],
        }),
        `Error: Duplicate file specified: 'css_file.js'.`);

    // Try again with a preceding slash.
    await chrome.test.assertPromiseRejects(
        chrome.scripting.insertCSS({
          target: {
            tabId: tab.id,
          },
          files: ['css_file.js', '/css_file.js'],
        }),
        `Error: Duplicate file specified: '/css_file.js'.`);
    chrome.test.succeed();
  },

  async function disallowedPermission() {
    const query = {url: 'http://chromium.org/*'};
    const tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.insertCSS({
          target: {
            tabId: tab.id,
          },
          css: CSS_CYAN,
        }),
        `Error: Cannot access contents of url "${tab.url}". ` +
            'Extension manifest must request permission ' +
            'to access this host.');
    chrome.test.succeed();
  },
]);
