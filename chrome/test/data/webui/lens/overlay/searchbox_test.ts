// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/lens_overlay_app.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens-overlay/lens_overlay_app.js';
import type {LensSearchboxElement} from 'chrome-untrusted://lens/lens/shared/searchbox/lens_searchbox.js';
import {SearchboxBrowserProxy} from 'chrome-untrusted://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome-untrusted://resources/js/util.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';
import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('Searchbox', () => {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let testSearchboxProxy: TestSearchboxBrowserProxy;
  let lensOverlayElement: LensOverlayAppElement;
  let lensSearchbox: LensSearchboxElement;

  async function areMatchesShowing(): Promise<boolean> {
    await testSearchboxProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    return window.getComputedStyle(lensSearchbox.getDropdownElement())
               .display !== 'none';
  }

  setup(async () => {
    testSearchboxProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testSearchboxProxy);

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

    lensSearchbox = lensOverlayElement.$.searchbox;
    await microtasksFinished();
  });

  test('Searchbox is focused initially', async () => {
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

    assertTrue(focusInputCalled, 'focus input should be called');
  });

  test('Searchbox hides when side panel opens', async () => {
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
    assertTrue(
        isVisible(lensOverlayElement.$.searchbox),
        'Searchbox should be visible');

    lensOverlayElement.setSearchboxFocusForTesting(true);
    await waitAfterNextRender(lensOverlayElement);
    assertTrue(
        lensOverlayElement.isSearchboxFocused,
        'Searchbox should be focused after setting focus');

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
    assertTrue(blurCalled, 'Searchbox should have been blurred');
    assertTrue(preventDefaultCalled, 'Default should have been prevented');
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
    assertTrue(
        isVisible(lensOverlayElement.$.searchboxGhostLoader),
        'Ghost loader should be visible when' +
            'autocomplete starts with empty input');
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

  //============================================================================
  // Test Thumbnails
  //============================================================================
  test('thumbnail appears on page call from browser', async () => {
    assertNull(
        lensSearchbox.shadowRoot.querySelector('cr-searchbox-thumbnail'),
        'thumbnail should not exist');
    testSearchboxProxy.callbackRouterRemote.setThumbnail(
        'foo.png', /*isDeletable=*/ true);
    await testSearchboxProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    const thumbnailContainer =
        lensSearchbox.shadowRoot.querySelector('cr-searchbox-thumbnail');
    assertTrue(thumbnailContainer !== null, 'thumbnail should exist');
    assertTrue(
        isVisible(thumbnailContainer), 'thumbnail container should be visible');
  });

  test('thumbnail clicked deletion', async () => {
    testSearchboxProxy.callbackRouterRemote.setThumbnail(
        'foo.png', /*isDeletable=*/ true);
    await testSearchboxProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    const thumbnail =
        lensSearchbox.shadowRoot.querySelector('cr-searchbox-thumbnail');
    assertTrue(!!thumbnail, 'thumbnail should exist');
    const thumbnailRemoveButton =
        thumbnail.shadowRoot.querySelector<HTMLElement>('#remove');
    assertTrue(!!thumbnailRemoveButton, 'thumbnail delete button should exist');
    // Thumbnail remove button click should remove thumbnail, focus input,
    // and notify browser.
    thumbnailRemoveButton.click();
    await microtasksFinished();
    const thumbnailContainer =
        lensSearchbox.shadowRoot.querySelector<HTMLElement>(
            'cr-searchbox-thumbnail');
    assertNull(thumbnailContainer, 'thumbnail should not exist');
    assertEquals(
        lensSearchbox.$.input.inputElement, getDeepActiveElement(),
        'input should be focused');
    await testSearchboxProxy.handler.whenCalled('onThumbnailRemoved');
    assertEquals(
        1, testSearchboxProxy.handler.getCallCount('onThumbnailRemoved'),
        'thumbnail should be removed');
    // When thumbnail is removed, autocomplete should be re-queried
    const args =
        await testSearchboxProxy.handler.whenCalled('stopAutocomplete');
    assertTrue(args.clearResult, 'results should be cleared');
    assertEquals(
        1, testSearchboxProxy.handler.getCallCount('queryAutocomplete'),
        'autocomplete should be queried');
  });

  test('thumbnail keyboard deletion', async () => {
    testSearchboxProxy.callbackRouterRemote.setThumbnail(
        'foo.png', /*isDeletable=*/ true);
    await testSearchboxProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    const thumbnail =
        lensSearchbox.shadowRoot.querySelector('cr-searchbox-thumbnail');
    assertTrue(thumbnail !== null, 'thumbnail should exist');
    lensSearchbox.$.input.focus();
    lensSearchbox.$.inputWrapper.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Backspace',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    // First backspace should focus the thumbnail
    assertEquals(
        thumbnail, getDeepActiveElement(), 'thumbnail should be focused');

    // When thumbnail is focused, a backspace should delete the thumbnail,
    // focus input, and notify browser.
    lensSearchbox.$.inputWrapper.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Backspace',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    const thumbnailContainer =
        lensSearchbox.shadowRoot.querySelector<HTMLElement>(
            'cr-searchbox-thumbnail');
    assertNull(thumbnailContainer, 'thumbnail should not exist');
    assertEquals(
        lensSearchbox.$.input.inputElement, getDeepActiveElement(),
        'input should be focused');
    await testSearchboxProxy.handler.whenCalled('onThumbnailRemoved');
    assertEquals(
        1, testSearchboxProxy.handler.getCallCount('onThumbnailRemoved'),
        'thumbnail should be removed');
    // When thumbnail is removed, autocomplete should be re-queried
    const args =
        await testSearchboxProxy.handler.whenCalled('stopAutocomplete');
    assertTrue(args.clearResult, 'results should be cleared');
    assertEquals(
        1, testSearchboxProxy.handler.getCallCount('queryAutocomplete'),
        'autocomplete should be queried');
  });

  test('keyboard deletion with non-empty input', async () => {
    testSearchboxProxy.callbackRouterRemote.setThumbnail(
        'foo.png', /*isDeletable=*/ true);
    await testSearchboxProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    const thumbnail =
        lensSearchbox.shadowRoot.querySelector('cr-searchbox-thumbnail');
    assertTrue(thumbnail !== null, 'thumbnail should exist');
    lensSearchbox.$.input.inputElement.value = 'hi';
    lensSearchbox.$.input.dispatchEvent(new CustomEvent('input'));
    lensSearchbox.$.input.focus();
    // Cursor is at the end of the input.
    assertEquals(
        lensSearchbox.$.input.inputElement.selectionStart, 2,
        'cursor should be at the end of the input');
    const backspaceEvent = new KeyboardEvent('keydown', {
      key: 'Backspace',
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    });
    lensSearchbox.$.input.inputElement.dispatchEvent(backspaceEvent);
    // Checking the input value after a backspace event doesn't work
    // so check the default behavior occurs (deleting a character).
    assertFalse(
        backspaceEvent.defaultPrevented,
        'default behavior should not be prevented');
  });

  test(
      'autocomplete triggers on focus on non-empty input with thumbnail',
      async () => {
        testSearchboxProxy.callbackRouterRemote.setThumbnail(
            'foo.png', /*isDeletable=*/ true);
        await testSearchboxProxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        const thumbnail =
            lensSearchbox.shadowRoot.querySelector('cr-searchbox-thumbnail');
        assertTrue(thumbnail !== null, 'thumbnail should exist');
        lensSearchbox.$.input.inputElement.value = 'hi';
        lensSearchbox.$.input.inputElement.dispatchEvent(
            new InputEvent('input'));
        // Make sure lensSearchbox is not focused and matches aren't showing.
        lensSearchbox.$.input.blur();
        assertFalse(await areMatchesShowing(), 'matches should not be showing');

        // Click on lensSearchbox.
        lensSearchbox.$.input.inputElement.dispatchEvent(
            new MouseEvent('mousedown', {button: 0}));

        // Check that autocomplete gets queried with last input on click with
        // non empty input when thumbnail is showing.
        let args =
            await testSearchboxProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(
            args.input, lensSearchbox.$.input.inputElement.value,
            'input should be passed to queryAutocomplete');

        // Make sure lensSearchbox focus is not focused and matches aren't
        // showing.
        lensSearchbox.$.input.blur();
        assertFalse(await areMatchesShowing(), 'matches should not be showing');

        // Tabbing into lensSearchbox.
        lensSearchbox.$.input.inputElement.dispatchEvent(
            new KeyboardEvent('keyup', {
              bubbles: true,
              cancelable: true,
              composed: true,
              key: 'Tab',
            }));

        // Check that autocomplete gets queried with last input on keyup with
        // non empty input when thumbnail is showing.
        args = await testSearchboxProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(
            args.input, lensSearchbox.$.input.inputElement.value,
            'input should be passed to queryAutocomplete again');
      });

  test('query autocomplete for empty inputs when enabled', async () => {
    // Arrange.

    lensSearchbox.$.input.inputElement.value = 'he';
    lensSearchbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    await testSearchboxProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(
        1, testSearchboxProxy.handler.getCallCount('queryAutocomplete'),
        'query autocomplete should be called for non-empty input');

    // Deleting a character queries autocomplete for non-empty input.
    lensSearchbox.$.input.inputElement.value = 'h';
    lensSearchbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    await testSearchboxProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(
        2, testSearchboxProxy.handler.getCallCount('queryAutocomplete'),
        'deleting character should query autocomplete for non-empty input');

    // Deleting a character still queries autocomplete for empty input in lens
    // searchboxes.
    lensSearchbox.$.input.inputElement.value = '';
    lensSearchbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await testSearchboxProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(
        3, testSearchboxProxy.handler.getCallCount('queryAutocomplete'),
        'deleting character should query autocomplete for empty input');
  });
});

suite('SearchboxMotionTweaks', () => {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let lensOverlayElement: LensOverlayAppElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      'autoFocusSearchbox': true,
      'enablePrivacyNotice': false,
      'enableOverlayContextualSearchbox': true,
      'enableGhostLoader': true,
      'enableCsbMotionTweaks': true,
    });

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    await waitAfterNextRender(lensOverlayElement);

    testBrowserProxy.page.shouldShowContextualSearchBox(true);
    await waitAfterNextRender(lensOverlayElement);
  });

  test('GhostLoaderHiddenWhenDropdownVisible', async () => {
    // Simulate focus to show ghost loader
    lensOverlayElement.$.searchbox.dispatchEvent(new CustomEvent('focusin', {
      bubbles: true,
      composed: true,
    }));
    await waitAfterNextRender(lensOverlayElement);

    // Assert ghost loader is visible
    const ghostLoader = lensOverlayElement.shadowRoot!.querySelector(
        'cr-searchbox-ghost-loader');
    assertTrue(ghostLoader !== null);

    // We cannot easily check computed style in this test environment without
    // more setup, but we can check if the attribute is removed or if we can
    // mock it. Let's check if we can simulate the condition that hides it.

    // Simulate dropdown becoming visible
    lensOverlayElement.$.searchbox.dropdownIsVisible = true;
    await waitAfterNextRender(lensOverlayElement);

    // Verify ghost loader becomes hidden
    assertFalse(isVisible(ghostLoader));
  });
});
