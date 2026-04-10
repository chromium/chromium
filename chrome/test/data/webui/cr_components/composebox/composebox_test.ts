// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './composebox_test_utils.js';

suite('ComposeboxTest', () => {
  let composebox: ComposeboxElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.resetForTesting({
      composeboxShowImageSuggest: false,
      composeboxSmartComposeEnabled: false,
      composeboxShowContextMenuDescription: false,
      composeboxShowZps: true,
      composeboxContextDragAndDropEnabled: false,
      composeboxSource: 'NTP',
      composeboxFileMaxCount: 1,
      composeboxFileMaxSize: 1024,
      composeboxAttachmentFileTypes: '.pdf',
      composeboxImageFileTypes: 'image/png',
      lensSendRawFileMediaTypesEnabled: false,
      composeDeepSearchPlaceholder: 'Deep Search',
      composeCreateImagePlaceholder: 'Create Image',
      searchboxComposePlaceholder: 'Compose',
      composeboxShowContextMenu: false,
      composeboxShowTypedSuggest: false,
      composeboxCancelButtonTitleInput: 'Cancel input',
      composeboxCancelButtonTitle: 'Cancel',
      voiceSearchButtonLabel: 'Voice search',
      lensSearchButtonLabel: 'Lens search',
      suggestionActivityLink: '<a>Activity</a>',
      composeboxSubmitButtonTitle: 'Submit',
      composeboxSmartComposeTabTitle: 'Tab',
      voiceListening: 'Listening',
      voiceDetails: 'Details',
      voiceClose: 'Close',
      dismissButton: 'Dismiss',
      composeboxDragAndDropHint: 'Hint',
      removeSuggestion: 'Remove',
    });

    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));

    installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);

    searchboxCallbackRouterRemote =
        ComposeboxProxyImpl.getInstance()
            .searchboxCallbackRouter.$.bindNewPipeAndPassRemote();

    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await composebox.updateComplete;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('suggestion activity link triggers navigation', async () => {
    composebox.suggestionActivityEnabled = true;

    // Mock results to show the link.
    const matches = [
      createSearchMatchForTesting({
        isNoncannedAimSuggestion: true,
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const suggestionActivity = composebox.shadowRoot.querySelector('#suggestionActivity');
    assertTrue(!!suggestionActivity);
    const localizedLink = suggestionActivity.querySelector('localized-link');
    assertTrue(!!localizedLink);

    const testUrl = 'https://google.com/activity';
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

    const url = await handler.whenCalled('navigateUrl');
    assertEquals(testUrl, url);
    assertTrue(preventDefaultCalled);
  });

  test('suggestion activity link hidden when suggestions are non canned', async () => {
    composebox.suggestionActivityEnabled = true;

    const matches = [
      createSearchMatchForTesting({
        isNoncannedAimSuggestion: false,
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const suggestionActivity = composebox.shadowRoot.querySelector('#suggestionActivity');
    assertFalse(!!suggestionActivity);
  });

  test(
      'Shift+Enter allows inserting a newline when input is focused and not empty',
      async () => {
        const composeboxDiv =
            composebox.shadowRoot.querySelector('#composebox');
        assertTrue(!!composeboxDiv);

        composebox.input = 'Some text';
        await composebox.updateComplete;

        const inputElement = composebox.getInputElement();
        inputElement.inputElement.focus();

        let preventDefaultCalled = false;
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        Object.defineProperty(event, 'preventDefault', {
          value: () => {
            preventDefaultCalled = true;
          },
        });

        assertEquals(composebox.getActiveElement(), inputElement);

        composeboxDiv.dispatchEvent(event);

        assertFalse(
            preventDefaultCalled, 'preventDefault should not be called');
      });

  test(
      'Enter prevents inserting a newline and attempts to submit query when focus is not in dropdown',
      () => {
        const composeboxDiv =
            composebox.shadowRoot.querySelector('#composebox');
        assertTrue(!!composeboxDiv);

        let preventDefaultCalled = false;
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: false,
          bubbles: true,
          cancelable: true,
        });

        Object.defineProperty(event, 'preventDefault', {
          value: () => {
            preventDefaultCalled = true;
          },
        });

        assertFalse(
            composebox.getActiveElement() === composebox.getDropdownElement());

        composeboxDiv.dispatchEvent(event);

        assertTrue(preventDefaultCalled, 'preventDefault should be called');
      });

  test(
      'Shift+Enter submits dropdown selection when focus is in dropdown',
      () => {
        const composeboxDiv =
            composebox.shadowRoot.querySelector('#composebox');
        assertTrue(!!composeboxDiv);

        let preventDefaultCalled = false;
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        Object.defineProperty(event, 'preventDefault', {
          value: () => {
            preventDefaultCalled = true;
          },
        });

        const originalGetActiveElement = composebox.getActiveElement;
        composebox.getActiveElement = () => composebox.getDropdownElement();

        composeboxDiv.dispatchEvent(event);

        assertTrue(preventDefaultCalled, 'preventDefault should be called');

        composebox.getActiveElement = originalGetActiveElement;
      });
});
