// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxElement, ComposeboxProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ContextUploadStatus, InputType} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

export {MockInputState};

export const FAKE_TOKEN_STRING = '00000000000000001234567890ABCDEF';
export const FAKE_TOKEN_STRING_2 = '00000000000000001234567890ABCDEE';
export const ADD_FILE_CONTEXT_FN = 'addFileContext';
export const ADD_TAB_CONTEXT_FN = 'addTabContext';

export function generateZeroId(): string {
  // Generate 128 bit unique identifier.
  const components = new Uint32Array(4);
  return components.reduce(
      (id = '', component) => id + component.toString(16).padStart(8, '0'), '');
}

export function createComposeboxFile(
    index: number, override: Partial<ComposeboxFile> = {}): ComposeboxFile {
  return Object.assign(
      {
        name: `file${index}`,
        type: 'application/pdf',
        inputType: InputType.kLensFile,
        objectUrl: null,
        dataUrl: null,
        uuid: `${index}`,
        status: ContextUploadStatus.kUploadSuccessful,
        url: null,
        file: null,
        tabId: null,
        isDeletable: true,
        iconName: null,
        supportsUnimodal: true,
        thumbnailUrl: null,
      },
      override);
}

export interface ComposeboxTestElement {
  element: ComposeboxElement;
  handler: TestMock<PageHandlerRemote>;
  searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  searchboxCallbackRouterRemote: SearchboxPageRemote;
  metrics: MetricsTracker;
}


export function setupComposeboxTest(): ComposeboxTestElement {
  // We can't return the variables initialized in setup() directly because
  // setup() runs later. We can return an object that will be populated.
  const testProxy = {} as ComposeboxTestElement;

  setup(() => {
    loadTimeData.overrideValues({
      'composeboxImageFileTypes':
          'image/avif,image/bmp,image/jpeg,image/png,image/webp,image/heif,image/heic',
      'composeboxAttachmentFileTypes': '.pdf,application/pdf',
      'contextualMenuUsePecApi': false,
      'searchboxComposePlaceholder': 'Placeholder',
      'lensSendRawFileMediaTypesEnabled': false,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    const searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setPromiseResolveFor('getRecentTabs', {tabs: []});
    searchboxHandler.setPromiseResolveFor('getInputState', {
      state: new MockInputState({
        toolConfigs: [],
        toolsSectionConfig: {header: ''},
        modelSectionConfig: {header: ''},
      }),
    });
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NTP_COMPOSEBOX'}));
    const searchboxCallbackRouterRemote =
        ComposeboxProxyImpl.getInstance()
            .searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    const metrics = fakeMetricsPrivate();

    testProxy.handler = handler;
    testProxy.searchboxHandler = searchboxHandler;
    testProxy.searchboxCallbackRouterRemote = searchboxCallbackRouterRemote;
    testProxy.metrics = metrics;
  });

  return testProxy;
}

export function createComposeboxElement(
    testProxy: ComposeboxTestElement,
    properties: Partial<ComposeboxElement> = {}) {
  testProxy.element = new ComposeboxElement();
  Object.assign(testProxy.element, {
    usePecApi: loadTimeData.getBoolean('contextualMenuUsePecApi'),
    ...properties,
  });
  document.body.appendChild(testProxy.element);
}

export async function waitForAddFileCallCount(
    searchboxHandler: TestMock<SearchboxPageHandlerRemote>,
    expectedCount: number): Promise<void> {
  const startTime = Date.now();
  return new Promise((resolve, reject) => {
    const checkCount = () => {
      const currentCount = searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN);
      if (currentCount === expectedCount) {
        resolve();
        return;
      }

      if (Date.now() - startTime >= 5000) {
        reject(new Error(`Could not add file ${expectedCount} times.`));
        return;
      }

      setTimeout(checkCount, 50);
    };
    checkCount();
  });
}

export function getInputForFileType(
    composeboxElement: ComposeboxElement, fileType: string): HTMLInputElement {
  return fileType === 'application/pdf' ?
      composeboxElement.$.fileInputs.$.fileInput :
      composeboxElement.$.fileInputs.$.imageInput;
}

export function getMockFileChangeEventForType(
    composeboxElement: ComposeboxElement, fileType: string): Event {
  if (fileType === 'application/pdf') {
    return new Event('change');
  }

  const mockFileChange = new Event('change', {bubbles: true});
  Object.defineProperty(mockFileChange, 'target', {
    writable: false,
    value: composeboxElement.$.fileInputs.$.imageInput,
  });
  return mockFileChange;
}

export async function areMatchesShowing(
    composeboxElement: ComposeboxElement,
    searchboxCallbackRouterRemote: SearchboxPageRemote): Promise<boolean> {
  // Force a synchronous render.
  await searchboxCallbackRouterRemote.$.flushForTesting();
  await microtasksFinished();
  return window.getComputedStyle(composeboxElement.$.matches).display !==
      'none';
}

export async function uploadFileAndVerify(
    testProxy: ComposeboxTestElement, token: Object, file: File) {
  // Assert no files.
  assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

  testProxy.searchboxHandler.setPromiseResolveFor(ADD_FILE_CONTEXT_FN, token);

  // Act.
  const dataTransfer = new DataTransfer();
  dataTransfer.items.add(file);

  const input: HTMLInputElement =
      getInputForFileType(testProxy.element, file.type);
  input.files = dataTransfer.files;
  input.dispatchEvent(
      getMockFileChangeEventForType(testProxy.element, file.type));

  await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
  await microtasksFinished();

  assertEquals(
      testProxy.searchboxHandler.getCallCount('notifySessionStarted'), 1);
  await verifyFileUpload(testProxy, file);
}

export async function verifyFileUpload(
    testProxy: ComposeboxTestElement, file: File) {
  // Assert one file.
  const files = testProxy.element.$.carousel.files;
  assertEquals(files.length, 1);

  assertEquals(files[0]!.type, file.type);
  assertEquals(files[0]!.name, file.name);

  // Assert file is uploaded.
  assertEquals(testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN), 1);

  const fileBuffer = await file.arrayBuffer();
  const fileArray = Array.from(new Uint8Array(fileBuffer));

  const [[fileInfo, fileData]] =
      testProxy.searchboxHandler.getArgs(ADD_FILE_CONTEXT_FN);
  assertEquals(fileInfo.fileName, file.name);
  assertDeepEquals(fileData.bytes, fileArray);
}

