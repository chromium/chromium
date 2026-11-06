// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

(async () => {
  const config = await chrome.test.getConfig();
  const mode = config.customArg || 'getstate_ineligible';

  const origin = '127.0.0.1';
  const path = '/simple.html';

  async function navigateTo(origin, path) {
    const url = `http://${origin}:${config.testServer.port}${path}`;
    return await openTab(url);
  }

  try {
    let documentId;
    if (mode === 'launch_panel_popup_window') {
      const url = `http://${origin}:${config.testServer.port}${path}`;
      const tabId = await new Promise((resolve) => {
        let createdTabId;
        const completedTabIds = [];
        const listener = (id, changeInfo) => {
          if (changeInfo.status !== 'complete') {
            return;
          }
          if (createdTabId === undefined) {
            completedTabIds.push(id);
            return;
          }
          if (id === createdTabId) {
            chrome.tabs.onUpdated.removeListener(listener);
            resolve(id);
          }
        };
        chrome.tabs.onUpdated.addListener(listener);
        chrome.windows.create({url: url, type: 'popup'}, (popupWindow) => {
          const tab = popupWindow.tabs[0];
          if (tab.status === 'complete' || completedTabIds.includes(tab.id)) {
            chrome.tabs.onUpdated.removeListener(listener);
            resolve(tab.id);
          } else {
            createdTabId = tab.id;
          }
        });
      });
      const frame =
          await chrome.webNavigation.getFrame({frameId: 0, tabId: tabId});
      documentId = frame.documentId;
    } else if (mode === 'aba_only_bindings') {
      tests_runAbaOnlyBindings();
      return;
    } else if (mode === 'register_promo_invalid_document_id') {
      documentId = 'not_a_document_id';
    } else if (mode === 'register_promo_subframe') {
      const newTab = await navigateTo(origin, '/iframe');
      const frames =
          await chrome.webNavigation.getAllFrames({tabId: newTab.id});
      const subframe = frames.find((frame) => frame.frameId !== 0);
      documentId = subframe?.documentId;
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
      case 'launch_panel_popup_window':
        tests_runLaunchPanelInNewTabPopupWindow(documentId);
        return;
      case 'launch_panel_missing_mstk':
        tests_runLaunchPanelInNewTabMissingMstk(documentId);
        return;
      case 'register_promo_disabled':
        tests_runRegisterAimThreadForPromoRejected(
            documentId, 'token', 'Error: ABA search journeys are disabled');
        return;
      case 'register_promo_ineligible':
        tests_runRegisterAimThreadForPromoRejected(
            documentId, 'token',
            'Error: ContextualTasks Private API is not eligible for this ' +
                'profile');
        return;
      case 'register_promo_missing_mstk':
        tests_runRegisterAimThreadForPromoRejected(
            documentId, '', 'Error: Missing required URL params');
        return;
      case 'register_promo_invalid_document_id':
        tests_runRegisterAimThreadForPromoRejected(
            documentId, 'token', 'Error: Invalid frame.');
        return;
      case 'register_promo_subframe':
        tests_runRegisterAimThreadForPromoRejected(
            documentId, 'token', 'Error: Not top-level frame');
        return;
      case 'register_promo_non_search_results_page':
        tests_runRegisterAimThreadForPromoRejected(
            documentId, 'token',
            'Error: Contextual Tasks are only supported on Search Results ' +
                'pages.');
        return;
      case 'register_promo_without_query':
        tests_runRegisterAimThreadForPromoRejected(
            documentId, 'token',
            'Error: Unable to read the search query of the page');
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
        aimParams: {
          ntc: '',
          mstk: '',
          aioh: '',
          csuir: '',
          ved: '',
          cs: '',
          sxsrf: '',
          ei: '',
        },
        targetUrl: 'https://example.com',
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
        // Note: contextual_tasks_private_apitest.cc will verify that these
        // parameters are correctly passed on.
        aimParams: {
          ntc: '1',
          mstk: 'abc',
          aioh: '1',
          csuir: '1',
          ved: '123',
          cs: '1',
          sxsrf: 'xyz',
          ei: '456',
          q: 'some_query',
        },
        targetUrl: 'https://example.com',
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
        aimParams: {
          ntc: '',
          mstk: 'dummy',
          aioh: '',
          csuir: '',
          ved: '',
          cs: '',
          sxsrf: '',
          ei: '',
        },
        targetUrl: 'http://example.com',
        documentId: documentId,
      };
      await chrome.test.assertPromiseRejects(
          chrome.contextualTasksPrivate.launchPanelInNewTab(details),
          'Error: URLs must be valid and use HTTPS');
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
        aimParams: {
          ntc: '',
          mstk: 'dummy',
          aioh: '',
          csuir: '',
          ved: '',
          cs: '',
          sxsrf: '',
          ei: '',
        },
        targetUrl: 'https://example.com',
        documentId: documentId,
      };
      await chrome.test.assertPromiseRejects(
          chrome.contextualTasksPrivate.launchPanelInNewTab(details),
          expectedError);
      chrome.test.succeed();
    },
  ]);
}

function tests_runLaunchPanelInNewTabMissingMstk(documentId) {
  chrome.test.runTests([
    async function testLaunchPanelInNewTabMissingMstk() {
      const details = {
        aimParams: {
          ntc: '1',
          // mstk is missing
          aioh: '1',
          csuir: '1',
          ved: '123',
          cs: '1',
          sxsrf: 'xyz',
          ei: '456',
        },
        targetUrl: 'https://example.com',
        documentId: documentId,
      };
      await chrome.test.assertPromiseRejects(
          chrome.contextualTasksPrivate.launchPanelInNewTab(details),
          'Error: Missing required URL params');
      chrome.test.succeed();
    },
  ]);
}

// Calls `registerAimThreadForPromo()` with `mstk` and expects it to reject with
// `expectedError`.
function tests_runRegisterAimThreadForPromoRejected(
    documentId, mstk, expectedError) {
  chrome.test.runTests([
    async function testRegisterAimThreadForPromoRejected() {
      const details = {
        aimPromoParams: {mstk: mstk, aioh: '3', csuir: '1', sxsrf: 'xsrf'},
        queryHints: ['edu_homework'],
        documentId: documentId,
      };
      await chrome.test.assertPromiseRejects(
          chrome.contextualTasksPrivate.registerAimThreadForPromo(details),
          expectedError);
      chrome.test.succeed();
    },
  ]);
}

function tests_runAbaOnlyBindings() {
  chrome.test.runTests([
    function testAbaOnlyBindings() {
      chrome.test.assertEq(
          'function',
          typeof chrome.contextualTasksPrivate.registerAimThreadForPromo);
      chrome.test.assertEq(
          'undefined', typeof chrome.contextualTasksPrivate.getState);
      chrome.test.assertEq(
          'undefined',
          typeof chrome.contextualTasksPrivate.launchPanelInNewTab);
      chrome.test.succeed();
    },
  ]);
}
