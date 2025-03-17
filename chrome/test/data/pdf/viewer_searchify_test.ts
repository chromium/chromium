// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController, PluginControllerEventType, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {setupMockMetricsPrivate} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const eventTarget = PluginController.getInstance().getEventTarget();

function sendShowSearchifyProgress(show: boolean) {
  eventTarget.dispatchEvent(new CustomEvent(
      PluginControllerEventType.PLUGIN_MESSAGE,
      {detail: {type: 'showSearchifyInProgress', show: show}}));
}

function sendCopyCommand() {
  eventTarget.dispatchEvent(new CustomEvent(
      PluginControllerEventType.PLUGIN_MESSAGE,
      {detail: {type: 'executedEditCommand', editCommand: 'Copy'}}));
}

function sendFindStartedMessage() {
  eventTarget.dispatchEvent(new CustomEvent(
      PluginControllerEventType.PLUGIN_MESSAGE,
      {detail: {type: 'startedFindInPage'}}));
}

function sendSetHasSearchifiedMessage() {
  eventTarget.dispatchEvent(new CustomEvent(
      PluginControllerEventType.PLUGIN_MESSAGE,
      {detail: {type: 'setHasSearchifyText'}}));
}

const tests = [
  // Verifies that sending `showSearchifyInProgress` with `show=true` opens
  // the progress indicator.
  function testShowSearchifyProgress() {
    sendShowSearchifyProgress(true);
    chrome.test.assertTrue(viewer.$.searchifyProgress.open);
    chrome.test.succeed();
  },

  // Verifies that sending `showSearchifyInProgress` with `show=false` closes
  // the progress indicator.
  function testHideSearchifyProgress() {
    sendShowSearchifyProgress(false);
    chrome.test.assertFalse(viewer.$.searchifyProgress.open);
    chrome.test.succeed();
  },

  // Verifies that progress indicator is re-opened after it's closed.
  function testReshowSearchifyProgress() {
    sendShowSearchifyProgress(false);
    sendShowSearchifyProgress(true);
    chrome.test.assertTrue(viewer.$.searchifyProgress.open);
    chrome.test.succeed();
  },

  // Verifies that COPY_SEARCHIFIED and FIND_IN_PAGE_SEARCHIFIED operations are
  // not recorded when document is not searchified.
  function testMetricsNotSearchified() {
    const mockMetricsPrivate = setupMockMetricsPrivate();

    // Copy and Find commands are sent more than once, to verify that COPY_FIRST
    // and FIND_IN_PAGE_FIRST are recorded only once.
    sendCopyCommand();
    sendCopyCommand();

    sendFindStartedMessage();
    sendFindStartedMessage();

    const expectations: Array<[UserAction, number]> = [
      [UserAction.COPY, 2],
      [UserAction.COPY_FIRST, 1],
      [UserAction.FIND_IN_PAGE, 2],
      [UserAction.FIND_IN_PAGE_FIRST, 1],
    ];
    chrome.test.assertEq(
        expectations.length, mockMetricsPrivate.actionCounter.size);

    expectations.forEach(([action, value]) => {
      chrome.test.assertEq(mockMetricsPrivate.actionCounter.get(action), value);
    });

    chrome.test.succeed();
  },

  // Verifies that COPY_SEARCHIFIED and FIND_IN_PAGE_SEARCHIFIED operations are
  // recorded when document is searchified.
  function testMetricsSearchified() {
    const mockMetricsPrivate = setupMockMetricsPrivate();

    sendSetHasSearchifiedMessage();

    sendCopyCommand();
    sendCopyCommand();

    sendFindStartedMessage();
    sendFindStartedMessage();

    const expectations: Array<[UserAction, number]> = [
      [UserAction.COPY, 2],
      [UserAction.COPY_FIRST, 1],
      [UserAction.COPY_SEARCHIFIED, 2],
      [UserAction.COPY_SEARCHIFIED_FIRST, 1],
      [UserAction.FIND_IN_PAGE, 2],
      [UserAction.FIND_IN_PAGE_FIRST, 1],
      [UserAction.FIND_IN_PAGE_SEARCHIFIED, 2],
      [UserAction.FIND_IN_PAGE_SEARCHIFIED_FIRST, 1],
    ];
    chrome.test.assertEq(
        expectations.length, mockMetricsPrivate.actionCounter.size);

    expectations.forEach(([action, value]) => {
      chrome.test.assertEq(mockMetricsPrivate.actionCounter.get(action), value);
    });

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
