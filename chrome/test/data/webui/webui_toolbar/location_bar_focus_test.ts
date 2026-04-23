// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {LocationBarElement, LocationBarState} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('LocationBarFocus', function() {
  let locationBar: LocationBarElement;
  let other: HTMLInputElement;  // A focusable sibling element.
  let initialState: LocationBarState;

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

  test('Border (and box-shadow) computation', async () => {
    locationBar.locationBarState = initialState;
    blurLocationBar();
    await microtasksFinished();
    const style = locationBar.computedStyleMap();
    assertEquals('none', style.get('border-style')?.toString());
    assertEquals('none', style.get('box-shadow')?.toString());

    // Focus doesn't add a border.
    focusLocationBar();
    await microtasksFinished();
    assertEquals('none', style.get('border-style')?.toString());
    // It does hover have a box-shadow that's pretty border-like.
    assertEquals(
        crFocusOutlineColor + ' 0px 0px 0px 2px inset',
        style.get('box-shadow')?.toString());

    // No outline in regular contrast.
    assertEquals('none', style.get('outline-style')?.toString());

    // If popup is open, the box-shadow goes away.
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        popupOpen: true,
      },
    };
    await microtasksFinished();
    assertEquals('none', style.get('box-shadow')?.toString());

    // In-progress gets a special border....
    blurLocationBar();
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        userInputInProgress: true,
      },
    };
    await microtasksFinished();
    assertEquals('solid', style.get('border-style')?.toString());
    assertEquals(
        colorLocationBarBorderOnMismatch,
        style.get('border-color')?.toString());
    assertEquals('none', style.get('box-shadow')?.toString());

    // ...unless it has focus, too.
    focusLocationBar();
    await microtasksFinished();
    assertEquals('none', style.get('border-style')?.toString());
    assertEquals(
        crFocusOutlineColor + ' 0px 0px 0px 2px inset',
        style.get('box-shadow')?.toString());
    assertEquals('none', style.get('outline-style')?.toString());
  });
});
