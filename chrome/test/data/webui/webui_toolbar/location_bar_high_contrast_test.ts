// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {LocationBarElement, LocationBarState} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('LocationBarHighContrast', function() {
  let locationBar: LocationBarElement;
  let initialState: LocationBarState;

  const colorLocationBarBackground = 'rgb(0, 0, 255)';
  const colorOmniboxResultsBackground = 'rgb(0, 0, 200)';
  const colorLocationBarBorderOnMismatch = 'rgb(255, 0, 0)';
  const crFocusOutlineColor = 'rgb(0, 255, 0)';
  const colorLocationBarBorder = 'rgb(0, 128, 0)';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Make first element something else focusable so we don't end up with
    // focus.
    document.body.appendChild(document.createElement('input'));
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
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        renderFocused: true,
      },
    };
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
        renderFocused: true,
      },
    };
    await microtasksFinished();
    assertEquals(
        colorOmniboxResultsBackground,
        style.get('background-color')?.toString());
  });

  test('Border (and box-shadow) computation', async () => {
    locationBar.locationBarState = initialState;
    await microtasksFinished();
    const style = locationBar.computedStyleMap();
    // Since we're in high-contrast, we get a border.
    assertEquals('solid', style.get('border-style')?.toString());
    assertEquals(colorLocationBarBorder, style.get('border-color')?.toString());
    assertEquals('none', style.get('box-shadow')?.toString());

    // When focused, we use --color-omnibox-results-background as a border.
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        renderFocused: true,
      },
    };
    await microtasksFinished();
    assertEquals('solid', style.get('border-style')?.toString());
    assertEquals(
        colorOmniboxResultsBackground, style.get('border-color')?.toString());
    // It also has a box-shadow that's pretty border-like.
    assertEquals(
        crFocusOutlineColor + ' 0px 0px 0px 2px inset',
        style.get('box-shadow')?.toString());

    // Input-in-progress for high-contrast just uses the same color as if it
    // were not set.
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
    assertEquals('none', style.get('box-shadow')?.toString());

    // And in high-contrast focus + input-in-progress just uses the focus
    // color.
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        renderFocused: true,
        userInputInProgress: true,
      },
    };
    await microtasksFinished();
    assertEquals('solid', style.get('border-style')?.toString());
    assertEquals(
        colorOmniboxResultsBackground, style.get('border-color')?.toString());
    assertEquals(
        crFocusOutlineColor + ' 0px 0px 0px 2px inset',
        style.get('box-shadow')?.toString());
  });
});
