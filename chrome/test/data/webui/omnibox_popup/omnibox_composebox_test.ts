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
import type {InputState} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
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
        ...createDefaultInputState(),
        allowedModels: [1],
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

  test('addSearchContext sets input and queries autocomplete', async () => {
    omniboxComposebox.searchboxNextEnabled = true;
    await microtasksFinished();

    const initialCallCount =
        testProxy.handler.getCallCount('queryAutocomplete');

    const context = {
      input: 'hello world',
      files: [],
      attachments: [],
      toolMode: 0,
    };
    omniboxComposebox.addSearchContext(context);
    await microtasksFinished();

    // Check that input and lastQueriedInput are set.
    assertEquals(omniboxComposebox.input, 'hello world');
    assertEquals(omniboxComposebox.lastQueriedInput, 'hello world');
    // Autocomplete should be queried.
    assertEquals(
        initialCallCount + 1,
        testProxy.handler.getCallCount('queryAutocomplete'));
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

  test(
      'Add Attachment with unsupported file type validation error fails',
      async () => {
        const mockToken = 'mock-unsupported-error-token';
        const context = {
          input: '',
          attachments: [{
            fileAttachment: {
              uuid: mockToken,
              name: 'test.txt',
              mimeType: 'text/plain',
              imageDataUrl: null,
              errorType: ContextUploadErrorType
                             .kBrowserProcessingUnsupportedFileTypeError,
            },
          }],
          toolMode: 0,
        };

        omniboxComposebox.i18n = (key: string) => {
          if (key === 'composeFileTypesAllowedError') {
            return 'Unsupported file type error';
          }
          return key;
        };

        omniboxComposebox.addSearchContext(context);
        await microtasksFinished();

        assertFalse(omniboxComposebox.files.has(mockToken));
        assertEquals(
            'Unsupported file type error', omniboxComposebox.errorMessage);
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

  async function dispatchDragAndDropEvent(hostElement: Element, files: File[]) {
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

  function createDefaultInputState(): InputState {
    return {
      allowedModels: [],
      allowedTools: [],
      allowedInputTypes: [],
      activeModel: 0,
      activeTool: 0,
      disabledModels: [],
      disabledTools: [],
      disabledInputTypes: [],
      toolConfigs: [],
      modelConfigs: [],
      inputTypeConfigs: [],
      toolsSectionConfig: null,
      modelSectionConfig: null,
      hintText: '',
      maxInputsByType: {},
      maxTotalInputs: 0,
      isCanvasQuerySubmitted: false,
    };
  }

  suite('DragAndDrop', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        'composeboxContextDragAndDropEnabled': true,
        'composeboxFileMaxCount': 4,
        'composeboxFileMaxSize': 10000000,
        'lensSendRawFileMediaTypesEnabled': false,
      });

      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      omniboxComposebox = document.createElement('cr-omnibox-composebox');
      document.body.appendChild(omniboxComposebox);
      await omniboxComposebox.updateComplete;
    });

    test('sets is-dragging-file attribute on dragenter', async () => {
      const dropZone =
          omniboxComposebox.shadowRoot.querySelector('#composebox');
      assertTrue(!!dropZone);

      assertFalse(omniboxComposebox.hasAttribute('is-dragging-file'));

      dropZone?.dispatchEvent(new DragEvent('dragenter', {
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();

      assertTrue(omniboxComposebox.hasAttribute('is-dragging-file'));
      assertEquals(
          GlowAnimationState.DRAGGING, omniboxComposebox.animationState);
    });

    test('removes is-dragging-file attribute on dragleave', async () => {
      const dropZone =
          omniboxComposebox.shadowRoot.querySelector('#composebox');
      assertTrue(!!dropZone);

      omniboxComposebox.animationState = GlowAnimationState.DRAGGING;
      dropZone?.dispatchEvent(new DragEvent('dragenter', {
        bubbles: true,
        composed: true,
      }));
      dropZone?.dispatchEvent(new DragEvent('dragleave', {
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();

      assertFalse(omniboxComposebox.hasAttribute('is-dragging-file'));
      assertEquals(GlowAnimationState.NONE, omniboxComposebox.animationState);
    });

    test('accepts a dropped file and adds it to the carousel', async () => {
      const sharedToken = '12345678123412341234123456789ABC';
      testProxy.handler.setPromiseResolveFor('addFileContext', sharedToken);

      const file = new File(['content'], 'foo.pdf', {type: 'application/pdf'});
      await dispatchDragAndDropEvent(omniboxComposebox, [file]);

      await testProxy.handler.whenCalled('addFileContext');
      assertEquals(1, testProxy.handler.getCallCount('addFileContext'));
      assertFalse(omniboxComposebox.hasAttribute('is-dragging-file'));

      // Simulate backend callback to render file
      const testFileInfo = {
        fileName: 'foo.pdf',
        imageDataUrl: null,
        mimeType: 'application/pdf',
        isDeletable: true,
        selectionTime: new Date(),
      };
      testProxy.page.addFileContext(sharedToken, testFileInfo);
      await testProxy.page.$.flushForTesting();
      await microtasksFinished();
      await omniboxComposebox.updateComplete;

      const carousel: ComposeboxFileCarouselElement|null =
          omniboxComposebox.shadowRoot.querySelector(
              'cr-composebox-file-carousel');

      assertTrue(!!carousel, 'Carousel should render');

      const carouselFiles = carousel.files;
      assertEquals(1, carouselFiles.length);
      assertEquals('foo.pdf', carouselFiles[0]!.name);
    });

    test('does not accept a dropped file that is too large', async () => {
      const sampleFileMaxSize = 10;  // bytes
      loadTimeData.overrideValues({'composeboxFileMaxSize': sampleFileMaxSize});

      // Recreate to pick up new limit
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      omniboxComposebox = document.createElement('cr-omnibox-composebox');
      document.body.appendChild(omniboxComposebox);
      await omniboxComposebox.updateComplete;

      const blob = new Blob([new Uint8Array(sampleFileMaxSize + 1)]);
      const testFile =
          new File([blob], 'largefile.pdf', {type: 'application/pdf'});
      await dispatchDragAndDropEvent(omniboxComposebox, [testFile]);

      assertEquals(0, testProxy.handler.getCallCount('addFileContext'));
    });

    test('does not accept wrong file type', async () => {
      const testFile =
          new File(['foo'], 'malware.exe', {type: 'application/x-msdownload'});
      await dispatchDragAndDropEvent(omniboxComposebox, [testFile]);

      const expectedCallCount =
          loadTimeData.getBoolean('lensSendRawFileMediaTypesEnabled') ? 1 : 0;
      assertEquals(
          expectedCallCount, testProxy.handler.getCallCount('addFileContext'));
    });

    test('does not accept multiple files if only one allowed', async () => {
      loadTimeData.overrideValues({'composeboxFileMaxCount': 1});

      // Recreate to pick up new limit
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      omniboxComposebox = document.createElement('cr-omnibox-composebox');
      document.body.appendChild(omniboxComposebox);
      await omniboxComposebox.updateComplete;

      const sharedToken = '12345678123412341234123456789ABC';
      testProxy.handler.setPromiseResolveFor('addFileContext', sharedToken);

      const file1 = new File(['a'], 'a.pdf', {type: 'application/pdf'});
      const file2 = new File(['b'], 'b.pdf', {type: 'application/pdf'});

      await dispatchDragAndDropEvent(omniboxComposebox, [file1, file2]);

      await testProxy.handler.whenCalled('addFileContext');

      assertEquals(1, testProxy.handler.getCallCount('addFileContext'));

      // Simulate backend callback
      const testFileInfo = {
        fileName: 'a.pdf',
        imageDataUrl: null,
        mimeType: 'application/pdf',
        isDeletable: true,
        selectionTime: new Date(),
      };
      testProxy.page.addFileContext(sharedToken, testFileInfo);
      await testProxy.page.$.flushForTesting();
      await microtasksFinished();
      await omniboxComposebox.updateComplete;

      const carousel: ComposeboxFileCarouselElement|null =
          omniboxComposebox.shadowRoot.querySelector(
              'cr-composebox-file-carousel');

      assertTrue(!!carousel, 'Carousel should render');

      const carouselFiles = carousel.files;
      assertEquals(1, carouselFiles.length);
      assertEquals('a.pdf', carouselFiles[0]?.name);
    });

    test('Deep Search mode blocks all uploads', async () => {
      const inputState = createDefaultInputState();
      inputState.activeTool = ToolMode.kDeepSearch;
      testProxy.page.onInputStateChanged(inputState);
      await testProxy.page.$.flushForTesting();
      await microtasksFinished();

      const imageFile = new File([''], 'test.png', {type: 'image/png'});
      await dispatchDragAndDropEvent(omniboxComposebox, [imageFile]);

      assertEquals(0, testProxy.handler.getCallCount('addFileContext'));
    });

    test('Image Gen mode allows images but blocks PDFs', async () => {
      loadTimeData.overrideValues({
        'composeboxImageFileTypes': 'image/*',
        'composeboxAttachmentFileTypes': 'application/pdf',
      });

      const inputState = createDefaultInputState();
      inputState.activeTool = ToolMode.kImageGen;
      inputState.disabledInputTypes = [InputType.kLensFile];
      inputState.maxInputsByType =
          {[InputType.kLensImage]: 1, [InputType.kLensFile]: 1};
      inputState.maxTotalInputs = 2;
      testProxy.page.onInputStateChanged(inputState);
      await testProxy.page.$.flushForTesting();
      await microtasksFinished();

      // 1. Drop a PDF (should be blocked).
      const pdfFile = new File([''], 'test.pdf', {type: 'application/pdf'});
      await dispatchDragAndDropEvent(omniboxComposebox, [pdfFile]);
      assertEquals(0, testProxy.handler.getCallCount('addFileContext'));

      // 2. Drop an image (should be allowed).
      testProxy.handler.setPromiseResolveFor(
          'addFileContext', 'ABCDEF00000000000000000000000000');
      const imageFile = new File(['content'], 'test.png', {type: 'image/png'});
      await dispatchDragAndDropEvent(omniboxComposebox, [imageFile]);
      await testProxy.handler.whenCalled('addFileContext');
      assertEquals(1, testProxy.handler.getCallCount('addFileContext'));
    });

    test('Canvas mode allows both images and PDFs', async () => {
      loadTimeData.overrideValues({
        'composeboxImageFileTypes': 'image/*',
        'composeboxAttachmentFileTypes': 'application/pdf',
      });

      const inputState = createDefaultInputState();
      inputState.activeTool = ToolMode.kCanvas;
      inputState.maxInputsByType =
          {[InputType.kLensImage]: 1, [InputType.kLensFile]: 1};
      inputState.maxTotalInputs = 2;
      testProxy.page.onInputStateChanged(inputState);
      await testProxy.page.$.flushForTesting();
      await microtasksFinished();

      // 1. Drop an image.
      testProxy.handler.setPromiseResolveFor(
          'addFileContext', 'ABCDEF00000000000000000000000000');
      const imageFile = new File(['content'], 'test.png', {type: 'image/png'});
      await dispatchDragAndDropEvent(omniboxComposebox, [imageFile]);
      await testProxy.handler.whenCalled('addFileContext');
      assertEquals(1, testProxy.handler.getCallCount('addFileContext'));

      // 2. Drop a PDF.
      testProxy.handler.reset();
      testProxy.handler.setPromiseResolveFor(
          'addFileContext', 'ABCDEF11111111111111111111111111');
      const pdfFile =
          new File(['content'], 'test.pdf', {type: 'application/pdf'});
      await dispatchDragAndDropEvent(omniboxComposebox, [pdfFile]);
      await testProxy.handler.whenCalled('addFileContext');
      assertEquals(1, testProxy.handler.getCallCount('addFileContext'));
    });
  });

  async function waitForAddFileCallCount(count: number) {
    let iterations = 0;
    while (testProxy.handler.getCallCount('addFileContext') < count) {
      if (iterations > 100) {
        throw new Error(
            `Timeout waiting for addFileContext to be called ${count} times`);
      }
      await microtasksFinished();
      iterations++;
    }
  }

  suite('Paste', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        'composeboxContextDragAndDropEnabled': true,
      });

      // Recreate to pick up drag and drop enabled binding
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      omniboxComposebox = document.createElement('cr-omnibox-composebox');
      document.body.appendChild(omniboxComposebox);
      await omniboxComposebox.updateComplete;
    });

    test('pasting valid files calls addFileContext', async () => {
      loadTimeData.overrideValues({'composeboxFileMaxCount': 5});
      testProxy.handler.setPromiseResolveFor(
          'addFileContext', 'TOKEN123456781234123412345678ABCD');

      const pngFile = new File(['foo'], 'foo.png', {type: 'image/png'});
      const pdfFile = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
      const dataTransfer = new DataTransfer();
      dataTransfer.items.add(pngFile);
      dataTransfer.items.add(pdfFile);
      const pasteEvent = new ClipboardEvent('paste', {
        clipboardData: dataTransfer,
        bubbles: true,
        cancelable: true,
        composed: true,
      });

      omniboxComposebox.getInputElement().inputElement.dispatchEvent(
          pasteEvent);
      await waitForAddFileCallCount(2);

      assertEquals(2, testProxy.handler.getCallCount('addFileContext'));
      const argsArray = testProxy.handler.getArgs('addFileContext');
      assertEquals('foo.png', argsArray[0][0].fileName);
      assertEquals('foo.pdf', argsArray[1][0].fileName);

      assertTrue(pasteEvent.defaultPrevented);
    });

    test('pasting too many files prevents paste', async () => {
      const inputState = createDefaultInputState();
      inputState.maxInputsByType =
          {[InputType.kLensImage]: 1, [InputType.kLensFile]: 1};
      inputState.maxTotalInputs = 2;
      testProxy.page.onInputStateChanged(inputState);
      await testProxy.page.$.flushForTesting();
      await microtasksFinished();

      testProxy.handler.setPromiseResolveFor(
          'addFileContext', 'TOKEN123456781234123412345678ABCD');

      const pngFile1 = new File(['foo'], 'foo1.png', {type: 'image/png'});
      const pngFile2 = new File(['foo'], 'foo2.png', {type: 'image/png'});
      const dataTransfer = new DataTransfer();
      dataTransfer.items.add(pngFile1);
      dataTransfer.items.add(pngFile2);
      const pasteEvent = new ClipboardEvent('paste', {
        clipboardData: dataTransfer,
        bubbles: true,
        cancelable: true,
        composed: true,
      });

      omniboxComposebox.getInputElement().inputElement.dispatchEvent(
          pasteEvent);
      await waitForAddFileCallCount(1);

      assertEquals(1, testProxy.handler.getCallCount('addFileContext'));
      assertTrue(pasteEvent.defaultPrevented);

      // Check whether the right error would show up.
      assertTrue(omniboxComposebox.errorMessage.length > 0);
    });

    test('pasting unsupported files fires validation error', async () => {
      const txtFile = new File(['foo'], 'foo.txt', {type: 'text/plain'});
      const dataTransfer = new DataTransfer();
      dataTransfer.items.add(txtFile);
      const pasteEvent = new ClipboardEvent('paste', {
        clipboardData: dataTransfer,
        bubbles: true,
        cancelable: true,
        composed: true,
      });

      omniboxComposebox.getInputElement().inputElement.dispatchEvent(
          pasteEvent);
      await microtasksFinished();

      assertTrue(omniboxComposebox.errorMessage.length > 0);
      assertEquals(0, testProxy.handler.getCallCount('addFileContext'));
      assertTrue(pasteEvent.defaultPrevented);
    });

    test(
        'pasting only text does not call addFiles or prevent default',
        async () => {
          const dataTransfer = new DataTransfer();
          dataTransfer.setData('text/plain', 'hello');
          const pasteEvent = new ClipboardEvent('paste', {
            clipboardData: dataTransfer,
            bubbles: true,
            cancelable: true,
            composed: true,
          });

          omniboxComposebox.getInputElement().inputElement.dispatchEvent(
              pasteEvent);
          await microtasksFinished();

          assertEquals(0, testProxy.handler.getCallCount('addFileContext'));
          assertFalse(pasteEvent.defaultPrevented);
        });

    test('pasting mixed files is processed correctly', async () => {
      const inputState = createDefaultInputState();
      inputState.maxInputsByType =
          {[InputType.kLensImage]: 2, [InputType.kLensFile]: 2};
      inputState.maxTotalInputs = 5;
      testProxy.page.onInputStateChanged(inputState);
      await testProxy.page.$.flushForTesting();
      await microtasksFinished();

      let i = 0;
      testProxy.handler.setResultMapperFor('addFileContext', () => {
        i += 1;
        return Promise.resolve(`MOCKTOKEN00000000000000000000000${i}`);
      });

      const pngFile = new File(['foo'], 'foo.png', {type: 'image/png'});
      const pdfFile = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});

      const dataTransfer = new DataTransfer();
      dataTransfer.items.add(pngFile);
      dataTransfer.items.add(pdfFile);
      const pasteEvent = new ClipboardEvent('paste', {
        clipboardData: dataTransfer,
        bubbles: true,
        cancelable: true,
        composed: true,
      });

      omniboxComposebox.getInputElement().inputElement.dispatchEvent(
          pasteEvent);
      await waitForAddFileCallCount(2);
      await microtasksFinished();
      await omniboxComposebox.updateComplete;

      const carousel: ComposeboxFileCarouselElement|null =
          omniboxComposebox.shadowRoot.querySelector(
              'cr-composebox-file-carousel');
      assertTrue(!!carousel);
      const files = carousel.files;
      assertEquals(2, files.length);

      const imageFile =
          files.find((f: ComposeboxFile) => f.type.includes('image'));
      const pdfFileInCarousel =
          files.find((f: ComposeboxFile) => f.type.includes('pdf'));

      assertTrue(!!imageFile);
      assertTrue(!!pdfFileInCarousel);

      assertTrue(!!imageFile.objectUrl);
      assertEquals(pdfFileInCarousel.objectUrl, null);
    });
  });

  suite('SmartCompose', () => {
    setup(async () => {
      loadTimeData.overrideValues({composeboxSmartComposeEnabled: true});
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      omniboxComposebox = document.createElement('cr-omnibox-composebox');
      document.body.appendChild(omniboxComposebox);
      await omniboxComposebox.updateComplete;
    });

    test('Smart Compose hint is hidden during backspacing', async () => {
      const inputElement = omniboxComposebox.getInputElement();
      const input = inputElement.inputElement as HTMLTextAreaElement;

      // Provide an input and a hint.
      input.value = 'tes';
      input.dispatchEvent(new Event('input'));
      const hint = 't';

      omniboxComposebox.haveReceivedSynchronousAutocompleteResponse = true;
      testProxy.page.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            input: 'tes',
            smartComposeInlineHint: hint,
          }));
      await testProxy.page.$.flushForTesting();
      await microtasksFinished();

      // Verify hint is visible.
      assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));

      // Simulate backspace.
      input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Backspace'}));
      await microtasksFinished();

      // Verify hint is hidden.
      assertFalse(!!inputElement.shadowRoot.querySelector('#smartCompose'));

      // Simulate typing a character.
      input.dispatchEvent(new KeyboardEvent('keydown', {key: 'a'}));
      await microtasksFinished();

      // Verify hint is visible again.
      assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));
    });

    test(
        'Smart Compose hint is hidden when it wraps in the middle of a word',
        async () => {
          const inputElement = omniboxComposebox.getInputElement();
          const input = inputElement.inputElement as HTMLTextAreaElement;

          // Mock Canvas measureText and clientWidth.
          const originalMeasureText =
              CanvasRenderingContext2D.prototype.measureText;
          try {
            CanvasRenderingContext2D.prototype.measureText = function(
                text: string) {
              if (text.includes('wrap')) {
                return {width: 150} as TextMetrics;
              }
              return {width: 50} as TextMetrics;
            };
            Object.defineProperty(
                input, 'clientWidth', {configurable: true, get: () => 100});

            // Provide an input ending with a non-space character and a hint.
            input.value = 'tes.';
            input.dispatchEvent(new Event('input'));
            const hint = 'wrap';  // This will trigger width = 150

            omniboxComposebox.haveReceivedSynchronousAutocompleteResponse =
                true;
            testProxy.page.autocompleteResultChanged(
                createAutocompleteResultForTesting({
                  input: 'tes.',
                  smartComposeInlineHint: hint,
                }));
            await testProxy.page.$.flushForTesting();
            await microtasksFinished();

            // Trigger re-evaluation by requesting update.
            inputElement.requestUpdate();
            await microtasksFinished();

            // Verify hint is hidden.
            assertFalse(
                !!inputElement.shadowRoot.querySelector('#smartCompose'));
          } finally {
            // Restore mock.
            CanvasRenderingContext2D.prototype.measureText =
                originalMeasureText;
          }
        });

    test(
        'Smart Compose hint is NOT hidden when only full hint wraps but first word fits',
        async () => {
          const inputElement = omniboxComposebox.getInputElement();
          const input = inputElement.inputElement as HTMLTextAreaElement;

          // Mock Canvas measureText and clientWidth.
          const originalMeasureText =
              CanvasRenderingContext2D.prototype.measureText;
          try {
            CanvasRenderingContext2D.prototype.measureText = function(
                text: string) {
              if (text.includes('wraps')) {
                return {width: 150} as TextMetrics;
              }
              return {width: 50} as TextMetrics;
            };
            Object.defineProperty(
                input, 'clientWidth', {configurable: true, get: () => 100});

            // Provide an input ending with a non-space character and a hint.
            input.value = 'tes.';
            input.dispatchEvent(new Event('input'));
            const hint = 'fits wraps';

            omniboxComposebox.haveReceivedSynchronousAutocompleteResponse =
                true;
            testProxy.page.autocompleteResultChanged(
                createAutocompleteResultForTesting({
                  input: 'tes.',
                  smartComposeInlineHint: hint,
                }));
            await testProxy.page.$.flushForTesting();
            await microtasksFinished();

            // Trigger re-evaluation by requesting update.
            inputElement.requestUpdate();
            await microtasksFinished();

            // Verify hint is visible.
            assertTrue(
                !!inputElement.shadowRoot.querySelector('#smartCompose'));
          } finally {
            // Restore mock.
            CanvasRenderingContext2D.prototype.measureText =
                originalMeasureText;
          }
        });

    test(
        'Smart Compose hint is hidden when cursor is not at the end',
        async () => {
          const inputElement = omniboxComposebox.getInputElement();
          const input = inputElement.inputElement as HTMLTextAreaElement;

          // Provide an input and a hint.
          input.value = 'test';
          input.dispatchEvent(new Event('input'));
          const hint = 'a';

          omniboxComposebox.haveReceivedSynchronousAutocompleteResponse = true;
          testProxy.page.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                input: 'test',
                smartComposeInlineHint: hint,
              }));
          await testProxy.page.$.flushForTesting();
          await microtasksFinished();

          // Verify hint is visible initially.
          assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));

          // Move cursor to the middle.
          input.selectionStart = 2;
          input.selectionEnd = 2;

          // Trigger re-evaluation.
          inputElement.requestUpdate();
          await microtasksFinished();

          // Verify hint is hidden.
          assertFalse(!!inputElement.shadowRoot.querySelector('#smartCompose'));
        });
  });
});
