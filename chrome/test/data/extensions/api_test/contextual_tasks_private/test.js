// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

(async () => {
  const config = await chrome.test.getConfig();
  const mode = config.customArg || 'getstate_ineligible';

  const origin = 'example.com';
  const path = '/simple.html';

  async function navigateTo(origin, path) {
    const url = `http://${origin}:${config.testServer.port}${path}`;
    return await openTab(url);
  }

  try {
    let documentId;
    if (mode === 'launch_panel_popup_window') {
      const url = `http://${origin}:${config.testServer.port}${path}`;
      const popupWindow =
          await chrome.windows.create({url: url, type: 'popup'});
      const tabId = popupWindow.tabs[0].id;
      const tab = await chrome.tabs.get(tabId);
      if (tab.status !== 'complete') {
        await new Promise((resolve) => {
          chrome.tabs.onUpdated.addListener(function listener(id, changeInfo) {
            if (id === tabId && changeInfo.status === 'complete') {
              chrome.tabs.onUpdated.removeListener(listener);
              resolve();
            }
          });
        });
      }
      const frame =
          await chrome.webNavigation.getFrame({frameId: 0, tabId: tabId});
      documentId = frame.documentId;
    } else {
      const newTab = await navigateTo(origin, path);
      const frame =
          await chrome.webNavigation.getFrame({frameId: 0, tabId: newTab.id});
      documentId = frame.documentId;
    }

    if (!documentId) {
      chrome.test.fail('documentId is required for API tests');
      return;
    }

    switch (mode) {
      case 'getstate_eligible':
        tests_runGetStateEligible(documentId);
        return;
      case 'launch_panel_eligible':
        tests_runLaunchPanelInNewTabEligible(documentId);
        return;
      case 'getstate_ineligible':
        tests_runGetStateIneligible(documentId);
        return;
      case 'launch_panel_ineligible':
        tests_runLaunchPanelInNewTabIneligible(documentId);
        return;
      case 'launch_panel_invalid_target_url':
        tests_runLaunchPanelInNewTabInvalidTargetUrl(documentId);
        return;
      case 'launch_panel_invalid_aim_url':
        tests_runLaunchPanelInNewTabInvalidAimUrl(documentId);
        return;
      case 'launch_panel_popup_window':
        tests_runLaunchPanelInNewTabPopupWindow(documentId);
        return;
      default:
        chrome.test.fail('Unexpected mode: ' + mode);
        return;
    }
  } catch (e) {
    chrome.test.fail(e.message);
  }
})();

function tests_runGetStateEligible(documentId) {
  chrome.test.runTests([
    async function testGetStateEligible() {
      const state = await chrome.contextualTasksPrivate.getState(documentId);
      chrome.test.assertEq({isEligible: true}, state);
      chrome.test.succeed();
    },
  ]);
}

function tests_runGetStateIneligible(documentId) {
  chrome.test.runTests([
    async function testGetStateIneligible() {
      const state = await chrome.contextualTasksPrivate.getState(documentId);
      chrome.test.assertEq({isEligible: false}, state);
      chrome.test.succeed();
    },
  ]);
}

function tests_runLaunchPanelInNewTabIneligible(documentId) {
  const expectedError =
      'Error: ContextualTasks Private API is not eligible for this profile';
  chrome.test.runTests([
    async function testLaunchPanelInNewTabIneligible() {
      const details = {
        targetUrl: 'https://example.com',
        aimUrl: 'https://google.com/aim',
        documentId: documentId,
      };
      await chrome.test.assertPromiseRejects(
          chrome.contextualTasksPrivate.launchPanelInNewTab(details),
          expectedError);
      chrome.test.succeed();
    },
  ]);
}

function tests_runLaunchPanelInNewTabEligible(documentId) {
  chrome.test.runTests([
    async function testLaunchPanelInNewTabEligible() {
      const details = {
        targetUrl: 'https://example.com',
        aimUrl: 'https://google.com/aim',
        documentId: documentId,
      };
      await chrome.contextualTasksPrivate.launchPanelInNewTab(details);
      chrome.test.succeed();
    },
  ]);
}

function tests_runLaunchPanelInNewTabInvalidTargetUrl(documentId) {
  chrome.test.runTests([
    async function testLaunchPanelInNewTabInvalidTargetUrl() {
      const details = {
        targetUrl: 'http://example.com',
        aimUrl: 'https://google.com/aim',
        documentId: documentId,
      };
      await chrome.test.assertPromiseRejects(
          chrome.contextualTasksPrivate.launchPanelInNewTab(details),
          'Error: URLs must be valid and use HTTPS');
      chrome.test.succeed();
    },
  ]);
}

function tests_runLaunchPanelInNewTabInvalidAimUrl(documentId) {
  chrome.test.runTests([
    async function testLaunchPanelInNewTabInvalidAimUrl() {
      const details = {
        targetUrl: 'https://example.com',
        aimUrl: 'https://google.com/search',
        documentId: documentId,
      };
      await chrome.test.assertPromiseRejects(
          chrome.contextualTasksPrivate.launchPanelInNewTab(details),
          'Error: Invalid AI URL');
      chrome.test.succeed();
    },
  ]);
}

function tests_runLaunchPanelInNewTabPopupWindow(documentId) {
  const expectedError =
      'Error: Contextual Tasks are only supported in normal browser windows.';
  chrome.test.runTests([
    async function testLaunchPanelInNewTabPopupWindow() {
      const details = {
        targetUrl: 'https://example.com',
        aimUrl: 'https://google.com/aim',
        documentId: documentId,
      };
      await chrome.test.assertPromiseRejects(
          chrome.contextualTasksPrivate.launchPanelInNewTab(details),
          expectedError);
      chrome.test.succeed();
    },
  ]);
}
