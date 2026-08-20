// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {OmniboxTextColor, SearchboxBrowserProxy} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {LocationBarElement, LocationBarState} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('LocationBar', function() {
  let locationBar: LocationBarElement;
  let initialState: LocationBarState;

  setup(() => {
    SearchboxBrowserProxy.setInstance(new TestSearchboxBrowserProxy());

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Make first element something else focusable so we don't end up with
    // focus.
    document.body.appendChild(document.createElement('input'));
    locationBar = document.createElement('location-bar');
    locationBar.setAttribute('id', 'location-bar');
    initialState = locationBar.locationBarState;

    document.body.appendChild(locationBar);
  });

  test('SecurityIconAccessibility', async () => {
    const accessibilityLabel = 'Connection is secure';
    const accessibilityDescription = 'This site uses an encrypted connection';
    locationBar.locationBarState = {
      ...initialState,
      lhsChipsState: {
        securityChip: {
          icon: {handleId: 0n},
          securityLevel: 0,
          text: '',
          tooltip: '',
          accessibilityState: {
            label: accessibilityLabel,
            description: accessibilityDescription,
          },
          isClickable: true,
          isTextDangerous: false,
          isVisible: true,
        },
        activityIndicators: [],
        permissionDashboard: null,
      },
    };
    await microtasksFinished();

    const locationIcon = locationBar.shadowRoot.querySelector('location-icon');
    assertTrue(!!locationIcon);
    const button = locationIcon.$.container;
    assertTrue(!!button);
    assertEquals('BUTTON', button.tagName);
    assertEquals(accessibilityLabel, button.ariaLabel);
    assertEquals(accessibilityDescription, button.ariaDescription);
    assertEquals(0, button.tabIndex);

    // Test non-clickable state
    locationBar.locationBarState = {
      ...initialState,
      lhsChipsState: {
        ...locationBar.locationBarState.lhsChipsState,
        securityChip: {
          ...locationBar.locationBarState.lhsChipsState.securityChip,
          isClickable: false,
        },
      },
    };
    await microtasksFinished();
    assertEquals(-1, button.tabIndex);
  });

  test('Chip hovered state', async () => {
    // Force the location bar to show the security chip.
    locationBar.locationBarState = {
      ...initialState,
      lhsChipsState: {
        securityChip: {
          icon: {handleId: 0n},
          securityLevel: 0,
          text: 'Not secure',
          tooltip: 'View site information',
          accessibilityState: {
            label: 'Not secure',
            description: '',
          },
          isClickable: true,
          isTextDangerous: false,
          isVisible: true,
        },
        activityIndicators: [],
        permissionDashboard: null,
      },
    };
    await microtasksFinished();

    const locationIcon = locationBar.shadowRoot.querySelector('location-icon');
    assertTrue(!!locationIcon);

    locationIcon.dispatchEvent(new PointerEvent('pointerenter'));
    assertTrue(locationBar.hasAttribute('chip-hovered'));

    locationIcon.dispatchEvent(new PointerEvent('pointerleave'));
    assertFalse(locationBar.hasAttribute('chip-hovered'));

    // Verify pointercancel also removes the hovered state.
    locationIcon.dispatchEvent(new PointerEvent('pointerenter'));
    assertTrue(locationBar.hasAttribute('chip-hovered'));

    locationIcon.dispatchEvent(new PointerEvent('pointercancel'));
    assertFalse(locationBar.hasAttribute('chip-hovered'));
  });

  test('ContentSettingsIcons hovered state', async () => {
    // Force the location bar to show a content setting icon.
    locationBar.locationBarState = {
      ...initialState,
      contentSettingImageStates: [{
        type: 0,  // kCookies
        isBlocked: true,
        tooltip: 'Cookies blocked',
        accessibilityString: '',
        isBubbleVisible: false,
        shouldRunAnimation: false,
        explanatoryString: '',
      }],
    };
    await microtasksFinished();

    const contentSettingsIcons =
        locationBar.shadowRoot?.querySelector('content-settings-icons');
    assertTrue(!!contentSettingsIcons);

    const contentSettingIcon =
        contentSettingsIcons.shadowRoot?.querySelector('content-setting-icon');
    assertTrue(!!contentSettingIcon);

    const iconButton =
        contentSettingIcon.shadowRoot?.querySelector('toolbar-chip-button');
    assertTrue(!!iconButton);

    // Hovering the container should NOT trigger the hovered state.
    contentSettingsIcons.dispatchEvent(new PointerEvent('pointerenter'));
    assertFalse(locationBar.hasAttribute('chip-hovered'));

    // Hovering the individual icon button SHOULD trigger the hovered state.
    iconButton.dispatchEvent(new PointerEvent('pointerenter'));
    assertTrue(locationBar.hasAttribute('chip-hovered'));

    iconButton.dispatchEvent(new PointerEvent('pointerleave'));
    assertFalse(locationBar.hasAttribute('chip-hovered'));

    // Verify pointercancel also removes the hovered state.
    iconButton.dispatchEvent(new PointerEvent('pointerenter'));
    assertTrue(locationBar.hasAttribute('chip-hovered'));

    iconButton.dispatchEvent(new PointerEvent('pointercancel'));
    assertFalse(locationBar.hasAttribute('chip-hovered'));
  });

  test('Clear button visibility and click', async () => {
    locationBar.locationBarState = {
      ...initialState,
      omniboxViewState: {
        ...initialState.omniboxViewState,
        textPieces: [
          {
            text: 'example.com',
            strikethrough: false,
            color: OmniboxTextColor.kOmniboxText,
          },
        ],
        selection: {start: 0, end: 0},
      },
    };
    await microtasksFinished();

    let clearButton = locationBar.shadowRoot.querySelector('#clear-all');
    assertTrue(!clearButton);

    locationBar.locationBarState = {
      ...locationBar.locationBarState,
      locationBarFlags: {
        userInputInProgress: true,
        popupOpen: false,
        forceAimButtonFocusRing: false,
        isVirtualKeyboardVisible: true,
      },
    };
    await microtasksFinished();

    clearButton = locationBar.shadowRoot.querySelector('#clear-all');
    assertTrue(!!clearButton);
    assertEquals('webui-toolbar:close', clearButton.getAttribute('iron-icon'));

    locationBar.touchUi = true;
    await microtasksFinished();
    assertEquals(
        'webui-toolbar:backspace_filled',
        clearButton.getAttribute('iron-icon'));

    locationBar.touchUi = false;
    await microtasksFinished();
    assertEquals('webui-toolbar:close', clearButton.getAttribute('iron-icon'));

    const omnibox = locationBar.$.omnibox;
    const searchbox = omnibox.$.textInput;
    const input = searchbox.inputElement;

    // Test 1: Pointer events (pointerdown + pointerup) should clear the input.
    const rect = clearButton.getBoundingClientRect();
    clearButton.dispatchEvent(new PointerEvent('pointerdown', {
      button: 0,
      pointerId: 1,
      clientX: rect.left,
      clientY: rect.top,
      bubbles: true,
      composed: true,
    }));
    clearButton.dispatchEvent(new PointerEvent('pointerup', {
      button: 0,
      pointerId: 1,
      clientX: rect.left,
      clientY: rect.top,
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertEquals('', input.value);
    assertEquals('', omnibox.$.textContainer.textContent);
    assertEquals(searchbox, omnibox.shadowRoot.activeElement);

    // Enter text and update state to re-enable clear button.
    locationBar.locationBarState = {
      ...locationBar.locationBarState,
      omniboxViewState: {
        ...locationBar.locationBarState.omniboxViewState,
        textPieces: [
          {
            text: 'test input',
            strikethrough: false,
            color: OmniboxTextColor.kOmniboxText,
          },
        ],
      },
    };
    await microtasksFinished();

    clearButton = locationBar.shadowRoot.querySelector('#clear-all');
    assertTrue(!!clearButton);

    // Test 2: Mouse click with detail > 0 is ignored by @click listener because
    // mouse clicks are already processed via pointerup in PressHandler.
    searchbox.setInputText('test input');
    clearButton.dispatchEvent(new MouseEvent('click', {detail: 1}));
    await microtasksFinished();
    assertEquals('test input', input.value);

    // Test 3: Keyboard synthetic click (Enter/Space) with detail === 0 clears
    // the input.
    clearButton.dispatchEvent(new MouseEvent('click', {detail: 0}));
    await microtasksFinished();

    assertEquals('', input.value);
    assertEquals('', omnibox.$.textContainer.textContent);
    assertEquals(searchbox, omnibox.shadowRoot.activeElement);
  });
});
