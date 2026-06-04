// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {LensOverlayDismissalSource, PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextUploadStatus, InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {assertStyle, deleteLastFile, FAKE_TOKEN_STRING, FAKE_TOKEN_STRING_2, fixtureUrl, getSubmitButton, getSubmitContainer, uploadFileAndVerify} from './test_utils.js';

suite('ContextualTasksComposeboxFilesTest', () => {
  let contextualTasksApp: ContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
      ComposeboxPageHandlerRemote;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
      SearchboxPageHandlerRemote;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let mockTimer: MockTimer;

  class MockResizeObserver {
    static instances: MockResizeObserver[] = [];

    constructor(private callback: ResizeObserverCallback) {
      MockResizeObserver.instances.push(this);
    }

    observe(_target: Element) {}
    unobserve(_target: Element) {}
    disconnect() {}

    trigger() {
      // Trigger with empty entries as the component doesn't use entries
      this.callback([], this);
    }
  }

  setup(async () => {
    if (!window.chrome) {
      Object.assign(window, {chrome: {}});
    }

    if (!window.chrome.histograms) {
      Object.assign(window.chrome, {
        histograms: {
          recordEnumerationValue: () => {},
          recordUserAction: () => {},
          recordBoolean: () => {},
        },
      });
    }
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Mock ResizeObserver
    window.ResizeObserver = MockResizeObserver;
    MockResizeObserver.instances = [];

    mockTimer = new MockTimer();

    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
      composeboxSmartTabSharingVisible: false,
      enableComposeboxJumpFix: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
      composeboxHintTextLensOverlay: 'Test Lens Hint',
      composeboxHintTextAskAboutThese: 'Ask about these',
      composeboxHintTextAskAboutThisTab: 'Ask about this tab',
      composeboxHintTextAskAboutThisImage: 'Ask about this image',
      composeboxHintTextAskAboutThisDoc: 'Ask about this doc',
      forcedEmbeddedPageHost: '',
    });

    testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockComposeboxPageHandler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));
    mockSearchboxPageHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    mockSearchboxPageHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        toolConfigs: [{
          tool: ToolMode.kCanvas,
          disableActiveModelSelection: false,
          menuLabel: 'Canvas',
          chipLabel: 'Canvas',
          hintText: 'Canvas hint',
          aimUrlParams: [{paramKey: 'rc', paramValue: '1'}],
        }],
      },
    }));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);
    await microtasksFinished();
    composebox = contextualTasksApp.$.composebox.$.composebox;

    assertTrue(
        MockResizeObserver.instances.length >= 1,
        'There should be at least one ResizeObserver instance.');

    searchboxCallbackRouterRemote.onInputStateChanged(new MockInputState());
    await microtasksFinished();
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  test('closes Lens overlay when image uploads are disabled', async () => {
    const disabledState = {
      ...new MockInputState(),
      disabledInputTypes: [InputType.kLensImage],
    };

    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;
    innerComposebox.dispatchEvent(new CustomEvent('input-state-changed', {
      detail: {inputState: disabledState},
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();

    assertEquals(
        1, mockComposeboxPageHandler.getCallCount('closeLensOverlayFromWebUI'));
    assertEquals(
        LensOverlayDismissalSource.kContextualTasksImageUploadsDisabled,
        mockComposeboxPageHandler.getArgs('closeLensOverlayFromWebUI')[0]);
  });

  test('lens button is disabled when image uploads are disabled', async () => {
    const disabledState = {
      ...new MockInputState(),
      disabledInputTypes: [InputType.kLensImage],
    };

    searchboxCallbackRouterRemote.onInputStateChanged(disabledState);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await contextualTasksApp.$.composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();

    assertTrue(composebox.lensButtonDisabled);
  });

  test(
      'Upload status is tracked properly when adding and removing files',
      async () => {
        assertEquals(0, composebox.pendingUploads.size);
        const testFile1 = new File(['test'], 'test1.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(
            FAKE_TOKEN_STRING, testFile1, composebox, mockSearchboxPageHandler);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kNotUploaded,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            0, composebox.pendingUploads.size,
            'First file should be uploading.');
        assertTrue(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (first file)');

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kProcessing,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            1, composebox.pendingUploads.size,
            'First file should be uploading.');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (first file)');
        const testFile2 =
            new File(['test2'], 'test2.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(
            FAKE_TOKEN_STRING_2, testFile2, composebox,
            mockSearchboxPageHandler, 1);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING_2,
            ContextUploadStatus.kProcessingSuggestSignalsReady,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            2, composebox.pendingUploads.size,
            'Second file should be uploading');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (second file)');

        await deleteLastFile(composebox);
        assertEquals(
            1, composebox.pendingUploads.size,
            'File should be deleted and number of files left are 1');

        await deleteLastFile(composebox);
        assertEquals(
            0, composebox.pendingUploads.size,
            'File should be deleted and number of files left are 0');
      });

  test('clear all (cancel) works for uploading set', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}), composebox,
        mockSearchboxPageHandler);

    await composebox.updateComplete;
    await microtasksFinished();

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kUploadSuccessful, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;

    composebox.clearAllInputs(false);

    await Promise.all([
      composebox.updateComplete,
      microtasksFinished(),
    ]);

    assertEquals(0, composebox.files.size);

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(submitButton !== null, 'Submit button should exist');
    assertTrue(submitButton.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
    assertTrue(
        submitContainer !== null, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
            even when disabled.');

    assertEquals(0, composebox.pendingUploads.size);
  });

  test(
      'clear all (cancel) works for uploading set with undeletable files',
      async () => {
        const token1 = FAKE_TOKEN_STRING;
        await uploadFileAndVerify(
            token1, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
            composebox, mockSearchboxPageHandler);

        const currentFiles = composebox.files;
        currentFiles.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.requestUpdate();

        await composebox.updateComplete;

        // Now file 1 is not deletable while file 2 is.
        const token2 = FAKE_TOKEN_STRING_2;
        await uploadFileAndVerify(
            token2, new File(['foo2'], 'foo2.jpg', {type: 'image/png'}),
            composebox, mockSearchboxPageHandler, 1);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token1, ContextUploadStatus.kUploadSuccessful, null);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token2, ContextUploadStatus.kUploadSuccessful, null);
        await searchboxCallbackRouterRemote.$.flushForTesting();

        await composebox.updateComplete;

        // Clear all inputs (only deletes file 2).
        composebox.clearAllInputs(false);
        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(1, composebox.files.size);

        const submitButton: HTMLButtonElement|null =
            getSubmitButton(composebox);
        const submitContainer: HTMLElement|null =
            getSubmitContainer(composebox);
        assertTrue(submitButton !== null, 'Submit button should exist');

        // There are no more deletable files, but the remaining undeletable
        // file supports unimodal search, so submit should be enabled.
        assertFalse(submitButton.disabled, 'Button should be enabled');

        assertTrue(
            submitContainer !== null, 'Submit container button should exist');

        assertStyle(
            submitContainer, 'cursor', 'pointer',
            'Submit button cursor should be pointer');
        assertStyle(
            submitContainer, 'pointer-events', 'auto',
            'Submit container should have pointer-events on.');

        // Reupload 2nd deleted file.
        await uploadFileAndVerify(
            token2, new File(['foo3'], 'foo3.jpg', {type: 'image/png'}),
            composebox, mockSearchboxPageHandler, 1);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token2, ContextUploadStatus.kUploadSuccessful, null);
        await searchboxCallbackRouterRemote.$.flushForTesting();
        await composebox.updateComplete;

        const currentFiles2 = composebox.files;
        currentFiles2.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.requestUpdate();

        await composebox.updateComplete;

        // Clear all inputs (deletes no files).
        composebox.clearAllInputs(false);
        await composebox.updateComplete;
        await microtasksFinished();
        assertEquals(2, composebox.files.size);

        assertTrue(submitButton !== null, 'Submit button should exist');
        // There are no more deletable files, but the remaining undeletable
        // files support unimodal search, so submit should be enabled.
        assertFalse(submitButton.disabled, 'Button should be enabled');

        assertTrue(
            submitContainer !== null, 'Submit container button should exist');

        assertStyle(
            submitContainer, 'cursor', 'pointer',
            'Submit button cursor should be pointer');
        assertStyle(
            submitContainer, 'pointer-events', 'auto',
            'Submit container should have pointer-events on.');
        assertEquals(2, composebox.files.size);
      });

  test('Composebox upload disabled when uploading files', async () => {
    composebox.searchboxLayoutMode = '';
    composebox.contextMenuEnabled = true;
    await composebox.updateComplete;
    await microtasksFinished();

    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(contextEntrypoint !== null);
    assertFalse(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be enabled');

    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
        composebox, mockSearchboxPageHandler);

    // Other processing state should result in not ready to submit.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );

    await microtasksFinished();
    await composebox.updateComplete;
    assertEquals(
        1, composebox.pendingUploads.size, '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');
    assertTrue(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be disabled while uploading');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );

    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        0, composebox.pendingUploads.size, '0 Files should be uploading');
    assertTrue(
        composebox.fileUploadsComplete, 'Files should be finished uploading');
    assertFalse(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be re-enabled after upload');
  });

  test(
      'Composebox upload disabled when uploading files with contextMenu',
      async () => {
        composebox.searchboxLayoutMode = '';
        composebox.contextMenuEnabled = true;
        await composebox.updateComplete;
        await microtasksFinished();

        const contextEntrypoint =
            composebox.shadowRoot.querySelector('#contextEntrypoint');
        assertTrue(contextEntrypoint !== null);

        const button =
            contextEntrypoint.shadowRoot?.querySelector('#entrypointButton');
        assertTrue(button !== null, 'Context menu button should exist');

        assertFalse(
            contextEntrypoint.uploadButtonDisabled,
            'Context menu button should be enabled at first');

        await uploadFileAndVerify(
            FAKE_TOKEN_STRING,
            new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}), composebox,
            mockSearchboxPageHandler);

        // Other processing state should result in not ready to submit.
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kProcessingSuggestSignalsReady,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            1, composebox.pendingUploads.size, '1 File should be uploading');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading');

        assertTrue(
            contextEntrypoint.uploadButtonDisabled,
            'Context menu button should be disabled while uploading');
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kUploadSuccessful,
            /*error_type=*/ null,
        );

        await microtasksFinished();
        await composebox.updateComplete;

        assertEquals(
            0, composebox.pendingUploads.size, '0 Files should be uploading');
        assertTrue(
            composebox.fileUploadsComplete,
            'Files should be finished uploading');
        assertFalse(
            contextEntrypoint.uploadButtonDisabled,
            'Context menu button should be enabled after upload');
      });

  test('image upload calls handler for image', async () => {
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(contextEntrypoint !== null);
    contextEntrypoint.dispatchEvent(new CustomEvent('open-image-upload', {
      detail: {isImage: true},
      bubbles: true,
      composed: true,
    }));

    await mockComposeboxPageHandler.whenCalled('handleFileUpload');
    assertEquals(1, mockComposeboxPageHandler.getCallCount('handleFileUpload'));
    const [isImage] = mockComposeboxPageHandler.getArgs('handleFileUpload');
    assertTrue(isImage);
  });

  test('file upload calls handler for file', async () => {
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(contextEntrypoint !== null);
    contextEntrypoint.dispatchEvent(new CustomEvent('open-file-upload', {
      detail: {isImage: false},
      bubbles: true,
      composed: true,
    }));

    await mockComposeboxPageHandler.whenCalled('handleFileUpload');
    assertEquals(1, mockComposeboxPageHandler.getCallCount('handleFileUpload'));
    const [isImage] = mockComposeboxPageHandler.getArgs('handleFileUpload');
    assertFalse(isImage);
  });
});
