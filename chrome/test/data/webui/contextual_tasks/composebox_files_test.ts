// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {ComposeboxFile, TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import {LensOverlayDismissalSource, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextUploadStatus, InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import type {CtComposeboxAppParts} from './contextual_tasks_test_utils.js';
import {assertStyle, createCtComposeboxApp, deleteLastFile, FAKE_TOKEN_STRING, FAKE_TOKEN_STRING_2, fixtureUrl, getSubmitButton, getSubmitContainer} from './contextual_tasks_test_utils.js';
import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {ADD_FILE_CONTEXT_FN, uploadFileAndVerify} from './test_searchbox_utils.js';

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
      useContextualTasksComposeboxFork: false,
      contextualMenuUsePecApi: false,
      composeboxSmartTabSharingVisible: false,
      contextManagementInComposeboxEnabled: false,
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
          menuTooltip: '',
        }],
      },
    }));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler, mockSearchboxPageHandler,
        searchboxCallbackRouter));

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

    assertEquals(0, composebox.attachedContext.size);

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

        const currentFiles = composebox.attachedContext;
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

        assertEquals(1, composebox.attachedContext.size);

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

        const currentFiles2 = composebox.attachedContext;
        currentFiles2.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.requestUpdate();

        await composebox.updateComplete;

        // Clear all inputs (deletes no files).
        composebox.clearAllInputs(false);
        await composebox.updateComplete;
        await microtasksFinished();
        assertEquals(2, composebox.attachedContext.size);

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
        assertEquals(2, composebox.attachedContext.size);
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

