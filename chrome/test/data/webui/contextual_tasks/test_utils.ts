// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {createAutocompleteMatch, createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {type PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {type MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// Base64 encoding of a UI handshake request message [1, 2, 3].
// Generated from btoa(String.fromCharCode(...[1, 2, 3]))
export const HANDSHAKE_REQUEST_MESSAGE_BASE64 = 'AQID';

export const ADD_FILE_CONTEXT_FN = 'addFileContext';
export const ADD_TAB_CONTEXT_FN = 'addTabContext';

// Byte array of a typical handshake response from the webview.
// Equivalent to base64 decoding 'CgIIAA=='
export const HANDSHAKE_RESPONSE_BYTES = new Uint8Array([10, 2, 8, 0]);

export const FAKE_TOKEN_STRING = '00000000000000001234567890ABCDEF';
export const FAKE_TOKEN_STRING_2 = '00000000000000001234567890ABCDFF';

export const fixtureUrl = 'chrome://webui-test/contextual_tasks/test.html';

export function assertHTMLElement(element: Element|null|undefined):
    asserts element is HTMLElement {
  assertTrue(!!element);
  assertTrue(element instanceof HTMLElement);
}

export function assertStyle(
    element: Element|null, name: string, expected: string, error: string = '') {
  assertTrue(!!element, `Element is null`);
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual, error);
}

type Constructor<T> = new (...args: any[]) => T;
type Installer<T> = (instance: T) => void;

export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);
  installer(mock);
  return mock;
}


export function simulateUserInput(
    inputElement: HTMLInputElement|HTMLTextAreaElement, value: string) {
  inputElement.value = value;
  inputElement.dispatchEvent(
      new Event('input', {bubbles: true, composed: true}));
}

export async function setupAutocompleteResults(
    searchboxCallbackRouterRemote: SearchboxPageRemote, testQuery: string,
    mockTimer: MockTimer) {
  const matches = [
    createAutocompleteMatch({
      allowedToBeDefaultMatch: true,
      contents: testQuery,
      destinationUrl: `${fixtureUrl}/search?q=${testQuery}`,
      type: 'search-what-you-typed',
      fillIntoEdit: testQuery,
    }),
    createAutocompleteMatch(),
  ];
  searchboxCallbackRouterRemote.autocompleteResultChanged(
      createAutocompleteResultForTesting({
        input: testQuery,
        matches: matches,
      }));
  await searchboxCallbackRouterRemote.$.flushForTesting();
  mockTimer.tick(0);
}

export async function uploadFileAndVerify(
    token: Object, file: File, composebox: any,
    mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>,
    expectedInitialFilesCount: number = 0) {
  // Assert initial file count if 0 -> carousel should not render.
  if (expectedInitialFilesCount === 0) {
    assertFalse(
        !!composebox.shadowRoot.querySelector('#carousel'),
        'Files should be empty and carousel should not render.');
  }

  mockSearchboxPageHandler.resetResolver(ADD_FILE_CONTEXT_FN);
  mockSearchboxPageHandler.setResultFor(
      ADD_FILE_CONTEXT_FN, Promise.resolve(token));
  const dataTransfer = new DataTransfer();
  dataTransfer.items.add(file);

  composebox.$.fileInputs.dispatchEvent(new CustomEvent('file-change', {
    detail: {files: dataTransfer.files},
    bubbles: true,
    composed: true,
  }));

  // Must call to upload. Await -> wait for it to be called once.
  await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);

  // Must await for file carousel to re-render since are adding files.
  await composebox.updateComplete;
  await microtasksFinished();
  await verifyFileCarouselMatchesUploaded(
      file, composebox, mockSearchboxPageHandler, expectedInitialFilesCount);
}


export async function verifyFileCarouselMatchesUploaded(
    file: File, composebox: any,
    mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>,
    expectedInitialFilesCount: number) {
  // Assert one file.

  // Avoid using $.carousel since may be cached.
  const carousel = composebox.shadowRoot.querySelector('#carousel');

  assertTrue(!!carousel, 'Carousel should be in the DOM');
  const files = carousel.files;

  assertEquals(
      expectedInitialFilesCount + 1,
      files.length,
      `Number of carousel files should be ${expectedInitialFilesCount + 1}`,
  );
  const currentFile = files[files.length - 1];

  assertEquals(currentFile!.type, file.type);
  assertEquals(currentFile!.name, file.name);

  // Assert file is uploaded.
  assertEquals(
      1, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN),
      `Add file context should be called for this file once.`);
  const fileBuffer = await file.arrayBuffer();
  const fileArray = Array.from(new Uint8Array(fileBuffer));

  // Verify identity of latest file with that of uploaded version.
  const allCalls = mockSearchboxPageHandler.getArgs(ADD_FILE_CONTEXT_FN);
  const [fileInfo, fileData] = allCalls[allCalls.length - 1];
  assertEquals(fileInfo.fileName, file.name);
  assertDeepEquals(fileData.bytes, fileArray);
}

export async function deleteLastFile(composebox: any) {
  const files = composebox.$.carousel.files;
  const deletedId = files[files.length - 1]!.uuid;
  composebox.$.carousel.dispatchEvent(new CustomEvent('delete-file', {
    detail: {
      uuid: deletedId,
    },
    bubbles: true,
    composed: true,
  }));
  await microtasksFinished();
}

export function getSubmitContainer(composebox: any): HTMLElement|null {
  return composebox.shadowRoot.querySelector('#submitContainer');
}

export function getSubmitButton(composebox: any): HTMLButtonElement|null {
  const submitContainer: HTMLElement|null = getSubmitContainer(composebox);

  if (!submitContainer) {
    return null;
  }

  const submitButton: HTMLButtonElement|null =
      submitContainer.querySelector('#submitIcon');
  return submitButton;
}
