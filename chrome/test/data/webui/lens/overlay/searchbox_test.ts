// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/lens_overlay_app.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens-overlay/lens_overlay_app.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('Searchbox', () => {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let lensOverlayElement: LensOverlayAppElement;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      'autoFocusSearchbox': true,
      'enablePrivacyNotice': false,
      'enableOverlayContextualSearchbox': true,
      'enableGhostLoader': true,
    });

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    await waitAfterNextRender(lensOverlayElement);

    testBrowserProxy.page.shouldShowContextualSearchBox(true);
    await waitAfterNextRender(lensOverlayElement);
  });

  test('SearchboxIsFocusedInitially', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    let focusInputCalled = false;
    lensOverlayElement.$.searchbox.focusInput = () => {
      focusInputCalled = true;
    };

    // Simulate the initial flash animation ending and screenshot rendering.
    // Dispatch from a child so it bubbles up to the app-container.
    lensOverlayElement.$.backgroundScrim.dispatchEvent(new CustomEvent('initial-flash-animation-end', {
      bubbles: true,
      composed: true,
    }));
    lensOverlayElement.$.backgroundScrim.dispatchEvent(new CustomEvent('screenshot-rendered', {
      bubbles: true,
      composed: true,
      detail: {isSidePanelOpen: false},
    }));
    await waitAfterNextRender(lensOverlayElement);

    assertTrue(focusInputCalled);
  });

  test('SearchBoxHidesWhenSidePanelOpens', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    testBrowserProxy.page.notifyResultsPanelOpened();
    await waitAfterNextRender(lensOverlayElement);

    assertFalse(isVisible(lensOverlayElement.$.searchbox));
  });

  test('Escape on non-empty input suppresses ghost loader', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    // Simulate searchbox being focused and an autocomplete request being
    // started with non-empty input.
    lensOverlayElement.setSearchboxFocusForTesting(true);
    document.dispatchEvent(new CustomEvent('query-autocomplete', {
      bubbles: true,
      cancelable: true,
      detail: {inputValue: 'hello'},
    }));
    await waitAfterNextRender(lensOverlayElement);
    // Ghost loader should not be visible if the input is not empty.
    assertFalse(isVisible(lensOverlayElement.$.searchboxGhostLoader));

    // Simulate escape being pressed from the searchbox with non-empty input.
    const escapeEvent = new CustomEvent('escape-searchbox', {
      bubbles: true,
      composed: true,
      detail: {
        event: {type: 'keydown', key: 'Escape', preventDefault: () => {}},
        emptyInput: false,
      },
    });
    lensOverlayElement.handleEscapeSearchboxForTesting(escapeEvent);
    await waitAfterNextRender(lensOverlayElement);

    // Simulate the searchbox clearing the input (which would normally trigger
    // zero suggest and show the ghost loader, but it should be suppressed).
    document.dispatchEvent(new CustomEvent('query-autocomplete', {
      bubbles: true,
      cancelable: true,
      detail: {inputValue: ''},
    }));
    await waitAfterNextRender(lensOverlayElement);

    // Ghost loader should stay hidden when escape is pressed because it was
    // suppressed.
    assertFalse(isVisible(lensOverlayElement.$.searchboxGhostLoader));
  });

  test('Escape on empty input blurs searchbox', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    lensOverlayElement.setSearchboxFocusForTesting(true);
    await waitAfterNextRender(lensOverlayElement);
    assertTrue(lensOverlayElement.isSearchboxFocused);

    let preventDefaultCalled = false;
    let blurCalled = false;
    lensOverlayElement.$.searchbox.blur = () => {
      blurCalled = true;
    };

    // Simulate escape being pressed from the searchbox with empty input.
    const escapeEvent = new CustomEvent('escape-searchbox', {
      bubbles: true,
      composed: true,
      detail: {
        event: {
          type: 'keydown',
          key: 'Escape',
          preventDefault: () => {
            preventDefaultCalled = true;
          },
        },
        emptyInput: true,
      },
    });
    lensOverlayElement.handleEscapeSearchboxForTesting(escapeEvent);
    await waitAfterNextRender(lensOverlayElement);

    // Focus should have been removed from the searchbox.
    assertTrue(blurCalled);
    assertTrue(preventDefaultCalled);
  });

  test('Ghost loader is shown when input is empty', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    // Simulate searchbox being focused and the autocomplete request being
    // started with empty input.
    lensOverlayElement.setSearchboxFocusForTesting(true);
    document.dispatchEvent(new CustomEvent('query-autocomplete', {
      bubbles: true,
      cancelable: true,
      detail: {inputValue: ''},
    }));
    await waitAfterNextRender(lensOverlayElement);

    // Ghost loader should be visible since autocomplete started with empty
    // input.
    assertTrue(isVisible(lensOverlayElement.$.searchboxGhostLoader));
  });

  test('Searchbox focus is maintained when focus moves to ghost loader', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    lensOverlayElement.setSearchboxFocusForTesting(true);
    await waitAfterNextRender(lensOverlayElement);
    assertTrue(lensOverlayElement.isSearchboxFocused);

    // Simulate focus moving to the ghost loader
    const focusoutEvent = new FocusEvent('focusout', {
      relatedTarget: lensOverlayElement.$.searchboxGhostLoader,
    });
    lensOverlayElement.$.searchboxContainer.dispatchEvent(focusoutEvent);
    await waitAfterNextRender(lensOverlayElement);

    // Searchbox focus should be maintained
    assertTrue(lensOverlayElement.isSearchboxFocused);
  });

  test('Searchbox is blurred when focus moves outside', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    lensOverlayElement.setSearchboxFocusForTesting(true);
    await waitAfterNextRender(lensOverlayElement);
    assertTrue(lensOverlayElement.isSearchboxFocused);

    // Simulate focus moving outside the container
    const focusoutEvent = new FocusEvent('focusout', {
      relatedTarget: document.body,
    });
    lensOverlayElement.$.searchboxContainer.dispatchEvent(focusoutEvent);
    await waitAfterNextRender(lensOverlayElement);

    // Searchbox focus should be lost
    assertFalse(lensOverlayElement.isSearchboxFocused);
  });

  test('SearchboxDropdownDoesNotShowWhenNotFocused', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    let queryAutocompleteCalled = false;
    lensOverlayElement.$.searchbox.queryInputAutocomplete = () => {
      queryAutocompleteCalled = true;
    };

    // Ensure the searchbox is not focused.
    lensOverlayElement.setSearchboxFocusForTesting(false);

    // Simulate the backend handshake completing.
    testBrowserProxy.page.notifyHandshakeComplete();
    await waitAfterNextRender(lensOverlayElement);

    // Dropdown should not show (queryInputAutocomplete should not be called).
    assertFalse(queryAutocompleteCalled);
  });

  test('SearchboxDropdownShowsWhenFocused', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    let queryAutocompleteCalled = false;
    lensOverlayElement.$.searchbox.queryInputAutocomplete = () => {
      queryAutocompleteCalled = true;
    };

    // Simulate the searchbox being focused.
    lensOverlayElement.$.searchbox.dispatchEvent(new CustomEvent('focusin', {
      bubbles: true,
      composed: true,
    }));
    await waitAfterNextRender(lensOverlayElement);

    // Simulate the backend handshake completing.
    testBrowserProxy.page.notifyHandshakeComplete();
    await waitAfterNextRender(lensOverlayElement);

    // Dropdown should show (queryInputAutocomplete should be called).
    assertTrue(queryAutocompleteCalled);
  });
});
