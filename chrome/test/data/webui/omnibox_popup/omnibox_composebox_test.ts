// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import {SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxComposeboxElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {ComposeboxProxyImpl} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
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

  setup(async () => {
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
});
