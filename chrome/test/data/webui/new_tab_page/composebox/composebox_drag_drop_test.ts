// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxWindowProxy} from 'chrome://new-tab-page/lazy_load.js';
import type {ComposeboxFile, ComposeboxFileCarouselElement, NtpComposeboxElement} from 'chrome://new-tab-page/lazy_load.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {InputType, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

import {ADD_FILE_CONTEXT_FN, createComposeboxElement, MockInputState, setupComposeboxTest} from './test_support.js';

// Creates drag event that is compatible across all OS's + w/bots
function createDragEvent(type: string, files: File[]): DragEvent {
  const event = new DragEvent(type, {
    bubbles: true,
    cancelable: true,
    composed: true,
  });

  const mockDataTransfer = {
    files: files,
    types: ['Files', 'text/plain'],
    items: files.map(f => ({
                       kind: 'file',
                       type: f.type,
                       getAsFile: () => f,
                     })),
    effectAllowed: 'all',
    dropEffect: 'copy',
    getData: () => '',
  };

  Object.defineProperty(event, 'dataTransfer', {
    value: mockDataTransfer,
  });
  return event;
}

async function dispatchDragAndDropEvent(
    hostElement: HTMLElement, files: File[]) {
  const dropZone = hostElement.shadowRoot!.querySelector('#composebox');

  if (!dropZone) {
    throw new Error(
        'dispatchDragAndDropEvent: #composebox drop zone not rendered.');
  }

  const enterEvent = createDragEvent('dragenter', files);
  dropZone.dispatchEvent(enterEvent);
  await microtasksFinished();

  const overEvent = createDragEvent('dragover', files);
  dropZone.dispatchEvent(overEvent);
  await microtasksFinished();

  const dropEvent = createDragEvent('drop', files);
  dropZone.dispatchEvent(dropEvent);
  await microtasksFinished();
}

