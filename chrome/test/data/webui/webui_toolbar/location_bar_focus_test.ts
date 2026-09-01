// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, INVALID_FOCUS_REQUEST_HANDLE, SearchboxBrowserProxy} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {LocationBarElement, LocationBarState, OmniboxAction} from 'chrome://webui-toolbar.top-chrome/app.js';

class MockToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super(['onLocationBarFocusWithinChanged', 'onOmniboxAction']);
  }

  onLocationBarFocusWithinChanged(focused: boolean) {
    this.methodCalled('onLocationBarFocusWithinChanged', focused);
  }

  onOmniboxAction(action: OmniboxAction) {
    this.methodCalled('onOmniboxAction', action);
  }
}

class MockBrowserProxy extends TestBrowserProxy {
  toolbarUIHandler: MockToolbarUiHandler = new MockToolbarUiHandler();

  addFocusRequestListener() {
    return INVALID_FOCUS_REQUEST_HANDLE;
  }

  removeFocusRequestListener() {}
}

suite('LocationBarFocus', function() {
  let locationBar: LocationBarElement;
  let other: HTMLInputElement;  // A focusable sibling element.
  let initialState: LocationBarState;
  let uiHandler: MockToolbarUiHandler;

  const colorLocationBarBackground = 'rgb(0, 0, 255)';
  const colorOmniboxResultsBackground = 'rgb(0, 0, 200)';
  const colorLocationBarBorderOnMismatch = 'rgb(255, 0, 0)';
  const crFocusOutlineColor = 'rgb(0, 255, 0)';

  function focusLocationBar(): void {
    locationBar.$.omnibox.$.textInput.focus();
  }

  function blurLocationBar(): void {
    other.focus();
  }

  setup(() => {
    const browserProxy = new MockBrowserProxy();
    uiHandler = browserProxy.toolbarUIHandler;
    BrowserProxyImpl.setInstance(browserProxy as any);
    SearchboxBrowserProxy.setInstance(new TestSearchboxBrowserProxy());

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Make first element something else focusable so we don't end up with
    // focus. It'll also be handy for transferring focus to.
    other = document.createElement('input');
    document.body.appendChild(other);
    locationBar = document.createElement('location-bar');
    locationBar.setAttribute('id', 'location-bar');
    initialState = locationBar.locationBarState;

    locationBar.style.setProperty(
        '--color-location-bar-background', colorLocationBarBackground);
    locationBar.style.setProperty(
        '--color-omnibox-results-background', colorOmniboxResultsBackground);
    locationBar.style.setProperty(
        '--color-location-bar-border-on-mismatch',
        colorLocationBarBorderOnMismatch);
    locationBar.style.setProperty(
        '--cr-focus-outline-color', crFocusOutlineColor);
    document.body.appendChild(locationBar);
  });

  test('Background color computation', async () => {
    const style = locationBar.computedStyleMap();
    blurLocationBar();
    assertEquals(
        colorLocationBarBackground, style.get('background-color')?.toString());

    // If focused it uses omnibox color and not location bar one.
    focusLocationBar();
    await microtasksFinished();
    assertEquals(
        colorOmniboxResultsBackground,
        style.get('background-color')?.toString());

    // Still does even if popup is open.
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        popupOpen: true,
      },
    };
    await microtasksFinished();
    assertEquals(
        colorOmniboxResultsBackground,
        style.get('background-color')?.toString());

    // Similarly input in progress will get omnibox-like colors.
    blurLocationBar();
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        userInputInProgress: true,
      },
    };
    await microtasksFinished();
    assertEquals(
        colorOmniboxResultsBackground,
        style.get('background-color')?.toString());
  });

  test('Outline layout computation', async () => {
    locationBar.locationBarState = initialState;
    blurLocationBar();
    await microtasksFinished();
    const style = locationBar.computedStyleMap();

    // Default invisible outline
    assertEquals('solid', style.get('outline-style')?.toString());
    assertEquals('1px', style.get('outline-width')?.toString());
    assertEquals('rgba(0, 0, 0, 0)', style.get('outline-color')?.toString());

    // Focus upgrades the outline to a 2px boundary.
    focusLocationBar();
    await microtasksFinished();
    assertEquals('solid', style.get('outline-style')?.toString());
    assertEquals('2px', style.get('outline-width')?.toString());
    assertEquals(crFocusOutlineColor, style.get('outline-color')?.toString());

    // If popup is open, the outline reverts.
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        popupOpen: true,
      },
    };
    await microtasksFinished();
    assertEquals('1px', style.get('outline-width')?.toString());
    assertEquals('rgba(0, 0, 0, 0)', style.get('outline-color')?.toString());

    // In-progress gets a special outline color....
    blurLocationBar();
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        userInputInProgress: true,
      },
    };
    await microtasksFinished();
    assertEquals('solid', style.get('outline-style')?.toString());
    assertEquals('1px', style.get('outline-width')?.toString());
    assertEquals(
        colorLocationBarBorderOnMismatch,
        style.get('outline-color')?.toString());

    // ...unless it has focus, too. (The 2px focus ring overrides it).
    focusLocationBar();
    await microtasksFinished();
    assertEquals('solid', style.get('outline-style')?.toString());
    assertEquals('2px', style.get('outline-width')?.toString());
    assertEquals(crFocusOutlineColor, style.get('outline-color')?.toString());
  });

  test('Focus state events', async () => {
    locationBar.locationBarState = initialState;
    blurLocationBar();
    await microtasksFinished();
    uiHandler.reset();

    focusLocationBar();
    await microtasksFinished();
    assertEquals(1, uiHandler.getCallCount('onLocationBarFocusWithinChanged'));
    let lastFocused =
        uiHandler.getArgs('onLocationBarFocusWithinChanged').at(-1);
    assertEquals(true, lastFocused);

    blurLocationBar();
    await microtasksFinished();
    assertEquals(2, uiHandler.getCallCount('onLocationBarFocusWithinChanged'));
    lastFocused = uiHandler.getArgs('onLocationBarFocusWithinChanged').at(-1);
    assertEquals(false, lastFocused);
  });
});
