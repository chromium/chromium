// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For composebox tests related to tools, secondary inputs (drag/drop).
import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextUploadStatus, InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import type {ComposeboxFileThumbnailElement} from 'chrome://resources/cr_components/composebox/file_thumbnail.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {FAKE_TOKEN_STRING, fixtureUrl, getSubmitContainer} from './contextual_tasks_test_utils.js';
import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {ADD_FILE_CONTEXT_FN, ADD_TAB_CONTEXT_FN} from './test_searchbox_utils.js';

async function dispatchDragAndDropEvent(dropZone: Element, files: File[]) {
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

suite('ContextualTasksComposeboxMiscInputsTest', () => {
  let contextualTasksApp: ContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
      ComposeboxPageHandlerRemote;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
      SearchboxPageHandlerRemote;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let mockTimer: MockTimer;

  async function setActiveTool(tool: ToolMode) {
    searchboxCallbackRouterRemote.onInputStateChanged({
      ...new MockInputState(),
      activeTool: tool,
    });
    await microtasksFinished();
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockTimer = new MockTimer();

    loadTimeData.overrideValues({
      useContextualTasksComposeboxFork: false,
      contextualMenuUsePecApi: false,
      composeboxSmartTabSharingVisible: false,
      contextManagementInComposeboxEnabled: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
    });

    testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockComposeboxPageHandler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));

    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler, mockSearchboxPageHandler,
        searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app');
    await customElements.whenDefined('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);

    await microtasksFinished();

    disableAnimationsRecursively(contextualTasksApp);

    composebox = contextualTasksApp.$.composebox.$.composebox;

    searchboxCallbackRouterRemote.onInputStateChanged(new MockInputState());
    await microtasksFinished();
  });

  teardown(() => {
    mockTimer.uninstall();
  });


  function getThumbnailForTab(token: string, expectExists: boolean = true):
      ComposeboxFileThumbnailElement|null {
    const allThumbnails: ComposeboxFileThumbnailElement[] = Array.from(
        composebox.$.carousel.shadowRoot.querySelectorAll('.file-thumbnail'));

    const tabThumbnail = allThumbnails.find((el) => {
      const uuid = el.file?.uuid as unknown as ({token?: string} | string);
      const elementId =
          typeof uuid === 'object' && uuid !== null ? uuid.token || uuid : uuid;

      return elementId === token;
    });
    if (expectExists) {
      assertTrue(!!tabThumbnail, 'Could not find the tab thumbnail in the DOM');
    } else {
      assertFalse(!!tabThumbnail, 'Tab thumbnail should not exist in the DOM');
    }
    return tabThumbnail || null;
  }

  test('sets is-dragging-file attribute on dragenter', async () => {
    // Get composebox div in cr-composebox
    const dropZone = composebox.$.composebox;

    assertFalse(composebox.hasAttribute('is-dragging-file'));

    dropZone.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertTrue(composebox.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.DRAGGING, composebox.animationState);
  });

  test('removes is-dragging-file attribute on dragleave', async () => {
    const dropZone = composebox.$.composebox;

    composebox.animationState = GlowAnimationState.DRAGGING;
    dropZone.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    dropZone.dispatchEvent(new DragEvent('dragleave', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertFalse(composebox.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.NONE, composebox.animationState);
  });

  test('accepts a dropped file and adds it to the carousel', async () => {
    const dropZone = composebox.$.composebox;
    // Same token for auto inject (mac) and manual (linux/windows)
    const sharedToken = '12345678-1234-1234-1234-123456789abc';
    mockSearchboxPageHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve(sharedToken));

    const file = new File(['content'], 'foo.pdf', {type: 'application/pdf'});
    // Automatically add file (Mac)
    await dispatchDragAndDropEvent(dropZone, [file]);

    await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(1, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
    assertFalse(composebox.hasAttribute('is-dragging-file'));

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
    composebox.addFileContextForTesting(mockAddedFile);
    await microtasksFinished();

    const carousel: ComposeboxFileCarouselElement|null =
        composebox.shadowRoot.querySelector('cr-composebox-file-carousel');

    assertTrue(!!carousel, 'Carousel should render');

    const carouselFiles = carousel.files;
    assertEquals(1, carouselFiles.length);
    assertEquals('foo.pdf', carouselFiles[0]!.name);
  });

  test('does not accept wrong file type', async () => {
    const dropZone = composebox.$.composebox;
    const testFile =
        new File(['foo'], 'malware.exe', {type: 'application/x-msdownload'});
    await dispatchDragAndDropEvent(dropZone, [testFile]);

    const expectedCallCount =
        loadTimeData.getBoolean('lensSendRawFileMediaTypesEnabled') ? 1 : 0;
    assertEquals(
        expectedCallCount,
        mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('accepts images', async () => {
    const dropZone = composebox.$.composebox;
    const file = new File(['content'], 'foo.png', {type: 'image/png'});
    await dispatchDragAndDropEvent(dropZone, [file]);
    await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(1, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('ExpandAnimationState', async function() {
    contextualTasksApp.$.composebox.startExpandAnimation();
    await microtasksFinished();
    assertEquals('expanding', composebox.animationState);
  });

  interface ToolModeInfo {
    toolMode: ToolMode;
    text: string;
  }

  [{
    toolMode: ToolMode.kDeepSearch,
    text: 'Deep Search',
  },
   {
     toolMode: ToolMode.kImageGen,
     text: 'Create Images',
   },
   {
     toolMode: ToolMode.kCanvas,
     text: 'Canvas',
   }].forEach((toolModeInfo: ToolModeInfo) => {
    test(
        toolModeInfo.text + 'tool click event triggers tool mode change',
        async () => {
          const contextEntrypoint =
              composebox.shadowRoot.querySelector('#contextEntrypoint');
          assertTrue(!!contextEntrypoint);

          await microtasksFinished();
          await composebox.updateComplete;

          await contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
            detail: {toolMode: toolModeInfo.toolMode},
            bubbles: true,
            composed: true,
          }));
          await setActiveTool(toolModeInfo.toolMode);
          assertEquals(
              toolModeInfo.toolMode, composebox.inputState.activeTool,
              'Active tool should be' + toolModeInfo.text +
                  ' after clicking tool');
          await contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
            detail: {toolMode: toolModeInfo.toolMode},
            bubbles: true,
            composed: true,
          }));

          await setActiveTool(ToolMode.kUnspecified);

          assertEquals(
              ToolMode.kUnspecified, composebox.inputState.activeTool,
              'Active tool should be unspecified after clicking tool twice');
        });

    test(
        toolModeInfo.text + ' tool is not reset after submitting a query',
        async () => {
          await setActiveTool(toolModeInfo.toolMode);
          await microtasksFinished();

          let toolChip =
              composebox.shadowRoot.querySelector('cr-composebox-tool-chip');

          assertTrue(!!toolChip, toolModeInfo.text + ' chip should be present');
          composebox.getInputElement().$.input.value = 'test';
          composebox.getInputElement().$.input.dispatchEvent(
              new Event('input'));
          // Since we cannot create a fake AutocompleteResult easily (35+
          // fields), we populate the result in a different way. There is an
          // assert statement in cr-component composebox.ts that checks if
          // AutocompleteResult is present, as it indicates if `input` is
          // present, as well as things like `contextFileSize` being nonzero).
          composebox.contextFilesSize_ = 2;
          await composebox.updateComplete;
          await microtasksFinished();

          getSubmitContainer(composebox)!.click();

          await composebox.updateComplete;
          await microtasksFinished();

          toolChip =
              composebox.shadowRoot.querySelector('cr-composebox-tool-chip');
          assertTrue(
              !!toolChip,
              toolModeInfo.text + 'chip not should be hidden' +
                  'after submitting');
        });

    test(toolModeInfo.text + ' mode: cancel resets mode', async () => {
      await setActiveTool(toolModeInfo.toolMode);

      let toolChip =
          composebox.shadowRoot.querySelector('cr-composebox-tool-chip');

      assertTrue(!!toolChip, toolModeInfo.text + ' chip should be present');
      // Simulate cancel button click without having to fully render button.
      composebox.onCancelClick_();
      await setActiveTool(ToolMode.kUnspecified);

      await composebox.updateComplete;
      await microtasksFinished();

      toolChip = composebox.shadowRoot.querySelector('cr-composebox-tool-chip');
      assertFalse(!!toolChip, toolModeInfo.text + ' chip should be removed');
    });

    test(toolModeInfo.text + ' mode: esc resets mode', async () => {
      await setActiveTool(toolModeInfo.toolMode);
      let toolChip =
          composebox.shadowRoot.querySelector('cr-composebox-tool-chip');

      assertTrue(!!toolChip, toolModeInfo.text + ' chip should be present');
      composebox.handleEscapeKeyLogic();
      await setActiveTool(ToolMode.kUnspecified);

      await composebox.updateComplete;
      await microtasksFinished();

      toolChip = composebox.shadowRoot.querySelector('cr-composebox-tool-chip');
      assertFalse(!!toolChip, toolModeInfo.text + ' chip should be removed');
    });
  });

  test('Tab spinner on regular file upload triggers', async () => {
    // Upload tab.
    mockSearchboxPageHandler.setResultFor(
        ADD_TAB_CONTEXT_FN, Promise.resolve(FAKE_TOKEN_STRING));
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    contextEntrypoint.fire('add-tab-context', {
      id: 0,
      title: 'test',
      url: new URL('about:blank'),
      delayUpload: false,
    });

    // Mark the tab as `kNotUploaded`.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kNotUploaded,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    // Check that tab spinner does not exist + tab not uploading.
    let tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);

    assertEquals(
        0, composebox.pendingUploads.size,
        '0 tab should not be uploading after not uploaded status');

    // Start upload.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadStarted,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;

    assertEquals(
        0, composebox.pendingUploads.size,
        '0 tab should be uploading after upload started since processing startts it');

    assertTrue(
        composebox.fileUploadsComplete,
        'Tabs should be finished uploading since all uploads are started but not processing');

    tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);

    assertTrue(
        tabThumbnail!.getIsUploadingForTesting(),
        'Tab thumbnail spinner should trigger for uploadStarted tab');

    // Upload processing state.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessing,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);

    // Check that spinner exists + uploading tab.
    assertTrue(
        tabThumbnail!.getIsUploadingForTesting(),
        'Tab thumbnail spinner should trigger for processing regular tab');

    assertEquals(
        1, composebox.pendingUploads.size,
        '1 tab should be uploading after processing started');
    assertFalse(
        composebox.fileUploadsComplete,
        'Tabs should not be finished uploading' +
            ' since one started upload process');

    // Suggestions are ready now, but should still be uploading
    // (spinner should exist.)
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);

    assertTrue(
        tabThumbnail!.getIsUploadingForTesting(),
        'tab thumbnail spinner should trigger' +
            ' for suggest signals ready processing tab');

    assertEquals(
        1, composebox.pendingUploads.size,
        '1 tab should be uploading after suggest signals ready');
    assertFalse(
        composebox.fileUploadsComplete,
        'Tabs should not be finished uploading' +
            ' since one started upload process');

    // Upload completed.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);

    // Spinner should not exist + tab should not be uploading.
    assertFalse(
        tabThumbnail!.getIsUploadingForTesting(),
        'tab thumbnail spinner should not trigger for successful upload');

    assertEquals(
        0, composebox.pendingUploads.size,
        'No tabs should be uploading after upload successful');
    assertTrue(
        composebox.fileUploadsComplete,
        'Tabs should be finished uploading since all uploads are successful');
  });

  test('Tab spinner on autochip does not trigger', async () => {
    // Upload tab with `delayUpload` = true.
    mockSearchboxPageHandler.setResultFor(
        ADD_TAB_CONTEXT_FN, Promise.resolve(FAKE_TOKEN_STRING));

    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    contextEntrypoint.fire('add-tab-context', {
      id: 0,
      title: 'test',
      url: new URL('about:blank'),
      delayUpload: false,
    });

    // Tab is not uploaded yet. Spinner should not trigger
    // and tab should not be uploading.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kNotUploaded,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    let tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);

    assertFalse(tabThumbnail!.getIsUploadingForTesting());

    assertEquals(
        0, composebox.pendingUploads.size, '0 tabs should be uploading');

    // `delayUpload` is true, but `kProcessing` means the delay already
    // happened.
    //  Spinner should trigger.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessing,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);

    assertTrue(
        tabThumbnail!.getIsUploadingForTesting(),
        'Tab thumbnail spinner should trigger for processing autochip tab');

    assertEquals(
        1, composebox.pendingUploads.size,
        '1 tab should be uploading due to processing status');

    assertFalse(
        composebox.fileUploadsComplete,
        'Tabs should be not finished uploading since processing');

    // Tab has expired and should not trigger spinner or be uploading.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadExpired,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    assertEquals(
        0, composebox.pendingUploads.size,
        '0 tabs should be uploading due to expired status');
    assertEquals(
        0, composebox.attachedContext.size,
        '0 files should be in the composebox after upload expired');
  });

  test('Tab spinner on autochip does not trigger upon failure', async () => {
    // Upload tab with `delayUpload` = true.
    mockSearchboxPageHandler.setResultFor(
        ADD_TAB_CONTEXT_FN, Promise.resolve(FAKE_TOKEN_STRING));
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    contextEntrypoint.fire('add-tab-context', {
      id: 0,
      title: 'test',
      url: new URL('about:blank'),
      delayUpload: false,
    });

    // `delayUpload` is true, but `kProcessing` means the delay already
    // happened.
    //  Spinner should trigger.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kNotUploaded,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();
    let tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);
    assertEquals(
        0, composebox.pendingUploads.size,
        '0 tabs should not be uploading after not uploaded status');
    assertTrue(
        composebox.fileUploadsComplete,
        'Tabs should be finished uploading since not uploaded');

    assertFalse(
        tabThumbnail!.getIsUploadingForTesting(),
        'not uploading so no tab spinner');

    // Tab is processing, but since `delayUpload` is true, spinner should not
    // trigger.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessing,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    tabThumbnail = getThumbnailForTab(FAKE_TOKEN_STRING);

    assertTrue(
        tabThumbnail!.getIsUploadingForTesting(),
        'autochip tab thumbnail spinner should be triggered for processing tab');

    assertEquals(
        1, composebox.pendingUploads.size,
        '1 tab should be uploading after processing started');

    assertFalse(
        composebox.fileUploadsComplete,
        'Tabs should be uploading since started processing');

    // Tab has failed to upload and should not trigger spinner or be uploading.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadFailed,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await composebox.$.carousel.updateComplete;
    await microtasksFinished();

    assertEquals(
        0, composebox.pendingUploads.size,
        '0 tabs should be uploading due to expired status');
    assertEquals(
        0, composebox.attachedContext.size,
        '0 files should be in the composebox after upload failed');
  });
});
