// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {LocationBarElement, LocationBarState} from 'chrome://webui-toolbar.top-chrome/app.js';
import {SearchboxBrowserProxy} from 'chrome://webui-toolbar.top-chrome/app.js';

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
    // Use a distinct color mockup against the cr_shared_vars.css default
    // (transparent) to ensure the CSS variable is properly bound during the
    // test.
    locationBar.style.setProperty(
        '--cr-focus-outline-hcm', '2px solid fuchsia');
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

  test('Outline layout computation', async () => {
    locationBar.locationBarState = initialState;
    blurLocationBar();
    await microtasksFinished();
    const style = locationBar.computedStyleMap();

    // Since we're in high-contrast (prefers-contrast), the outline inherits
    // the location bar border color organically.
    assertEquals('solid', style.get('outline-style')?.toString());
    assertEquals('1px', style.get('outline-width')?.toString());
    assertEquals(
        colorLocationBarBorder, style.get('outline-color')?.toString());

    // When focused, rule specificity promotes the focus outline color
    // (which in HCM is mapped via --cr-focus-outline-hcm).
    focusLocationBar();
    await microtasksFinished();
    // Verify that the focus ring binds correctly to our custom
    // --cr-focus-outline-hcm mockup (fuchsia / rgb(255, 0, 255)) rather than
    // falling back to transparent.
    assertEquals('rgb(255, 0, 255)', style.get('outline-color')?.toString());
    assertEquals('solid', style.get('outline-style')?.toString());
    assertEquals('2px', style.get('outline-width')?.toString());

    // If popup is open, the explicit focus-ring goes away. Because we are
    // in prefers-contrast and still focused, it falls back to the results bg.
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        popupOpen: true,
      },
    };
    await microtasksFinished();
    assertEquals(
        colorOmniboxResultsBackground, style.get('outline-color')?.toString());
    assertEquals('1px', style.get('outline-width')?.toString());

    // Input-in-progress for high-contrast just uses the same base outline color
    // as if it were not set, because standard mismatch outlines are disabled
    // in prefers-contrast.
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
        colorLocationBarBorder, style.get('outline-color')?.toString());
    assertEquals('1px', style.get('outline-width')?.toString());

    // And in high-contrast focus + input-in-progress it leverages the focus hcm
    // again.
    focusLocationBar();
    locationBar.locationBarState = {
      ...initialState,
      locationBarFlags: {
        ...initialState.locationBarFlags,
        userInputInProgress: true,
      },
    };
    await microtasksFinished();
    assertEquals('rgb(255, 0, 255)', style.get('outline-color')?.toString());
    assertEquals('2px', style.get('outline-width')?.toString());
  });
});
