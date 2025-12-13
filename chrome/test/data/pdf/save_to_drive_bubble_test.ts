// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {SaveToDriveBubbleRequestType, SaveToDriveState} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {ViewerSaveToDriveBubbleElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

function createBubbleElement(): ViewerSaveToDriveBubbleElement {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const element = document.createElement('viewer-save-to-drive-bubble');
  document.body.appendChild(element);
  return element;
}

interface ExpectedElements {
  cancel?: boolean;
  link?: boolean;
  openInDrive?: boolean;
  progress?: boolean;
  retry?: boolean;
  manageStorage?: boolean;
}

async function testBubbleState(
    state: SaveToDriveState, element: ViewerSaveToDriveBubbleElement,
    {cancel, link, openInDrive, progress, retry, manageStorage}:
        ExpectedElements) {
  element.state = state;
  await microtasksFinished();

  const shadowRoot = element.shadowRoot;
  chrome.test.assertEq(
      !!shadowRoot.querySelector('#cancel-upload-button'), !!cancel);
  chrome.test.assertEq(!!shadowRoot.querySelector('a'), !!link);
  chrome.test.assertEq(
      !!shadowRoot.querySelector('#open-in-drive-button'), !!openInDrive);
  chrome.test.assertEq(!!shadowRoot.querySelector('cr-progress'), !!progress);
  chrome.test.assertEq(!!shadowRoot.querySelector('#retry-button'), !!retry);
  chrome.test.assertEq(
      !!shadowRoot.querySelector('#manage-storage-button'), !!manageStorage);
}

async function testEventDispatchedFromButtonClick(
    element: ViewerSaveToDriveBubbleElement, buttonSelector: string,
    expectedEvent: SaveToDriveBubbleRequestType) {
  const button =
      element.shadowRoot.querySelector<HTMLButtonElement>(buttonSelector)!;
  const eventPromise = eventToPromise('save-to-drive-bubble-action', element);
  button.click();
  const e: CustomEvent<SaveToDriveBubbleRequestType> = await eventPromise;
  chrome.test.assertEq(expectedEvent, e.detail);
  chrome.test.assertFalse(element.$.dialog.open);
}