[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkFilesTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
      () => {
        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let searchboxCallbackRouterRemote: SearchboxPageRemote;
        let parts: CtComposeboxAppParts;

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

          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
            composeboxSmartTabSharingVisible: false,
            contextManagementInComposeboxEnabled: false,
            enableComposeboxJumpFix: false,
            composeboxShowTypedSuggest: true,
            composeboxShowZps: true,
            enableBasicModeZOrder: true,
            composeboxShowContextMenu: true,
            composeboxContextDragAndDropEnabled: true,
            enableFileHint: true,
            webUIOmniboxAskGAboutThisPageEnabled: false,
            supportsLensButtonInComposebox: true,
            lensSearchButtonLabel: 'Lens search',
            composeboxHintTextLensOverlay: 'Test Lens Hint',
            composeboxHintTextAskAboutThese: 'Ask about these',
            composeboxHintTextAskAboutThisTab: 'Ask about this tab',
            composeboxHintTextAskAboutThisImage: 'Ask about this image',
            composeboxHintTextAskAboutThisDoc: 'Ask about this doc',
            forcedEmbeddedPageHost: '',
            tabFaviconChipsToCoinsEnabled: false,
          });

          const testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
          BrowserProxyImpl.setInstance(testProxy);

          mockComposeboxPageHandler =
              TestMock.fromClass(ComposeboxPageHandlerRemote);
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          mockComposeboxPageHandler.setResultFor(
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          mockSearchboxPageHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: []}));
          mockSearchboxPageHandler.setResultFor(
              'getPageClassification',
              Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
          mockSearchboxPageHandler.setResultFor(
              'getInputState', Promise.resolve({state: new MockInputState()}));
          const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
          searchboxCallbackRouterRemote =
              searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
          ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);

          searchboxCallbackRouterRemote.onInputStateChanged(
              new MockInputState());
          await microtasksFinished();
        });

        test(
            'closes Lens overlay when image uploads are disabled', async () => {
              const disabledState = {
                ...new MockInputState(),
                disabledInputTypes: [InputType.kLensImage],
              };

              parts.innerComposebox.dispatchEvent(
                  new CustomEvent('input-state-changed', {
                    detail: {inputState: disabledState},
                    bubbles: true,
                    composed: true,
                  }));

              await microtasksFinished();

              assertEquals(
                  1,
                  mockComposeboxPageHandler.getCallCount(
                      'closeLensOverlayFromWebUI'));
              assertEquals(
                  LensOverlayDismissalSource
                      .kContextualTasksImageUploadsDisabled,
                  mockComposeboxPageHandler.getArgs(
                      'closeLensOverlayFromWebUI')[0]);
            });

        test(
            'lens button is disabled when image uploads are disabled',
            async () => {
              const disabledState = {
                ...new MockInputState(),
                disabledInputTypes: [InputType.kLensImage],
              };

              searchboxCallbackRouterRemote.onInputStateChanged(disabledState);
              await searchboxCallbackRouterRemote.$.flushForTesting();
              await parts.wrapper.updateComplete;
              await parts.innerComposebox.updateComplete;
              await microtasksFinished();

              assertTrue(parts.innerComposebox.lensButtonDisabled);
            });

        test(
            'lens button renders in side panel and click opens overlay',
            async () => {
              const {wrapper, innerComposebox} = parts;
              wrapper.isSidePanel = true;
              await wrapper.updateComplete;
              await innerComposebox.updateComplete;

              const lensIcon =
                  innerComposebox.shadowRoot.querySelector<CrIconButtonElement>(
                      '#lensIcon');
              assertTrue(
                  lensIcon !== null, 'Lens icon should render in side panel');
              assertEquals(lensIcon, innerComposebox.getLensButtonElement());

              lensIcon.click();
              await mockComposeboxPageHandler.whenCalled(
                  'handleLensButtonClick');
              assertEquals(
                  1,
                  mockComposeboxPageHandler.getCallCount(
                      'handleLensButtonClick'));
              assertEquals(
                  0,
                  mockComposeboxPageHandler.getCallCount('handleFileUpload'));
            });

        test('lens icon disabled state reflects on the icon', async () => {
          const {wrapper, innerComposebox} = parts;
          wrapper.isSidePanel = true;
          await wrapper.updateComplete;

          searchboxCallbackRouterRemote.onInputStateChanged({
            ...new MockInputState(),
            disabledInputTypes: [InputType.kLensImage],
          });
          await searchboxCallbackRouterRemote.$.flushForTesting();
          await wrapper.updateComplete;
          await innerComposebox.updateComplete;

          const lensIcon =
              innerComposebox.shadowRoot.querySelector<CrIconButtonElement>(
                  '#lensIcon');
          assertTrue(lensIcon !== null);
          assertTrue(lensIcon.disabled);
        });

        test('file inputs are disabled', () => {
          const fileInputs = parts.innerComposebox.$.fileInputs;
          assertTrue(fileInputs.disableFileInputs);
          assertFalse(!!fileInputs.shadowRoot.querySelector('#imageInput'));
        });

        test(
            'file upload renders the carousel and delete removes it',
            async () => {
              const {innerComposebox} = parts;
              const testFile =
                  new File(['test'], 'test.jpg', {type: 'image/jpeg'});
              await uploadFileAndVerify(
                  FAKE_TOKEN_STRING, testFile, innerComposebox,
                  mockSearchboxPageHandler);

              const carouselContainer =
                  innerComposebox.shadowRoot.querySelector(
                      '#carouselContainer');
              assertTrue(carouselContainer !== null);
              assertEquals(
                  'carousel-container', carouselContainer.getAttribute('part'));
              const carousel =
                  innerComposebox.shadowRoot.querySelector('#carousel');
              assertTrue(carousel !== null);
              assertEquals(
                  'cr-composebox-file-carousel', carousel.getAttribute('part'));
              const exportparts = carousel.getAttribute('exportparts');
              assertTrue(exportparts !== null);
              const exportedParts = exportparts.split(',').map(p => p.trim());
              assertTrue(exportedParts.includes('thumbnail'));
              assertTrue(exportedParts.includes('thumbnail-title'));

              await deleteLastFile(innerComposebox);
              await innerComposebox.updateComplete;
              assertFalse(
                  !!innerComposebox.shadowRoot.querySelector('#carousel'));
            });

        test(
            'paste with a file attaches it and renders the carousel',
            async () => {
              const {innerComposebox} = parts;
              assertFalse(
                  !!innerComposebox.shadowRoot.querySelector('#carousel'));

              mockSearchboxPageHandler.resetResolver(ADD_FILE_CONTEXT_FN);
              mockSearchboxPageHandler.setResultFor(
                  ADD_FILE_CONTEXT_FN, Promise.resolve(FAKE_TOKEN_STRING));

              const dataTransfer = new DataTransfer();
              dataTransfer.items.add(
                  new File(['test'], 'test.jpg', {type: 'image/jpeg'}));
              const composeboxDiv =
                  innerComposebox.shadowRoot.querySelector<HTMLElement>(
                      '#composebox');
              assertTrue(composeboxDiv !== null);

              let pasteEvent = new ClipboardEvent('paste', {
                clipboardData: dataTransfer,
                bubbles: true,
                composed: true,
              });
              if (!pasteEvent.clipboardData) {
                // The clipboardData constructor init is ignored in some
                // environments; fall back to injecting the property.
                pasteEvent = new Event('paste', {
                               bubbles: true,
                               composed: true,
                             }) as ClipboardEvent;
                Object.defineProperty(
                    pasteEvent, 'clipboardData', {value: dataTransfer});
              }
              composeboxDiv.dispatchEvent(pasteEvent);

              await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);
              await innerComposebox.updateComplete;
              await microtasksFinished();

              assertEquals(1, innerComposebox.attachedContext.size);
              assertTrue(
                  !!innerComposebox.shadowRoot.querySelector('#carousel'));
            });

        test('file hint updates the input placeholder', async () => {
          const {innerComposebox} = parts;
          const imageFile =
              new File(['test'], 'test.jpg', {type: 'image/jpeg'});
          await uploadFileAndVerify(
              FAKE_TOKEN_STRING, imageFile, innerComposebox,
              mockSearchboxPageHandler);
          assertEquals(
              'Ask about this image', innerComposebox.inputPlaceholder);

          const pdfFile =
              new File(['test2'], 'test2.pdf', {type: 'application/pdf'});
          await uploadFileAndVerify(
              FAKE_TOKEN_STRING_2, pdfFile, innerComposebox,
              mockSearchboxPageHandler, 1);
          assertEquals('Ask about these', innerComposebox.inputPlaceholder);
        });

        test('single tab file updates the input placeholder', async () => {
          const {innerComposebox} = parts;
          const token = {high: 0n, low: 1n} as unknown as UnguessableToken;
          const file = new ComposeboxFile(
              token, 'test.tab', 'tab', InputType.kBrowserTab);
          innerComposebox.addFileContextForTesting(file);
          await innerComposebox.updateComplete;

          assertEquals('Ask about this tab', innerComposebox.inputPlaceholder);
        });

        test('single pdf file updates the input placeholder', async () => {
          const {innerComposebox} = parts;
          const pdfFile =
              new File(['test'], 'test.pdf', {type: 'application/pdf'});
          await uploadFileAndVerify(
              FAKE_TOKEN_STRING, pdfFile, innerComposebox,
              mockSearchboxPageHandler);
          assertEquals('Ask about this doc', innerComposebox.inputPlaceholder);
        });

        test('single unknown file does not update placeholder', async () => {
          const {innerComposebox} = parts;
          const token = {high: 0n, low: 1n} as unknown as UnguessableToken;
          const file = new ComposeboxFile(
              token, 'unknown.dat', 'unknown/type', InputType.kLensFile);
          innerComposebox.addFileContextForTesting(file);
          await innerComposebox.updateComplete;

          const placeholder = innerComposebox.inputPlaceholder;
          assertTrue(
              !placeholder.includes('Ask about'),
              `Placeholder '${placeholder}' should not include 'Ask about'`);
        });

        test('file hint skips the automatic active tab', async () => {
          const {innerComposebox} = parts;
          mockSearchboxPageHandler.setResultFor(
              'addTabContext',
              Promise.resolve('0000000000000000DDDDDDDDDDDDDD04'));
          searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
              {
                tabId: 1,
                title: 'Auto tab',
                url: 'https://auto.example.com',
                lastActive: {internalValue: BigInt(100)},
                showInCurrentTabChip: true,
                showInPreviousTabChip: false,
              },
              null);
          await searchboxCallbackRouterRemote.$.flushForTesting();
          await mockSearchboxPageHandler.whenCalled('addTabContext');
          await microtasksFinished();
          await innerComposebox.updateComplete;

          assertEquals(1, innerComposebox.attachedContext.size);
          assertNotEquals(
              'Ask about this tab', innerComposebox.inputPlaceholder);

          const imageFile =
              new File(['test'], 'test.jpg', {type: 'image/jpeg'});
          await uploadFileAndVerify(
              FAKE_TOKEN_STRING, imageFile, innerComposebox,
              mockSearchboxPageHandler, 1);
          assertEquals('Ask about these', innerComposebox.inputPlaceholder);
        });
      });
});