export async function addTab(testProxy: ComposeboxTestElement): Promise<string> {
  testProxy.searchboxHandler.setPromiseResolveFor(
      ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING);

  // Assert no files.
  assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

  const contextMenuButton = $$(testProxy.element, '#contextEntrypoint');
  assertTrue(!!contextMenuButton);
  const sampleTabTitle = 'Sample Tab';
  contextMenuButton.dispatchEvent(new CustomEvent('add-tab-context', {
    detail: {id: 1, title: sampleTabTitle},
    bubbles: true,
    composed: true,
  }));

  await testProxy.searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
  await microtasksFinished();
  const files = testProxy.element.$.carousel.files;
  assertEquals(files.length, 1);
  assertEquals(files[0]!.type, 'tab');
  assertEquals(files[0]!.name, sampleTabTitle);
  return FAKE_TOKEN_STRING;
}

export function getSubmitContainer(testProxy: ComposeboxTestElement):
    HTMLElement {
  const button = testProxy.element.shadowRoot.querySelector(
      'cr-composebox-submit');
  assertTrue(!!button);
  const container = button.shadowRoot.querySelector<HTMLElement>('#submitContainer');
  assertTrue(!!container);
  return container;
}

export function getSubmitIcon(testProxy: ComposeboxTestElement): HTMLElement {
  const button = testProxy.element.shadowRoot.querySelector(
      'cr-composebox-submit');
  assertTrue(!!button);
  const submitIcon = button.shadowRoot.querySelector<HTMLElement>('#submitIcon');
  assertTrue(!!submitIcon);
  return submitIcon;
}
