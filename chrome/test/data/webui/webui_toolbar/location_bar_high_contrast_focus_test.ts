// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {LocationBarElement, LocationBarState} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('LocationBarHighContrastFocus', function() {
  let locationBar: LocationBarElement;
  let other: HTMLInputElement;  // A focusable sibling element.
  let initialState: LocationBarState;

  const colorLocationBarBackground = 'rgb(0, 0, 255)';
  const colorOmniboxResultsBackground = 'rgb(0, 0, 200)';
  const colorLocationBarBorderOnMismatch = 'rgb(255, 0, 0)';
  const crFocusOutlineColor = 'rgb(0, 255, 0)';
  const colorLocationBarBorder = 'rgb(0, 128, 0)';

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
    // Matches what cr_shared_vars.css uses
    locationBar.style.setProperty(
        '--cr-focus-outline-hcm', '2px solid transparent');
    locationBar.style.setProperty(
        '--color-location-bar-border', colorLocationBarBorder);
    document.body.appendChild(locationBar);
  });

  test('Background color computation', async () => {
    const style = locationBar.computedStyleMap();
    assertEquals(
        colorLocationBarBackground, style.get('background-color')?.toString());

    // In high-contrast mode, input-in-progress w/o focus keeps the
    // location bar colors.
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
        colorLocationBarBackground, style.get('background-color')?.toString());

    // If focused it uses omnibox color and not location bar one.
    focusLocationBar();
    await microtasksFinished();
    assertEquals(
        colorOmniboxResultsBackground,
        style.get('background-color')?.toString());

    // Both focus + input-in-progress gets omnibox-like colors.
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
    // Since we're in high-contrast, we get a border.
    assertEquals('solid', style.get('border-style')?.toString());
    assertEquals(colorLocationBarBorder, style.get('border-color')?.toString());
    assertEquals('none', style.get('box-shadow')?.toString());

    // When focused, we use --color-omnibox-results-background as a border.
    focusLocationBar();
    await microtasksFinished();
    assertEquals('solid', style.get('border-style')?.toString());
    assertEquals(
        colorOmniboxResultsBackground, style.get('border-color')?.toString());
    // HCM has an outline; fox-shadow appears to report as none and not
    // render.
    assertEquals('solid', style.get('outline-style')?.toString());
    assertEquals('none', style.get('box-shadow')?.toString());

    // If popup is open, the box-shadow goes away.
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        popupOpen: true,
      },
    };
    await microtasksFinished();
    assertEquals('none', style.get('outline-style')?.toString());
    assertEquals('none', style.get('box-shadow')?.toString());

    // Input-in-progress for high-contrast just uses the same color as if it
    // were not set.
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
    assertEquals(colorLocationBarBorder, style.get('border-color')?.toString());
    assertEquals('none', style.get('outline-style')?.toString());
    assertEquals('none', style.get('box-shadow')?.toString());

    // And in high-contrast focus + input-in-progress just uses the focus
    // color.
    focusLocationBar();
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
        colorOmniboxResultsBackground, style.get('border-color')?.toString());
    assertEquals('solid', style.get('outline-style')?.toString());
    assertEquals('none', style.get('box-shadow')?.toString());
  });
});
