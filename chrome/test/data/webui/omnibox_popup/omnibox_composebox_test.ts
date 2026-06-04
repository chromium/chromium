// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import {SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxComposeboxElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {ComposeboxProxyImpl} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {ComposeboxFile, TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ContextUploadErrorType, ContextUploadStatus, InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('OmniboxComposeboxTest', () => {
  let omniboxComposebox: OmniboxComposeboxElement;
  let mockPageHandler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let testProxy: TestSearchboxBrowserProxy;
  let originalWindowProxy: WindowProxy;

  setup(async () => {
    if (!originalWindowProxy) {
      originalWindowProxy = WindowProxy.getInstance();
    }
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      composeboxShowZps: true,
    });

    testProxy = new TestSearchboxBrowserProxy();
    testProxy.handler.setPromiseResolveFor('getInputState', {
      state: {
        allowedModels: [1],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
      },
    });
    SearchboxBrowserProxy.setInstance(testProxy);

    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    mockPageHandler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockPageHandler, new PageCallbackRouter(),
        testProxy.handler as unknown as SearchboxPageHandlerRemote,
        testProxy.callbackRouter as unknown as SearchboxPageCallbackRouter));

    omniboxComposebox = document.createElement('cr-omnibox-composebox');
    document.body.appendChild(omniboxComposebox);
    await microtasksFinished();
  });

  teardown(() => {
    if (originalWindowProxy) {
      WindowProxy.setInstance(originalWindowProxy);
    }
  });

  test(
      'Shift+Enter allows inserting a newline when input is focused and not empty',
      async () => {
        omniboxComposebox.input = 'Some text';
        await microtasksFinished();

        const inputElement = omniboxComposebox.getInputElement();
        inputElement.inputElement.focus();


        let preventDefaultCalled = false;
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        // Override preventDefault so that it updates the test variable.
        Object.defineProperty(event, 'preventDefault', {
          value: () => {
            preventDefaultCalled = true;
          },
        });

        assertEquals(omniboxComposebox.getActiveElement(), inputElement);

        omniboxComposebox.$.composebox.dispatchEvent(event);

        // When input is focused shift + enter should create a new line
        // and not submit. It should also not call preventDefault
        // as that is called before submission.
        assertFalse(preventDefaultCalled);
      });

  test(
      'Enter prevents inserting a newline when focus is not in dropdown',
      () => {
        let preventDefaultCalled = false;
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: false,
          bubbles: true,
          cancelable: true,
        });

        // Override preventDefault so that it updates the test variable.
        Object.defineProperty(event, 'preventDefault', {
          value: () => {
            preventDefaultCalled = true;
          },
        });

        assertFalse(
            omniboxComposebox.getActiveElement() ===
            omniboxComposebox.getDropdownElement());

        omniboxComposebox.$.composebox.dispatchEvent(event);

        // Enter should try to submit (and call preventDefault) if focus is in
        // input.
        assertTrue(preventDefaultCalled);
      });

  test('Enter submits query when focus is in dropdown', () => {
    let preventDefaultCalled = false;
    const event = new KeyboardEvent('keydown', {
      key: 'Enter',
      bubbles: true,
      cancelable: true,
    });

    // Override preventDefault so that it updates the test variable.
    Object.defineProperty(event, 'preventDefault', {
      value: () => {
        preventDefaultCalled = true;
      },
    });

    const originalGetActiveElement = omniboxComposebox.getActiveElement;
    omniboxComposebox.getActiveElement = () =>
        omniboxComposebox.getDropdownElement();

    omniboxComposebox.$.composebox.dispatchEvent(event);

    // Enter in the dropdown should try to submit query (and call
    // preventDefault).
    assertTrue(preventDefaultCalled);

    omniboxComposebox.getActiveElement = originalGetActiveElement;
  });

  test('Dropdown is hidden when there are no results', async () => {
    // Initially hidden as there are no results.
    assertFalse(omniboxComposebox.showDropdown);
    assertTrue(omniboxComposebox.$.matches.hidden);

    // Set results with matches.
    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: false,
      }),
    ];
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await testProxy.page.$.flushForTesting();
    await microtasksFinished();

    assertTrue(omniboxComposebox.showDropdown);
    assertFalse(omniboxComposebox.$.matches.hidden);

    // Set empty results.
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: [],
        }));
    await testProxy.page.$.flushForTesting();
    await microtasksFinished();

    assertFalse(omniboxComposebox.showDropdown);
    assertTrue(omniboxComposebox.$.matches.hidden);
  });

  test('Mojo callback router adds file context correctly', async () => {
    assertEquals(0, omniboxComposebox.files.size);
    const testToken = '12345678901234567890123456789012';
    const testFileInfo = {
      fileName: 'test_file.png',
      imageDataUrl: 'data:image/png;base64,sometestdata',
      mimeType: 'image/png',
      isDeletable: true,
      selectionTime: new Date(),
    };

    // Simulate Mojo Callback: Page interface callback router.
    testProxy.page.addFileContext(testToken, testFileInfo);
    await testProxy.page.$.flushForTesting();
    await microtasksFinished();

    // Verify it reached the map.
    assertEquals(1, omniboxComposebox.files.size);
    const addedFile = omniboxComposebox.files.get(testToken);
    assertTrue(!!addedFile);
    assertEquals('test_file.png', addedFile.name);
  });

  test('Tool chip renders when context menu is enabled', async () => {
    omniboxComposebox.contextMenuEnabled = true;
    omniboxComposebox.inToolMode = true;
    omniboxComposebox.searchboxLayoutMode = '';
    await microtasksFinished();

    const toolChip =
        omniboxComposebox.shadowRoot.querySelector('cr-composebox-tool-chip');

    assertTrue(!!toolChip);
  });

  test('Tool chip does not render when context menu is disabled', async () => {
    omniboxComposebox.contextMenuEnabled = false;
    omniboxComposebox.inToolMode = true;
    omniboxComposebox.searchboxLayoutMode = '';
    await microtasksFinished();

    const toolChip =
        omniboxComposebox.shadowRoot.querySelector('cr-composebox-tool-chip');

    assertFalse(!!toolChip);
  });

  test('Tool chip does not render when not in tool mode', async () => {
    omniboxComposebox.contextMenuEnabled = true;
    omniboxComposebox.inToolMode = false;
    omniboxComposebox.searchboxLayoutMode = '';
    await microtasksFinished();

    const toolChip =
        omniboxComposebox.shadowRoot.querySelector('cr-composebox-tool-chip');

    assertFalse(!!toolChip);
  });

  test('Tool chip does not render when in compact layout', async () => {
    omniboxComposebox.contextMenuEnabled = true;
    omniboxComposebox.inToolMode = true;
    omniboxComposebox.searchboxLayoutMode = 'Compact';
    await microtasksFinished();

    const toolChip =
        omniboxComposebox.shadowRoot.querySelector('cr-composebox-tool-chip');

    assertFalse(!!toolChip);
  });

  test('Add File Attachment (Unimodal) via addSearchContext', async () => {
    const mockToken = 'mock-file-token';
    const context = {
      input: 'test unimodal',
      attachments: [{
        fileAttachment: {
          uuid: mockToken,
          name: 'test.pdf',
          mimeType: 'application/pdf',
          imageDataUrl: null,  // Non-image
          errorType: null,
        },
      }],
      toolMode: 0,
    };
    // Mock queryAutocomplete on element to verify clearMatches=true.
    let clearMatchesPassed = false;
    omniboxComposebox.queryAutocomplete = (clearMatches?: boolean) => {
      assertTrue(clearMatches === true);
      clearMatchesPassed = true;
    };

    omniboxComposebox.addSearchContext(context);
    await microtasksFinished();

    assertEquals('test unimodal', omniboxComposebox.input);
    assertEquals(1, omniboxComposebox.files.size);
    const addedFile = omniboxComposebox.files.get(mockToken);
    assertTrue(!!addedFile);
    assertEquals('test.pdf', addedFile.name);
    assertEquals('application/pdf', addedFile.type);
    assertEquals(
        ContextUploadStatus.kNotUploaded,
        addedFile.status);  // Non-image starts as not uploaded.
    assertTrue(clearMatchesPassed);
  });

  test('Add Tab Attachment via addSearchContext', async () => {
    const mockToken = 'mock-tab-token';
    // Mock Mojo AddTabContext to resolve with mock token.
    testProxy.handler.setPromiseResolveFor('addTabContext', mockToken);
    const context = {
      input: '',
      attachments: [{
        tabAttachment: {
          tabId: 42,
          title: 'Google Search',
          url: 'https://google.com',
        },
      }],
      toolMode: 0,
    };

    omniboxComposebox.addSearchContext(context);
    await microtasksFinished();
    await testProxy.handler.whenCalled('addTabContext');
    await microtasksFinished();

    const args = testProxy.handler.getArgs('addTabContext')[0];
    assertEquals(42, args[0]);
    assertFalse(args[1]);
    assertEquals(1, omniboxComposebox.files.size);
    const addedFile = omniboxComposebox.files.get(mockToken);
    assertTrue(!!addedFile);
    assertEquals('Google Search', addedFile.name);
    assertEquals('tab', addedFile.type);
    assertEquals(ContextUploadStatus.kUploadSuccessful, addedFile.status);
    // Verify tab ID mapping.
    assertTrue(omniboxComposebox.addedTabsIds.has(42));
    assertEquals(mockToken, omniboxComposebox.addedTabsIds.get(42));
  });

  test(
      'Carousel renders when files are present, hides when empty', async () => {
        assertFalse(omniboxComposebox.showFileCarousel);
        let carousel = omniboxComposebox.shadowRoot.querySelector(
            'cr-composebox-file-carousel');
        assertFalse(!!carousel);
        const mockToken = 'mock-file-token-2';
        const file = new ComposeboxFile(
            mockToken, 'test.png', 'image/png', InputType.kLensImage);
        omniboxComposebox.files.set(mockToken, file);

        omniboxComposebox.files =
            new Map(omniboxComposebox.files);  // Trigger Lit update
        await microtasksFinished();

        // Carousel should be visible.
        assertTrue(omniboxComposebox.showFileCarousel);
        carousel = omniboxComposebox.shadowRoot.querySelector(
            'cr-composebox-file-carousel');
        assertTrue(!!carousel);

        // Clear files.
        omniboxComposebox.files.clear();
        omniboxComposebox.files = new Map(omniboxComposebox.files);
        await microtasksFinished();

        // Carousel should be hidden.
        assertFalse(omniboxComposebox.showFileCarousel);
        carousel = omniboxComposebox.shadowRoot.querySelector(
            'cr-composebox-file-carousel');
        assertFalse(!!carousel);
      });

  test('Delete File Attachment', async () => {
    const mockToken = 'mock-delete-file-token';
    const file = new ComposeboxFile(
        mockToken, 'delete_me.pdf', 'pdf', InputType.kLensFile);
    omniboxComposebox.files.set(mockToken, file);
    omniboxComposebox.files = new Map(omniboxComposebox.files);
    await microtasksFinished();
    let queryAutocompleteCalled = false;
    let queryAutocompleteClearMatches = false;
    omniboxComposebox.queryAutocomplete = (clearMatches?: boolean) => {
      assertTrue(clearMatches === true);
      queryAutocompleteCalled = true;
      queryAutocompleteClearMatches = clearMatches;
    };

    omniboxComposebox.deleteFile(mockToken, /*fromUserAction=*/ true);
    await microtasksFinished();

    assertFalse(omniboxComposebox.files.has(mockToken));
    const deleteArgs = testProxy.handler.getArgs('deleteContext')[0];
    assertEquals(mockToken, deleteArgs[0]);
    assertTrue(queryAutocompleteCalled);
    assertTrue(queryAutocompleteClearMatches);
  });

  test('Delete Tab Attachment clears index', async () => {
    const mockToken = 'mock-delete-tab-token';
    const file = new ComposeboxFile(
        mockToken, 'tab.html', 'tab', InputType.kBrowserTab, {tabId: 100});
    omniboxComposebox.files.set(mockToken, file);
    omniboxComposebox.addedTabsIds.set(100, mockToken);
    omniboxComposebox.files = new Map(omniboxComposebox.files);
    await microtasksFinished();

    omniboxComposebox.deleteFile(mockToken, /*fromUserAction=*/ true);
    await microtasksFinished();

    assertFalse(omniboxComposebox.files.has(mockToken));
    assertFalse(omniboxComposebox.addedTabsIds.has(100));
    const deleteArgs = testProxy.handler.getArgs('deleteContext')[0];
    assertEquals(mockToken, deleteArgs[0]);
  });

  test('Render Error Scrim on validation error', async () => {
    let scrim = omniboxComposebox.shadowRoot.querySelector('ntp-error-scrim');
    assertTrue(!!scrim);
    assertEquals('', scrim.errorMessage);
    const composebox =
        omniboxComposebox.shadowRoot.querySelector('#composebox');
    assertFalse(composebox!.hasAttribute('inert'));

    omniboxComposebox.errorMessage = 'File size exceeds 100 MiB';
    await microtasksFinished();

    // Scrim should be visible.
    scrim = omniboxComposebox.shadowRoot.querySelector('ntp-error-scrim');
    assertTrue(!!scrim);
    assertTrue(composebox!.hasAttribute('inert'));

    // Dismiss error scrim.
    scrim.dispatchEvent(new CustomEvent(
        'dismiss-error-scrim', {bubbles: true, composed: true}));
    await microtasksFinished();

    // Error cleared.
    assertEquals('', omniboxComposebox.errorMessage);
    assertFalse(composebox!.hasAttribute('inert'));
  });

  test('Add Attachment with validation error fails', async () => {
    const mockToken = 'mock-validation-error-token';
    const context = {
      input: '',
      attachments: [{
        fileAttachment: {
          uuid: mockToken,
          name: 'huge.zip',
          mimeType: 'application/zip',
          imageDataUrl: null,
          errorType:
              ContextUploadErrorType
                  .kBrowserProcessingFileTooLargeError,  // Validation error.
        },
      }],
      toolMode: 0,
    };

    omniboxComposebox.addSearchContext(context);
    await microtasksFinished();

    assertFalse(omniboxComposebox.files.has(mockToken));
    // Verify errorMessage set (i18n lookup, will be blank in test if not
    // overridden but we verify the property is set to a string).
    assertTrue(omniboxComposebox.errorMessage.length > 0);
  });

  test('Carousel delete-file event triggers deletion', async () => {
    const mockToken = 'mock-delete-event-token';
    const file = new ComposeboxFile(
        mockToken, 'test.png', 'image/png', InputType.kLensImage);
    omniboxComposebox.files.set(mockToken, file);
    omniboxComposebox.files = new Map(omniboxComposebox.files);
    await microtasksFinished();
    const carousel = omniboxComposebox.shadowRoot.querySelector(
        'cr-composebox-file-carousel');
    assertTrue(!!carousel);
    let deleteFileCalled = false;
    const originalDeleteFile = omniboxComposebox.deleteFile;
    omniboxComposebox.deleteFile = (token, fromUserAction) => {
      assertEquals(mockToken, token);
      assertTrue(fromUserAction === true);
      deleteFileCalled = true;
      return null;
    };

    carousel.dispatchEvent(new CustomEvent('delete-file', {
      detail: {uuid: mockToken, fromUserAction: true},
      bubbles: true,
      composed: true,
    }));

    assertTrue(deleteFileCalled);
    omniboxComposebox.deleteFile = originalDeleteFile;
  });

  test('addTabContextHandleCallback success adds tab to files', async () => {
    const testToken = 'mock-tab-token-101';
    testProxy.handler.setPromiseResolveFor('addTabContext', testToken);
    const tabUpload = {
      tabId: 101,
      title: 'Tab 101',
      url: 'https://tab101.com',
      delayUpload: false,
      origin: TabUploadOrigin.OTHER,
    };

    await omniboxComposebox.addTabContextHandleCallback(tabUpload);
    await microtasksFinished();

    assertEquals(1, omniboxComposebox.files.size);
    const addedFile = omniboxComposebox.files.get(testToken);
    assertTrue(!!addedFile);
    assertEquals('Tab 101', addedFile.name);
    assertEquals(101, addedFile.tabId);
  });

  test('addTabContextHandleCallback failure sets errorMessage', async () => {
    testProxy.handler.setPromiseRejectFor(
        'addTabContext',
        ContextUploadErrorType.kBrowserProcessingFileTooLargeError);
    const tabUpload = {
      tabId: 101,
      title: 'Tab 101',
      url: 'https://tab101.com',
      delayUpload: false,
      origin: TabUploadOrigin.OTHER,
    };
    // Mock i18n for the error key.
    omniboxComposebox.i18n = (key: string) => {
      if (key === 'composeboxFileUploadInvalidTooLarge') {
        return 'File too large error';
      }
      return key;
    };

    await omniboxComposebox.addTabContextHandleCallback(tabUpload);
    await microtasksFinished();

    assertEquals(0, omniboxComposebox.files.size);
    assertEquals('File too large error', omniboxComposebox.errorMessage);
  });

  test(
      'submit button is rendered in TallBottomContext with valid input',
      async () => {
        omniboxComposebox.searchboxNextEnabled = true;
        omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
        omniboxComposebox.input = 'test';
        await omniboxComposebox.updateComplete;
        await microtasksFinished();

        const composeboxSubmit =
            omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');

        assertTrue(!!composeboxSubmit);
      });

  test(
      'submit button is not rendered when searchboxNextEnabled is false',
      async () => {
        omniboxComposebox.searchboxNextEnabled = false;
        omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
        omniboxComposebox.input = 'test';
        await omniboxComposebox.updateComplete;
        await microtasksFinished();

        const composeboxSubmit =
            omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');

        assertFalse(!!composeboxSubmit);
      });

  test(
      'submit button is not rendered when layout is not TallBottomContext',
      async () => {
        omniboxComposebox.searchboxNextEnabled = true;
        omniboxComposebox.searchboxLayoutMode = 'Compact';
        omniboxComposebox.input = 'test';
        await omniboxComposebox.updateComplete;
        await microtasksFinished();

        const composeboxSubmit =
            omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');

        assertFalse(!!composeboxSubmit);
      });

  test(
      'submit button is not rendered when there is no input text',
      async () => {
        omniboxComposebox.searchboxNextEnabled = true;
        omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
        omniboxComposebox.input = '';
        await omniboxComposebox.updateComplete;
        await microtasksFinished();

        const composeboxSubmit =
            omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');

        assertFalse(!!composeboxSubmit);
      });

  test('submit button click leads to handler called', async () => {
    omniboxComposebox.searchboxNextEnabled = true;
    omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
    omniboxComposebox.input = 'test';
    omniboxComposebox.lastQueriedInput = 'test';
    await omniboxComposebox.updateComplete;
    await microtasksFinished();
    const matches =
        [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'test',
          matches,
        }));
    await testProxy.page.$.flushForTesting();
    await microtasksFinished();

    // When the submit button is clicked.
    const composeboxSubmit =
        omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');
    const submitContainer =
        composeboxSubmit!.shadowRoot.querySelector<HTMLElement>(
            '#submitContainer');
    submitContainer!.click();
    await microtasksFinished();

    // Then openAutocompleteMatch is called.
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test(
      'submit button is disabled when file does not support unimodal and input is empty',
      async () => {
        // Given a file that does not support unimodal and empty input.
        omniboxComposebox.searchboxNextEnabled = true;
        omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
        const mockToken = 'mock-file-token';
        const file = new ComposeboxFile(
            mockToken, 'test.png', 'image/png', InputType.kLensImage,
            {supportsUnimodal: false});

        omniboxComposebox.files.set(mockToken, file);
        omniboxComposebox.files = new Map(omniboxComposebox.files);
        omniboxComposebox.input = '';
        await omniboxComposebox.updateComplete;
        await microtasksFinished();

        const composeboxSubmit =
            omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');
        assertTrue(composeboxSubmit!.hasAttribute('disabled'));
      });

  test(
      'submit button click leads to submitQuery called when no match selected',
      async () => {
        omniboxComposebox.searchboxNextEnabled = true;
        omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
        omniboxComposebox.input = 'test query';
        await omniboxComposebox.updateComplete;
        await microtasksFinished();
        omniboxComposebox.selectedMatchIndex = -1;
        await microtasksFinished();

        const composeboxSubmit =
            omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');
        const submitContainer =
            composeboxSubmit!.shadowRoot.querySelector<HTMLElement>(
                '#submitContainer');
        submitContainer!.click();
        await microtasksFinished();

        assertEquals(1, testProxy.handler.getCallCount('submitQuery'));
        const args = testProxy.handler.getArgs('submitQuery')[0];
        assertEquals('test query', args[0]);
      });

  test('submit button is disabled during file upload', async () => {
    omniboxComposebox.searchboxNextEnabled = true;
    omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
    omniboxComposebox.input = 'test';
    await omniboxComposebox.updateComplete;
    await microtasksFinished();
    const testToken = '12345678901234567890123456789012';
    const testFileInfo = {
      fileName: 'test_file.pdf',
      mimeType: 'application/pdf',
      imageDataUrl: null,
      isDeletable: true,
      selectionTime: new Date(),
    };

    testProxy.page.addFileContext(testToken, testFileInfo);
    await testProxy.page.$.flushForTesting();
    await microtasksFinished();

    const composeboxSubmit =
        omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');
    assertTrue(composeboxSubmit!.hasAttribute('disabled'));
  });

  test('clicking submit button after file upload succeeds calls handler', async () => {
    omniboxComposebox.searchboxNextEnabled = true;
    omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
    omniboxComposebox.input = 'test';
    await omniboxComposebox.updateComplete;
    await microtasksFinished();
    const testToken = '12345678901234567890123456789012';
    const testFileInfo = {
      fileName: 'test_file.pdf',
      mimeType: 'application/pdf',
      imageDataUrl: null,
      isDeletable: true,
      selectionTime: new Date(),
    };

    testProxy.page.addFileContext(testToken, testFileInfo);
    await testProxy.page.$.flushForTesting();
    await microtasksFinished();
    testProxy.page.onContextualInputStatusChanged(
        testToken, ContextUploadStatus.kUploadSuccessful, null);
    await testProxy.page.$.flushForTesting();
    await omniboxComposebox.updateComplete;
    await microtasksFinished();

    // When the submit button is clicked.
    const composeboxSubmit =
        omniboxComposebox.shadowRoot.querySelector('cr-composebox-submit');
    assertFalse(composeboxSubmit!.hasAttribute('disabled'));
    const submitContainer =
        composeboxSubmit!.shadowRoot.querySelector<HTMLElement>(
            '#submitContainer');
    submitContainer!.click();
    await microtasksFinished();

    // Then submitQuery is called.
    assertEquals(1, testProxy.handler.getCallCount('submitQuery'));
  });

  test(
      'submit disabled when tool is Deep Search (default entrypoint)',
      async () => {
        omniboxComposebox.searchboxNextEnabled = true;
        omniboxComposebox.searchboxLayoutMode = 'TallBottomContext';
        omniboxComposebox.entrypointName = 'Omnibox';
        await omniboxComposebox.updateComplete;
        await microtasksFinished();
        assertFalse(omniboxComposebox.submitEnabled);
        assertFalse(omniboxComposebox.canSubmitFilesAndInput);

        // Change tool to Deep Search.
        const inputState = {
          allowedModels: [],
          allowedTools: [ToolMode.kDeepSearch],
          allowedInputTypes: [],
          activeModel: 0,
          activeTool: ToolMode.kDeepSearch,
          disabledModels: [],
          disabledTools: [],
          disabledInputTypes: [],
          inputTypeConfigs: [],
          toolConfigs: [],
          modelConfigs: [],
          toolsSectionConfig: null,
          modelSectionConfig: null,
          hintText: '',
          maxInputsByType: {},
          maxTotalInputs: 0,
          isCanvasQuerySubmitted: false,
        };
        testProxy.page.onInputStateChanged(inputState);
        await testProxy.page.$.flushForTesting();
        await omniboxComposebox.updateComplete;
        await microtasksFinished();

        // Submit should still be disabled because entrypoint is not
        // ContextualTasks, and input is empty.
        assertFalse(omniboxComposebox.submitEnabled);
        assertFalse(omniboxComposebox.canSubmitFilesAndInput);

        // Try to submit via Enter key.
        const enterEvent = new KeyboardEvent('keydown', {
          key: 'Enter',
          bubbles: true,
          cancelable: true,
          composed: true,
        });
        omniboxComposebox.$.composebox.dispatchEvent(enterEvent);
        await microtasksFinished();

        assertEquals(0, testProxy.handler.getCallCount('submitQuery'));
      });

  test('Cancel button closes composebox when there is no content', async () => {
    omniboxComposebox.input = '';
    omniboxComposebox.files.clear();
    omniboxComposebox.files = new Map(omniboxComposebox.files);
    await microtasksFinished();
    let closeEventFired = false;
    omniboxComposebox.addEventListener('close-composebox', () => {
      closeEventFired = true;
    });

    const cancelIcon =
        omniboxComposebox.getInputElement()
            .shadowRoot.querySelector<HTMLElement>('#cancelIcon')!;
    cancelIcon.click();
    await microtasksFinished();

    assertTrue(closeEventFired);
    assertEquals(1, testProxy.handler.getCallCount('clearFiles'));
  });

  test('Cancel button clears input text when there is text', async () => {
    omniboxComposebox.input = 'some text';
    await microtasksFinished();
    let closeEventFired = false;
    omniboxComposebox.addEventListener('close-composebox', () => {
      closeEventFired = true;
    });

    const cancelIcon =
        omniboxComposebox.getInputElement()
            .shadowRoot.querySelector<HTMLElement>('#cancelIcon')!;
    cancelIcon.click();
    await microtasksFinished();

    assertEquals('', omniboxComposebox.input);
    assertEquals(1, testProxy.handler.getCallCount('clearFiles'));
    assertFalse(closeEventFired);
  });

  test('Cancel button clears files when there are files', async () => {
    const mockToken = 'mock-file-token';
    const file = new ComposeboxFile(
        mockToken, 'test.png', 'image/png', InputType.kLensImage);
    omniboxComposebox.files.set(mockToken, file);
    omniboxComposebox.files = new Map(omniboxComposebox.files);
    await microtasksFinished();
    let closeEventFired = false;
    omniboxComposebox.addEventListener('close-composebox', () => {
      closeEventFired = true;
    });

    const cancelIcon =
        omniboxComposebox.getInputElement()
            .shadowRoot.querySelector<HTMLElement>('#cancelIcon')!;
    cancelIcon.click();
    await microtasksFinished();

    assertEquals(0, omniboxComposebox.files.size);
    assertEquals(1, testProxy.handler.getCallCount('clearFiles'));
    assertFalse(closeEventFired);
  });

  test('Cancel button resets active tool when in tool mode', async () => {
    const inputState = {
      allowedModels: [],
      allowedTools: [ToolMode.kDeepSearch],
      allowedInputTypes: [],
      activeModel: 0,
      activeTool: ToolMode.kDeepSearch,
      disabledModels: [],
      disabledTools: [],
      disabledInputTypes: [],
      inputTypeConfigs: [],
      toolConfigs: [],
      modelConfigs: [],
      toolsSectionConfig: null,
      modelSectionConfig: null,
      hintText: '',
      maxInputsByType: {},
      maxTotalInputs: 0,
      isCanvasQuerySubmitted: false,
    };
    testProxy.page.onInputStateChanged(inputState);
    await testProxy.page.$.flushForTesting();
    await omniboxComposebox.updateComplete;
    await microtasksFinished();
    let closeEventFired = false;
    omniboxComposebox.addEventListener('close-composebox', () => {
      closeEventFired = true;
    });

    const cancelIcon =
        omniboxComposebox.getInputElement()
            .shadowRoot.querySelector<HTMLElement>('#cancelIcon')!;
    cancelIcon.click();
    await microtasksFinished();

    const activeTool = await testProxy.handler.whenCalled('setActiveToolMode');
    assertEquals(ToolMode.kUnspecified, activeTool);
    assertFalse(closeEventFired);
  });

  test(
      'Voice search components render when showVoiceSearch is true and supported',
      async () => {
        const windowProxy = TestMock.fromClass(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
        windowProxy.setResultMapperFor(
            'matchMedia', (query: string) => window.matchMedia(query));
        WindowProxy.setInstance(windowProxy);
        testProxy.handler.setPromiseResolveFor('getPageClassification', {
          metricSource: 'NTP_OMNIBOX_COMPOSEBOX',
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        omniboxComposebox = document.createElement('cr-omnibox-composebox');
        omniboxComposebox.showVoiceSearch = true;
        document.body.appendChild(omniboxComposebox);
        await omniboxComposebox.updateComplete;

        const voiceSearchOverlay = omniboxComposebox.shadowRoot.querySelector(
            'cr-composebox-voice-search');
        assertTrue(!!voiceSearchOverlay);
        const voiceSearchButton =
            omniboxComposebox.shadowRoot.querySelector('#voiceSearchButton');
        assertTrue(!!voiceSearchButton);
      });

  test(
      'Voice search components do not render when showVoiceSearch is false',
      async () => {
        const windowProxy = TestMock.fromClass(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
        windowProxy.setResultMapperFor(
            'matchMedia', (query: string) => window.matchMedia(query));
        WindowProxy.setInstance(windowProxy);
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        omniboxComposebox = document.createElement('cr-omnibox-composebox');
        omniboxComposebox.showVoiceSearch = false;

        document.body.appendChild(omniboxComposebox);
        await omniboxComposebox.updateComplete;

        const voiceSearchOverlay = omniboxComposebox.shadowRoot.querySelector(
            'cr-composebox-voice-search');
        assertFalse(!!voiceSearchOverlay);
        const voiceSearchButton =
            omniboxComposebox.shadowRoot.querySelector('#voiceSearchButton');
        assertFalse(!!voiceSearchButton);
      });

  test(
      'voice permission changed updates cr-composebox-voice-search class and hides bottomActions',
      async () => {
        const windowProxy = TestMock.fromClass(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
        windowProxy.setResultMapperFor(
            'matchMedia', (query: string) => window.matchMedia(query));
        WindowProxy.setInstance(windowProxy);
        testProxy.handler.setPromiseResolveFor('getPageClassification', {
          metricSource: 'NTP_OMNIBOX_COMPOSEBOX',
        });
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        // Recreate omniboxComposebox so updated loadTimeData and WindowProxy
        // mock take effect.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        omniboxComposebox = document.createElement('cr-omnibox-composebox');
        omniboxComposebox.showVoiceSearch = true;
        document.body.appendChild(omniboxComposebox);
        await omniboxComposebox.updateComplete;

        const voiceSearchOverlay = omniboxComposebox.shadowRoot.querySelector(
            'cr-composebox-voice-search');
        assertTrue(!!voiceSearchOverlay);
        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        voiceSearchOverlay.shadowRoot.appendChild(style);
        const bottomActions =
            voiceSearchOverlay.shadowRoot.querySelector('#bottomActions');
        assertTrue(!!bottomActions);
        // Verify that the bottom actions are initially visible (opacity 1).
        assertEquals('1', window.getComputedStyle(bottomActions).opacity);

        // Simulate voice permission prompt opening.
        omniboxComposebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await voiceSearchOverlay.updateComplete;

        assertTrue(voiceSearchOverlay.classList.contains(
            'embedded-permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(bottomActions).opacity);
      });

  test(
      'voice permission changed updates search-animated-glow class and hides audio-wave',
      async () => {
        const windowProxy = TestMock.fromClass(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
        windowProxy.setResultMapperFor(
            'matchMedia', (query: string) => window.matchMedia(query));
        WindowProxy.setInstance(windowProxy);
        testProxy.handler.setPromiseResolveFor('getPageClassification', {
          metricSource: 'NTP_OMNIBOX_COMPOSEBOX',
        });

        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        omniboxComposebox = document.createElement('cr-omnibox-composebox');
        omniboxComposebox.showVoiceSearch = true;
        document.body.appendChild(omniboxComposebox);
        await omniboxComposebox.updateComplete;

        const glow =
            omniboxComposebox.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!glow);

        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        glow.shadowRoot.appendChild(style);

        // Make sure it is listening so the audio element becomes visible
        // (opacity 1).
        omniboxComposebox.isListening = true;
        await omniboxComposebox.updateComplete;
        await glow.updateComplete;

        assertTrue(glow.isListening, 'glow.isListening should be true');
        assertTrue(
            glow.hasAttribute('is-listening'),
            'glow should have is-listening attribute');

        const audioWave = glow.shadowRoot.querySelector('audio-wave');
        assertTrue(!!audioWave);
        assertEquals('1', window.getComputedStyle(audioWave).opacity);

        // Simulate voice permission prompt opening.
        omniboxComposebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await omniboxComposebox.updateComplete;

        // Verify the class was added and opacity turned to 0.
        assertTrue(
            glow.classList.contains('embedded-permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(audioWave).opacity);
      });

  test(
      'voice permission changed updates search-animated-glow class and hides recording-wave',
      async () => {
        const windowProxy = TestMock.fromClass(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
        windowProxy.setResultMapperFor(
            'matchMedia', (query: string) => window.matchMedia(query));
        WindowProxy.setInstance(windowProxy);
        testProxy.handler.setPromiseResolveFor('getPageClassification', {
          metricSource: 'NTP_OMNIBOX_COMPOSEBOX',
        });

        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        omniboxComposebox = document.createElement('cr-omnibox-composebox');
        omniboxComposebox.showVoiceSearch = true;
        document.body.appendChild(omniboxComposebox);
        await omniboxComposebox.updateComplete;

        const glow =
            omniboxComposebox.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!glow);

        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        glow.shadowRoot.appendChild(style);

        // Make sure it is listening so the audio element becomes visible
        // (opacity 1)
        omniboxComposebox.isListening = true;
        await omniboxComposebox.updateComplete;
        await glow.updateComplete;

        assertTrue(glow.isListening, 'glow.isListening should be true');
        assertTrue(
            glow.hasAttribute('is-listening'),
            'glow should have is-listening attribute');

        const recordingWave = glow.shadowRoot.querySelector('recording-wave');
        assertTrue(!!recordingWave);
        assertEquals('1', window.getComputedStyle(recordingWave).opacity);

        // Simulate voice permission prompt opening.
        omniboxComposebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await omniboxComposebox.updateComplete;

        // Verify the class was added and opacity turned to 0.
        assertTrue(
            glow.classList.contains('embedded-permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(recordingWave).opacity);
      });

  test('suggestion activity link triggers navigation', async () => {
    // Mock results to show the link.
    const matches = [
      createSearchMatchForTesting({
        isNoncannedAimSuggestion: true,
      }),
    ];
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await testProxy.page.$.flushForTesting();
    await microtasksFinished();
    await omniboxComposebox.updateComplete;

    const suggestionActivity =
        omniboxComposebox.shadowRoot.querySelector('#suggestionActivity');
    assertTrue(!!suggestionActivity);
    const localizedLink = suggestionActivity.querySelector('localized-link');
    assertTrue(!!localizedLink);

    const testUrl = 'about:blank?activity';
    // Simulate the event fired by localized-link.
    const anchor = document.createElement('a');
    anchor.href = testUrl;

    let preventDefaultCalled = false;
    const linkClickedEvent = new CustomEvent('link-clicked', {
      detail: {
        event: {
          preventDefault: () => {
            preventDefaultCalled = true;
          },
          currentTarget: anchor,
        },
      },
    });

    localizedLink.dispatchEvent(linkClickedEvent);

    const url = await mockPageHandler.whenCalled('navigateUrl');
    assertEquals(testUrl, url);
    assertTrue(preventDefaultCalled);
  });

  test(
      'suggestion activity link hidden when suggestions are non canned',
      async () => {
        const matches = [
          createSearchMatchForTesting({
            isNoncannedAimSuggestion: false,
          }),
        ];
        testProxy.page.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              matches: matches,
            }));
        await testProxy.page.$.flushForTesting();
        await microtasksFinished();
        await omniboxComposebox.updateComplete;

        const suggestionActivity =
            omniboxComposebox.shadowRoot.querySelector('#suggestionActivity');
        assertFalse(!!suggestionActivity);
      });
});