const tests = [
  async function testShowAndHide() {
    const element = createBubbleElement();
    const anchor = document.createElement('div');
    document.body.appendChild(anchor);

    element.showAt(anchor);
    await microtasksFinished();
    chrome.test.assertTrue(element.$.dialog.open);

    await testEventDispatchedFromButtonClick(
        element, '#close', SaveToDriveBubbleRequestType.DIALOG_CLOSED);
    chrome.test.assertFalse(element.$.dialog.open);

    chrome.test.succeed();
  },

  async function testHideWhenFocusoutOfDialog() {
    const element = createBubbleElement();
    const anchor = document.createElement('div');
    anchor.style.position = 'fixed';
    anchor.style.top = `400px`;
    anchor.style.left = `400px`;
    document.body.appendChild(anchor);
    element.showAt(anchor);

    // Focusout from elements within dialog should do nothing.
    const header = element.shadowRoot.querySelector('#header')!;
    header.dispatchEvent(new CustomEvent('focusout', {
      composed: true,
      bubbles: true,
    }));
    await microtasksFinished();
    chrome.test.assertTrue(element.$.dialog.open);

    // Focusout from the dialog itself should close the dialog.
    element.$.dialog.dispatchEvent(
        new CustomEvent('focusout', {composed: true, bubbles: true}));
    await microtasksFinished();
    chrome.test.assertFalse(element.$.dialog.open);

    chrome.test.succeed();
  },

  async function testPositionsCorrectly() {
    const windowHeight = 1000;
    const dialogWidth = 100;
    const dialogHeight = 200;
    const anchorWidth = 50;
    const anchorHeight = 25;
    const anchorTop = 300;
    const anchorLeft = 400;

    const element = createBubbleElement();
    const anchor = document.createElement('div');
    anchor.style.position = 'fixed';
    anchor.style.top = `${anchorTop}px`;
    anchor.style.height = `${anchorHeight}px`;
    anchor.style.left = `${anchorLeft}px`;
    anchor.style.width = `${anchorWidth}px`;
    element.$.dialog.style.width = `${dialogWidth}px`;
    element.$.dialog.style.height = `${dialogHeight}px`;
    window.innerHeight = windowHeight;
    document.body.appendChild(anchor);
    element.showAt(anchor);
    await microtasksFinished();

    chrome.test.assertEq(
        `${anchorTop + anchorHeight}px`, element.$.dialog.style.top);
    chrome.test.assertEq(
        `${anchorLeft + anchorWidth - dialogWidth}px`,
        element.$.dialog.style.left);

    // Test that the top position changes if anchor is near bottom of window.
    const newAnchorTop = windowHeight;
    anchor.style.top = `${newAnchorTop}px`;
    element.showAt(anchor);
    await microtasksFinished();
    chrome.test.assertEq(
        `${newAnchorTop - dialogHeight}px`, element.$.dialog.style.top);

    // Test that the left position changes if window resizes.
    const newWindowWidth = 1000;
    anchor.style.left = `${newWindowWidth}px`;
    await microtasksFinished();
    // Dialog left should still be at the old position.
    chrome.test.assertEq(
        `${anchorLeft + anchorWidth - dialogWidth}px`,
        element.$.dialog.style.left);
    // Dialog left should be changed if window resizes.
    window.dispatchEvent(new Event('resize'));
    await microtasksFinished();
    chrome.test.assertEq(
        `${newWindowWidth + anchorWidth - dialogWidth}px`,
        element.$.dialog.style.left);

    // Test the position in RTL.
    anchor.style.left = `${anchorLeft}px`;
    document.documentElement.dir = 'rtl';
    element.showAt(anchor);
    await microtasksFinished();
    const anchorLeftRtl = anchor.getBoundingClientRect().left;
    chrome.test.assertEq(
        `${window.innerWidth - anchorLeftRtl - dialogWidth}px`,
        element.$.dialog.style.right);

    chrome.test.succeed();
  },

  async function testUploadingState() {
    const element = createBubbleElement();
    element.progress.uploadedBytes = 100;
    element.progress.fileSizeBytes = 200;
    await testBubbleState(SaveToDriveState.UPLOADING, element, {
      progress: true,
      cancel: true,
    });
    const progressBar = element.shadowRoot.querySelector('cr-progress');
    chrome.test.assertTrue(!!progressBar);
    chrome.test.assertEq(100, progressBar.value);
    chrome.test.assertEq(200, progressBar.max);
    await testEventDispatchedFromButtonClick(
        element, '#cancel-upload-button',
        SaveToDriveBubbleRequestType.CANCEL_UPLOAD);

    chrome.test.succeed();
  },

  async function testSuccessState() {
    const element = createBubbleElement();
    await testBubbleState(SaveToDriveState.SUCCESS, element, {
      openInDrive: true,
    });
    await testEventDispatchedFromButtonClick(
        element, '#open-in-drive-button',
        SaveToDriveBubbleRequestType.OPEN_IN_DRIVE);

    chrome.test.succeed();
  },

  async function testConnectionErrorState() {
    const element = createBubbleElement();
    await testBubbleState(SaveToDriveState.CONNECTION_ERROR, element, {
      retry: true,
    });
    await testEventDispatchedFromButtonClick(
        element, '#retry-button', SaveToDriveBubbleRequestType.RETRY);

    chrome.test.succeed();
  },

  async function testStorageFullState() {
    const element = createBubbleElement();
    await testBubbleState(SaveToDriveState.STORAGE_FULL_ERROR, element, {
      manageStorage: true,
    });
    await testEventDispatchedFromButtonClick(
        element, '#manage-storage-button',
        SaveToDriveBubbleRequestType.MANAGE_STORAGE);

    chrome.test.succeed();
  },

  async function testSessionTimeoutState() {
    const element = createBubbleElement();
    await testBubbleState(SaveToDriveState.SESSION_TIMEOUT_ERROR, element, {
      retry: true,
    });
    await testEventDispatchedFromButtonClick(
        element, '#retry-button', SaveToDriveBubbleRequestType.RETRY);

    chrome.test.succeed();
  },

  async function testUnknownErrorState() {
    const element = createBubbleElement();
    await testBubbleState(SaveToDriveState.UNKNOWN_ERROR, element, {
      link: true,
    });

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