function disableAnimationsRecursively(element: Element) {
  const noAnimation = document.createElement('style');
  noAnimation.textContent = `
    :host, * {
    transition: none !important;
    animation: none !important;
    transition-duration: 0s !important;
    animation-duration: 0s !important;
    }
  `;

  if (element.shadowRoot) {
    element.shadowRoot.appendChild(noAnimation);

    const children = element.shadowRoot.querySelectorAll('*');
    children.forEach(child => disableAnimationsRecursively(child));
  }
}

[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkAutoTabTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
      () => {
        const AUTO_TOKEN = '0000000000000000AAAAAAAAAAAAAA01';
        const MANUAL_TOKEN = '0000000000000000BBBBBBBBBBBBBB02';
        const REPLACEMENT_TOKEN = '0000000000000000CCCCCCCCCCCCCC03';
        const ADD_TAB_CONTEXT_FN = 'addTabContext';
        const QUERY_AUTOCOMPLETE_FN = 'queryAutocomplete';
        const STOP_AUTOCOMPLETE_FN = 'stopAutocomplete';

        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let searchboxCallbackRouterRemote: SearchboxPageRemote;
        let parts: CtComposeboxAppParts;
        let recordedUserActions: string[];
        let recordedBooleans: Array<{name: string, value: boolean}>;

        function createTabInfo(tabId: number, title: string, url: string) {
          return {
            tabId,
            title,
            url,
            lastActive: {internalValue: BigInt(100)},
            showInCurrentTabChip: true,
            showInPreviousTabChip: false,
          };
        }

        async function pushAutoTab(
            tabInfo: ReturnType<typeof createTabInfo>|null,
            invocationSource: string|null = null) {
          searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
              tabInfo, invocationSource);
          await searchboxCallbackRouterRemote.$.flushForTesting();
        }

        async function settle() {
          await microtasksFinished();
          await parts.innerComposebox.updateComplete;
          await microtasksFinished();
        }

        // Resets the resolver (restoring the result token) so the wait below
        // never reuses an already-resolved whenCalled().
        async function expectAddTabContext(
            token: string, action: () => void|Promise<void>) {
          mockSearchboxPageHandler.resetResolver(ADD_TAB_CONTEXT_FN);
          mockSearchboxPageHandler.setResultFor(
              ADD_TAB_CONTEXT_FN, Promise.resolve(token));
          await action();
          await mockSearchboxPageHandler.whenCalled(ADD_TAB_CONTEXT_FN);
        }

        function getEntrypointAndMenu() {
          const entrypoint = parts.innerComposebox.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypoint);
          return entrypoint;
        }

        async function addManualTab(
            token: string, tabId: number, title: string, url: string) {
          await expectAddTabContext(token, () => {
            getEntrypointAndMenu().dispatchEvent(
                new CustomEvent('add-tab-context', {
                  detail: {
                    id: tabId,
                    title,
                    url,
                    delayUpload: false,
                    origin: TabUploadOrigin.CURRENT_TAB_CHIP,
                  },
                  bubbles: true,
                  composed: true,
                }));
          });
          await settle();
        }

        async function mountApp() {
          parts = await createCtComposeboxApp(useFork);
          searchboxCallbackRouterRemote.onInputStateChanged(
              new MockInputState());
          await settle();
        }

        function hasFileWithTabId(tabId: number): boolean {
          return Array.from(parts.innerComposebox.attachedContext.values())
              .some((file: ComposeboxFile) => file.tabId === tabId);
        }

        async function assertDefaultAutoTabSemantics(
            invocationSource: string|null, isSidePanel: boolean) {
          await mountApp();
          const {innerComposebox, wrapper} = parts;
          if (isSidePanel) {
            wrapper.isSidePanel = true;
            await wrapper.updateComplete;
            await innerComposebox.updateComplete;
          }

          const stopCallCount =
              mockSearchboxPageHandler.getCallCount(STOP_AUTOCOMPLETE_FN);
          await expectAddTabContext(
              AUTO_TOKEN,
              () => pushAutoTab(
                  createTabInfo(1, 'Auto tab', 'https://a.example.com'),
                  invocationSource));
          await settle();

          const addArgs = mockSearchboxPageHandler.getArgs(ADD_TAB_CONTEXT_FN);
          assertEquals(true, addArgs[addArgs.length - 1][1]);
          assertTrue(
              mockSearchboxPageHandler.getCallCount(STOP_AUTOCOMPLETE_FN) >=
              stopCallCount + 1);

          const queryCallCount =
              mockSearchboxPageHandler.getCallCount(QUERY_AUTOCOMPLETE_FN);
          await pushAutoTab(null, invocationSource);
          await settle();

          assertFalse(innerComposebox.getHasAutomaticActiveTabChipToken());
          assertEquals(
              queryCallCount + 1,
              mockSearchboxPageHandler.getCallCount(QUERY_AUTOCOMPLETE_FN));
        }

        setup(() => {
          if (!window.chrome) {
            Object.assign(window, {chrome: {}});
          }
          // Replace any pre-existing no-op stub unconditionally; the mixin
          // InputStateDeletion path also calls recordEnumerationValue.
          recordedUserActions = [];
          recordedBooleans = [];
          Object.assign(window.chrome, {
            histograms: {
              recordEnumerationValue: () => {},
              recordUserAction: (name: string) =>
                  recordedUserActions.push(name),
              recordBoolean: (name: string, value: boolean) =>
                  recordedBooleans.push({name, value}),
            },
          });
          document.body.innerHTML = window.trustedTypes!.emptyHTML;

          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
            composeboxSmartTabSharingVisible: false,
            contextManagementInComposeboxEnabled: false,
            enableComposeboxJumpFix: false,
            composeboxShowTypedSuggest: true,
            composeboxShowZps: true,
            enableBasicModeZOrder: true,
            composeboxShowContextMenu: true,
            webUIOmniboxAskGAboutThisPageEnabled: false,
            forcedEmbeddedPageHost: '',
            tabFaviconChipsToCoinsEnabled: false,
          });

          const testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
          BrowserProxyImpl.setInstance(testProxy);

          mockComposeboxPageHandler =
              TestMock.fromClass(ComposeboxPageHandlerRemote);
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          mockComposeboxPageHandler.setResultFor(
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          mockSearchboxPageHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: []}));
          mockSearchboxPageHandler.setResultFor(
              'getPageClassification',
              Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
          mockSearchboxPageHandler.setResultFor(
              'getInputState', Promise.resolve({state: new MockInputState()}));
          const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
          searchboxCallbackRouterRemote =
              searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
          ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));
        });

        suite('ChipCreationAndMismatch', () => {
          test('creates the automatic tab chip from a tab update', async () => {
            await mountApp();
            const {innerComposebox} = parts;
            assertFalse(innerComposebox.getHasAutomaticActiveTabChipToken());

            await expectAddTabContext(
                AUTO_TOKEN,
                () => pushAutoTab(
                    createTabInfo(1, 'Auto tab', 'https://auto.example.com')));
            await settle();

            assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
            assertEquals(1, innerComposebox.attachedContext.size);
            const file =
                Array.from(innerComposebox.attachedContext.values())[0] as
                ComposeboxFile;
            assertEquals(1, file.tabId);
            assertEquals('Auto tab', file.name);
            assertEquals('https://auto.example.com', file.url);
          });

          test('updates the chip title in place', async () => {
            await mountApp();
            const {innerComposebox} = parts;
            await expectAddTabContext(
                AUTO_TOKEN,
                () => pushAutoTab(createTabInfo(
                    1, 'Initial Title', 'https://a.example.com')));
            await settle();

            // Suggest identical tab but updated title
            await pushAutoTab(
                createTabInfo(1, 'Updated Title', 'https://a.example.com'));
            await settle();
            assertEquals(1, innerComposebox.attachedContext.size);
            const updatedFile =
                Array.from(innerComposebox.attachedContext.values())[0] as
                ComposeboxFile;
            assertEquals('Updated Title', updatedFile.name);
            assertEquals(AUTO_TOKEN, updatedFile.uuid);

            // Suggest identical tab (same url/tabId), identical title, but
            // different lastActive
            await pushAutoTab({
              ...createTabInfo(1, 'Updated Title', 'https://a.example.com'),
              lastActive: {internalValue: BigInt(500)},
            });
            await settle();
            // Reference should be exactly the same (no re-allocation or
            // modification)
            assertEquals(
                updatedFile,
                Array.from(innerComposebox.attachedContext.values())[0]);
          });

          test(
              'url mismatch deletes the chip and returns without creating',
              async () => {
                await mountApp();
                const {innerComposebox} = parts;
                await addManualTab(
                    MANUAL_TOKEN, 2, 'Manual tab',
                    'https://manual.example.com');
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());

                const addCallCount =
                    mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN);
                await pushAutoTab(
                    createTabInfo(3, 'Other tab', 'https://b.example.com'));
                await settle();

                assertFalse(
                    innerComposebox.getHasAutomaticActiveTabChipToken());
                assertFalse(hasFileWithTabId(1));
                assertTrue(hasFileWithTabId(2));
                assertEquals(
                    addCallCount,
                    mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN));

                await expectAddTabContext(
                    REPLACEMENT_TOKEN,
                    () => pushAutoTab(createTabInfo(
                        3, 'Other tab', 'https://b.example.com')));
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
                assertTrue(hasFileWithTabId(3));
                assertTrue(hasFileWithTabId(2));
              });

          test(
              'url mismatch deletes and returns when ask-G is enabled',
              async () => {
                loadTimeData.overrideValues({
                  webUIOmniboxAskGAboutThisPageEnabled: true,
                });
                await mountApp();
                const {innerComposebox} = parts;
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();

                const addCallCount =
                    mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN);
                await pushAutoTab(
                    createTabInfo(3, 'Other tab', 'https://b.example.com'));
                await settle();
                assertFalse(
                    innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(
                    addCallCount,
                    mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN));

                await expectAddTabContext(
                    REPLACEMENT_TOKEN,
                    () => pushAutoTab(createTabInfo(
                        3, 'Other tab', 'https://b.example.com')));
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
              });
        });

        suite('DeletionSemantics', () => {
          test(
              'url mismatch deletes and returns in the side panel',
              async () => {
                await mountApp();
                const {innerComposebox, wrapper} = parts;
                wrapper.isSidePanel = true;
                await wrapper.updateComplete;
                await innerComposebox.updateComplete;

                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();

                const addCallCount =
                    mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN);
                await pushAutoTab(
                    createTabInfo(3, 'Other tab', 'https://b.example.com'));
                await settle();
                assertFalse(
                    innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(
                    addCallCount,
                    mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN));

                await expectAddTabContext(
                    REPLACEMENT_TOKEN,
                    () => pushAutoTab(createTabInfo(
                        3, 'Other tab', 'https://b.example.com')));
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
              });

          test('null update deletes the chip and requeries', async () => {
            await mountApp();
            const {innerComposebox} = parts;
            await addManualTab(
                MANUAL_TOKEN, 2, 'Manual tab', 'https://manual.example.com');
            await expectAddTabContext(
                AUTO_TOKEN,
                () => pushAutoTab(
                    createTabInfo(1, 'Auto tab', 'https://a.example.com')));
            await settle();

            const queryCallCount =
                mockSearchboxPageHandler.getCallCount(QUERY_AUTOCOMPLETE_FN);
            await pushAutoTab(null);
            await settle();

            assertFalse(innerComposebox.getHasAutomaticActiveTabChipToken());
            assertFalse(hasFileWithTabId(1));
            assertTrue(hasFileWithTabId(2));
            assertEquals(
                queryCallCount + 1,
                mockSearchboxPageHandler.getCallCount(QUERY_AUTOCOMPLETE_FN));
          });

          test(
              're-suggesting the same tab URL after null update recreates ' +
                  'the chip',
              async () => {
                await mountApp();
                const {innerComposebox} = parts;

                // Initial suggestion creates the chip.
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(1, innerComposebox.files.size);

                // Null update deletes the chip.
                await pushAutoTab(null);
                await settle();
                assertFalse(
                    innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(0, innerComposebox.files.size);

                // Re-suggesting the exact same URL recreates the chip.
                await expectAddTabContext(
                    REPLACEMENT_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(1, innerComposebox.files.size);
                assertTrue(hasFileWithTabId(1));
              });

          test(
              're-suggesting the same tab URL after user deletion recreates ' +
                  'the chip',
              async () => {
                await mountApp();
                const {innerComposebox} = parts;

                // Initial suggestion creates the chip.
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(1, innerComposebox.files.size);

                // User deletes the chip.
                const file = Array.from(innerComposebox.files.values())[0] as
                    ComposeboxFile;
                innerComposebox.deleteFile(file.uuid, /*fromUserAction=*/ true);
                await settle();
                assertFalse(
                    innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(0, innerComposebox.files.size);

                // Re-suggesting the exact same URL recreates the chip.
                await expectAddTabContext(
                    REPLACEMENT_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(1, innerComposebox.files.size);
                assertTrue(hasFileWithTabId(1));
              });

          test(
              'page action without ask-G in the side panel keeps the default ' +
                  'semantics',
              async () => {
                await assertDefaultAutoTabSemantics('OmniboxPageAction', true);
              });

          test(
              'page action with ask-G outside the side panel keeps the ' +
                  'default semantics',
              async () => {
                loadTimeData.overrideValues({
                  webUIOmniboxAskGAboutThisPageEnabled: true,
                });
                await assertDefaultAutoTabSemantics('OmniboxPageAction', false);
              });
        });

        suite('SourcesAndUploadTiming', () => {
          test(
              'app menu source with ask-G in the side panel keeps the ' +
                  'default semantics',
              async () => {
                loadTimeData.overrideValues({
                  webUIOmniboxAskGAboutThisPageEnabled: true,
                });
                await assertDefaultAutoTabSemantics('AppMenu', true);
              });

          test(
              'page action with ask-G in the side panel uploads immediately ' +
                  'and keeps the chip on null',
              async () => {
                loadTimeData.overrideValues({
                  webUIOmniboxAskGAboutThisPageEnabled: true,
                });
                await mountApp();
                const {innerComposebox, wrapper} = parts;
                wrapper.isSidePanel = true;
                await wrapper.updateComplete;
                await innerComposebox.updateComplete;

                const stopCallCount =
                    mockSearchboxPageHandler.getCallCount(STOP_AUTOCOMPLETE_FN);
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com'),
                        'OmniboxPageAction'));
                await settle();

                const addArgs =
                    mockSearchboxPageHandler.getArgs(ADD_TAB_CONTEXT_FN);
                assertEquals(false, addArgs[addArgs.length - 1][1]);
                assertEquals(
                    stopCallCount,
                    mockSearchboxPageHandler.getCallCount(
                        STOP_AUTOCOMPLETE_FN));

                await pushAutoTab(null, 'OmniboxPageAction');
                await settle();
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(1, innerComposebox.attachedContext.size);
              });

          test('delays the upload outside the side panel', async () => {
            await mountApp();
            await expectAddTabContext(
                AUTO_TOKEN,
                () => pushAutoTab(
                    createTabInfo(1, 'Auto tab', 'https://a.example.com')));
            await settle();
            const addArgs =
                mockSearchboxPageHandler.getArgs(ADD_TAB_CONTEXT_FN);
            assertEquals(true, addArgs[addArgs.length - 1][1]);
          });

          test(
              'deferred upload issues one add and adopts the latest title',
              async () => {
                await mountApp();
                const {innerComposebox} = parts;

                mockSearchboxPageHandler.resetResolver(ADD_TAB_CONTEXT_FN);
                const {promise, resolve} =
                    Promise.withResolvers<typeof AUTO_TOKEN>();
                mockSearchboxPageHandler.setResultFor(
                    ADD_TAB_CONTEXT_FN, promise);

                await pushAutoTab(
                    createTabInfo(1, 'First title', 'https://a.example.com'));
                await mockSearchboxPageHandler.whenCalled(ADD_TAB_CONTEXT_FN);

                await pushAutoTab(
                    createTabInfo(1, 'Second title', 'https://a.example.com'));
                await microtasksFinished();
                assertEquals(
                    1,
                    mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN));

                resolve(AUTO_TOKEN);
                await settle();
                assertEquals(1, innerComposebox.attachedContext.size);
                const file =
                    Array.from(innerComposebox.attachedContext.values())[0] as
                    ComposeboxFile;
                assertEquals('Second title', file.name);
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
              });
        });

        suite('UserActionsAndLifecycle', () => {
          test(
              'tab updates are inert while detached and work after reattach',
              async () => {
                await mountApp();
                const {app, innerComposebox} = parts;

                document.body.removeChild(app);
                await pushAutoTab(
                    createTabInfo(1, 'Auto tab', 'https://a.example.com'));
                await microtasksFinished();
                assertEquals(
                    0,
                    mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN));
                assertEquals(0, innerComposebox.attachedContext.size);

                document.body.appendChild(app);
                await settle();
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();
                assertEquals(1, innerComposebox.attachedContext.size);
                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
              });

          test(
              'user deletion of the auto chip reports the auto-suggested flag',
              async () => {
                await mountApp();
                const {app, innerComposebox} = parts;
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();

                disableAnimationsRecursively(app);
                const chip = innerComposebox.getAutomaticActiveTabChipElement();
                assertTrue(!!chip);
                const chipShadow = chip.shadowRoot;
                assertTrue(!!chipShadow);
                const removeButton =
                    chipShadow.querySelector<HTMLElement>('.remove-button');
                assertTrue(!!removeButton);

                const queryCallCount = mockSearchboxPageHandler.getCallCount(
                    QUERY_AUTOCOMPLETE_FN);
                removeButton.click();
                await settle();

                const deleteArgs =
                    mockSearchboxPageHandler.getArgs('deleteContext');
                const lastDelete = deleteArgs[deleteArgs.length - 1];
                assertEquals(AUTO_TOKEN, lastDelete[0]);
                assertEquals(true, lastDelete[1]);

                const metricName =
                    'ContextualSearch.UserAction.DeleteAutoSuggestedTab.' +
                    loadTimeData.getString('composeboxSource');
                assertTrue(recordedUserActions.includes(metricName));
                assertTrue(recordedBooleans.some(
                    entry =>
                        entry.name === metricName && entry.value === true));

                assertFalse(
                    innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(
                    queryCallCount + 1,
                    mockSearchboxPageHandler.getCallCount(
                        QUERY_AUTOCOMPLETE_FN));
              });

          test(
              'deleting a manual tab keeps the auto chip and does not requery',
              async () => {
                await mountApp();
                const {app, innerComposebox} = parts;
                await addManualTab(
                    MANUAL_TOKEN, 2, 'Manual tab',
                    'https://manual.example.com');
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();

                disableAnimationsRecursively(app);
                const carousel =
                    innerComposebox.shadowRoot
                        .querySelector<ComposeboxFileCarouselElement>(
                            '#carousel');
                assertTrue(!!carousel);
                const manualChip =
                    carousel.getThumbnailElementByUuid(MANUAL_TOKEN);
                assertTrue(!!manualChip);
                const manualChipShadow = manualChip.shadowRoot;
                assertTrue(!!manualChipShadow);
                const removeButton =
                    manualChipShadow.querySelector<HTMLElement>(
                        '.remove-button');
                assertTrue(!!removeButton);

                const queryCallCount = mockSearchboxPageHandler.getCallCount(
                    QUERY_AUTOCOMPLETE_FN);
                const stopCallCount =
                    mockSearchboxPageHandler.getCallCount(STOP_AUTOCOMPLETE_FN);
                removeButton.click();
                await settle();

                const deleteArgs =
                    mockSearchboxPageHandler.getArgs('deleteContext');
                const lastDelete = deleteArgs[deleteArgs.length - 1];
                assertEquals(MANUAL_TOKEN, lastDelete[0]);
                assertEquals(false, lastDelete[1]);

                assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
                assertTrue(hasFileWithTabId(1));
                assertEquals(
                    queryCallCount,
                    mockSearchboxPageHandler.getCallCount(
                        QUERY_AUTOCOMPLETE_FN));
                assertTrue(
                    mockSearchboxPageHandler.getCallCount(
                        STOP_AUTOCOMPLETE_FN) >= stopCallCount + 1);
                assertFalse(recordedUserActions.some(
                    name => name.startsWith(
                        'ContextualSearch.UserAction.' +
                        'DeleteAutoSuggestedTab.')));
              });

          test('cancel resets the chip state for a fresh chip', async () => {
            await mountApp();
            const {innerComposebox} = parts;
            await expectAddTabContext(
                AUTO_TOKEN,
                () => pushAutoTab(
                    createTabInfo(1, 'Auto tab', 'https://a.example.com')));
            await settle();
            assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());

            const cancelIcon = innerComposebox.getInputElement().$.cancelIcon;
            assertTrue(!!cancelIcon);
            cancelIcon.click();
            await settle();
            assertFalse(innerComposebox.getHasAutomaticActiveTabChipToken());
            assertEquals(0, innerComposebox.attachedContext.size);

            // The pending url/title guard must reset too: the same url pushed
            // again creates a fresh chip instead of being deduped away.
            await expectAddTabContext(
                REPLACEMENT_TOKEN,
                () => pushAutoTab(
                    createTabInfo(1, 'Auto tab', 'https://a.example.com')));
            await settle();
            assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());
            assertEquals(1, innerComposebox.attachedContext.size);
          });
        });

        // The searchbox Smart Tab Sharing surface is gated out of the
        // is_android mojom types.
        // <if expr="not is_android">
        suite('SmartTabSharing', () => {
          test(
              'smart tab sharing callback clears manual and auto tabs',
              async () => {
                await mountApp();
                const {innerComposebox} = parts;
                await addManualTab(
                    MANUAL_TOKEN, 2, 'Manual tab',
                    'https://manual.example.com');
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();
                assertEquals(2, innerComposebox.attachedContext.size);

                const queryCallCount = mockSearchboxPageHandler.getCallCount(
                    QUERY_AUTOCOMPLETE_FN);
                searchboxCallbackRouterRemote.updateSmartTabSharingActive(true);
                await searchboxCallbackRouterRemote.$.flushForTesting();
                await settle();

                assertEquals(0, innerComposebox.attachedContext.size);
                assertFalse(
                    innerComposebox.getHasAutomaticActiveTabChipToken());
                // Exactly one requery proves the auto tab was excluded from
                // clearManualTabs_ and deleted through the pointer-first path.
                assertEquals(
                    queryCallCount + 1,
                    mockSearchboxPageHandler.getCallCount(
                        QUERY_AUTOCOMPLETE_FN));
              });

          test(
              'smart tab sharing visible-change fetch clears the tabs',
              async () => {
                await mountApp();
                const {innerComposebox} = parts;
                await addManualTab(
                    MANUAL_TOKEN, 2, 'Manual tab',
                    'https://manual.example.com');
                await expectAddTabContext(
                    AUTO_TOKEN,
                    () => pushAutoTab(
                        createTabInfo(1, 'Auto tab', 'https://a.example.com')));
                await settle();

                const queryCallCount = mockSearchboxPageHandler.getCallCount(
                    QUERY_AUTOCOMPLETE_FN);
                mockSearchboxPageHandler.setResultFor(
                    'getSmartTabSharingActive',
                    Promise.resolve({active: true}));
                innerComposebox.smartTabSharingVisible = true;
                await innerComposebox.updateComplete;
                await settle();

                assertEquals(0, innerComposebox.attachedContext.size);
                assertFalse(
                    innerComposebox.getHasAutomaticActiveTabChipToken());
                assertEquals(
                    queryCallCount + 1,
                    mockSearchboxPageHandler.getCallCount(
                        QUERY_AUTOCOMPLETE_FN));
              });

          test('no chip is created while sharing is active', async () => {
            await mountApp();
            const {innerComposebox} = parts;
            await expectAddTabContext(
                AUTO_TOKEN,
                () => pushAutoTab(
                    createTabInfo(1, 'Auto tab', 'https://a.example.com')));
            await settle();
            assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());

            searchboxCallbackRouterRemote.updateSmartTabSharingActive(true);
            await searchboxCallbackRouterRemote.$.flushForTesting();
            await settle();
            assertEquals(0, innerComposebox.attachedContext.size);

            const addCallCount =
                mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN);
            await pushAutoTab(
                createTabInfo(3, 'Other tab', 'https://b.example.com'));
            await settle();
            assertEquals(
                addCallCount,
                mockSearchboxPageHandler.getCallCount(ADD_TAB_CONTEXT_FN));
            assertEquals(0, innerComposebox.attachedContext.size);
            assertFalse(innerComposebox.getHasAutomaticActiveTabChipToken());
          });
        });
        // </if>
      });
});