// --- Integration Tests for the Fork Element ---
suite('ComposeboxDragAndDrop', () => {
  const testProxy = setupComposeboxTest<NtpComposeboxElement>();
  let windowProxy: TestMock<ComposeboxWindowProxy>;

  setup(async () => {
    loadTimeData.overrideValues({
      useNtpComposeboxFork: true,  // <-- Locked to NTP fork element
      'composeboxContextDragAndDropEnabled': true,
      'composeboxFileMaxCount': 4,
      'composeboxFileMaxSize': 10000000,
      'lensSendRawFileMediaTypesEnabled': false,
    });

    // Additional Mojo overrides required specifically for drag-drop tests
    testProxy.handler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
    testProxy.searchboxHandler.setResultFor('getInputState', Promise.resolve({
      state: new MockInputState({
        toolConfigs: [],
        toolsSectionConfig: {header: ''},
        modelSectionConfig: {header: ''},
        maxInputsByType: {
          [InputType.kBrowserTab]: 1,
          [InputType.kLensImage]: 1,
          [InputType.kLensFile]: 1,
        },
        maxTotalInputs: 3,
      }),
    }));

    // Mock Window timers and media selectors
    windowProxy = installMock(ComposeboxWindowProxy);
    windowProxy.setResultFor('setTimeout', 0);
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   addEventListener() {},
                                                   removeListener() {},
                                                   removeEventListener() {},
                                                 }));

    createComposeboxElement(testProxy);
    await microtasksFinished();
  });

  teardown(() => {
    loadTimeData.overrideValues({
      'composeboxFileMaxCount': 4,
      'composeboxFileMaxSize': 1000000,
    });
    testProxy.searchboxHandler.reset();
    testProxy.handler.reset();
  });

  test('sets is-dragging-file attribute on dragenter', async () => {
    const dropZone = testProxy.element.shadowRoot.querySelector('#composebox');
    assertTrue(!!dropZone);

    assertFalse(testProxy.element.hasAttribute('is-dragging-file'));

    dropZone?.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertTrue(testProxy.element.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.DRAGGING, testProxy.element.animationState);
  });

  test('removes is-dragging-file attribute on dragleave', async () => {
    const dropZone = testProxy.element.shadowRoot.querySelector('#composebox');
    assertTrue(!!dropZone);

    testProxy.element.animationState = GlowAnimationState.DRAGGING;
    dropZone?.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    dropZone?.dispatchEvent(new DragEvent('dragleave', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertFalse(testProxy.element.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.NONE, testProxy.element.animationState);
  });

  test('accepts a dropped file and adds it to the carousel', async () => {
    const sharedToken = {high: 1n, low: 2n} as unknown as UnguessableToken;
    testProxy.searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve(sharedToken));

    const file = new File(['content'], 'foo.pdf', {type: 'application/pdf'});
    await dispatchDragAndDropEvent(testProxy.element, [file]);

    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
    assertFalse(testProxy.element.hasAttribute('is-dragging-file'));

    const mockAddedFile: ComposeboxFile = {
      uuid: sharedToken,
      name: 'foo.pdf',
      status: 0,
      type: 'application/pdf',
      inputType: InputType.kLensFile,
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
      iconName: null,
      supportsUnimodal: true,
    };

    // Uses public mixin method directly to allow parameterization
    testProxy.element.onFileContextAdded(mockAddedFile);
    await microtasksFinished();
    await testProxy.element.updateComplete;
    await microtasksFinished();

    const carousel: ComposeboxFileCarouselElement|null =
        testProxy.element.shadowRoot.querySelector(
            'cr-composebox-file-carousel');

    assertTrue(!!carousel, 'Carousel should render');

    const carouselFiles = carousel.files;
    assertEquals(1, carouselFiles.length);
    assertEquals('foo.pdf', carouselFiles[0]!.name);
  });

  test('does not accept a dropped file that is too large', async () => {
    const sampleFileMaxSize = 10;  // bytes
    loadTimeData.overrideValues({'composeboxFileMaxSize': sampleFileMaxSize});

    // Re-create element with updated size limit
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const blob = new Blob([new Uint8Array(sampleFileMaxSize + 1)]);
    const testFile =
        new File([blob], 'largefile.pdf', {type: 'application/pdf'});
    await dispatchDragAndDropEvent(testProxy.element, [testFile]);

    assertEquals(
        0, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('does not accept wrong file type', async () => {
    const testFile =
        new File(['foo'], 'malware.exe', {type: 'application/x-msdownload'});
    await dispatchDragAndDropEvent(testProxy.element, [testFile]);

    const expectedCallCount =
        loadTimeData.getBoolean('lensSendRawFileMediaTypesEnabled') ? 1 : 0;
    assertEquals(
        expectedCallCount,
        testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('does not accept multiple files if only one allowed', async () => {
    loadTimeData.overrideValues({'composeboxFileMaxCount': 1});

    // Re-create element with updated count limit
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const sharedToken = {high: 1n, low: 2n} as unknown as UnguessableToken;
    testProxy.searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve(sharedToken));

    const file1 = new File(['a'], 'a.pdf', {type: 'application/pdf'});
    const file2 = new File(['b'], 'b.pdf', {type: 'application/pdf'});

    await dispatchDragAndDropEvent(testProxy.element, [file1, file2]);

    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);

    assertEquals(
        1, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    const mockAddedFile: ComposeboxFile = {
      uuid: sharedToken,
      name: 'a.pdf',
      status: 0,
      type: 'application/pdf',
      inputType: InputType.kLensFile,
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
      iconName: null,
      supportsUnimodal: true,
    };
    testProxy.element.onFileContextAdded(mockAddedFile);
    await microtasksFinished();
    await testProxy.element.updateComplete;
    await microtasksFinished();

    const carousel: ComposeboxFileCarouselElement|null =
        testProxy.element.shadowRoot.querySelector(
            'cr-composebox-file-carousel');

    assertTrue(!!carousel, 'Carousel should render');

    const carouselFiles = carousel.files;
    assertEquals(1, carouselFiles.length);
    assertEquals('a.pdf', carouselFiles[0]?.name);
  });

  test('Deep Search mode blocks all uploads', async () => {
    const contextEntrypoint =
        testProxy.element.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kDeepSearch},
    }));

    // Propagate state change through mocked Mojo router callback
    testProxy.searchboxCallbackRouterRemote.onInputStateChanged({
      ...new MockInputState(),
      activeTool: ToolMode.kDeepSearch,
    });
    await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    const imageFile = new File(
        ['content'], 'test.png', {type: 'image/png'});  // size = 7 bytes
    await dispatchDragAndDropEvent(testProxy.element, [imageFile]);
    assertEquals(
        0, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('Image Gen mode allows images but blocks PDFs', async () => {
    loadTimeData.overrideValues({
      'composeboxImageFileTypes': 'image/*',
      'composeboxAttachmentFileTypes': 'application/pdf',
    });
    testProxy.searchboxHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [InputType.kLensFile],
        inputTypeConfigs: [],
        toolConfigs: [],
        modelConfigs: [],
        toolsSectionConfig: null,
        modelSectionConfig: null,
        hintText: '',
        maxInputsByType: {[InputType.kLensImage]: 1, [InputType.kLensFile]: 1},
        maxTotalInputs: 2,
      },
    }));

    // Re-create element with updated constraints
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const contextEntrypoint =
        testProxy.element.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kImageGen},
    }));

    // Propagate state change through mocked Mojo router callback
    testProxy.searchboxCallbackRouterRemote.onInputStateChanged({
      ...new MockInputState(),
      activeTool: ToolMode.kImageGen,
      disabledInputTypes: [InputType.kLensFile],
    });
    await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // 1. Drop a PDF (should be blocked).
    const pdfFile = new File(
        ['content'], 'test.pdf', {type: 'application/pdf'});  // size = 7 bytes
    await dispatchDragAndDropEvent(testProxy.element, [pdfFile]);
    assertEquals(
        0, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    // 2. Drop an image (should be allowed).
    const imageToken = {high: 3n, low: 4n} as unknown as UnguessableToken;
    testProxy.searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve(imageToken));
    const imageFile = new File(
        ['content'], 'test.png', {type: 'image/png'});  // size = 7 bytes
    await dispatchDragAndDropEvent(testProxy.element, [imageFile]);
    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('Canvas mode allows both images and PDFs', async () => {
    loadTimeData.overrideValues({
      'composeboxImageFileTypes': 'image/*',
      'composeboxAttachmentFileTypes': 'application/pdf',
    });
    testProxy.searchboxHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        inputTypeConfigs: [],
        toolConfigs: [],
        modelConfigs: [],
        toolsSectionConfig: null,
        modelSectionConfig: null,
        hintText: '',
        maxInputsByType: {[InputType.kLensImage]: 1, [InputType.kLensFile]: 1},
        maxTotalInputs: 2,
      },
    }));

    // Re-create element with updated constraints
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const contextEntrypoint =
        testProxy.element.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kCanvas},
    }));

    // Propagate state change through mocked Mojo router callback
    testProxy.searchboxCallbackRouterRemote.onInputStateChanged({
      ...new MockInputState(),
      activeTool: ToolMode.kCanvas,
    });
    await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // 1. Drop an image.
    const imageToken = {high: 3n, low: 4n} as unknown as UnguessableToken;
    testProxy.searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve(imageToken));
    const imageFile = new File(
        ['content'], 'test.png', {type: 'image/png'});  // size = 7 bytes
    await dispatchDragAndDropEvent(testProxy.element, [imageFile]);
    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    // 2. Drop a PDF.
    testProxy.searchboxHandler.reset();
    const pdfToken = {high: 5n, low: 6n} as unknown as UnguessableToken;
    testProxy.searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve(pdfToken));
    const pdfFile = new File(
        ['content'], 'test.pdf', {type: 'application/pdf'});  // size = 7 bytes
    await dispatchDragAndDropEvent(testProxy.element, [pdfFile]);
    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });
});
