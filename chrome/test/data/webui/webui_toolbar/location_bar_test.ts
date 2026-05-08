// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {LocationBarElement, LocationBarState} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('LocationBar', function() {
  let locationBar: LocationBarElement;
  let initialState: LocationBarState;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Make first element something else focusable so we don't end up with
    // focus.
    document.body.appendChild(document.createElement('input'));
    locationBar = document.createElement('location-bar');
    locationBar.setAttribute('id', 'location-bar');
    initialState = locationBar.locationBarState;

    document.body.appendChild(locationBar);
  });

  test('Chip hovered state', async () => {
    // Force the location bar to show the security chip.
    locationBar.locationBarState = {
      ...initialState,
      lhsChipsState: {
        securityChip: {
          icon: 0,
          securityLevel: 0,
          text: 'Not secure',
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
        locationBar.shadowRoot.querySelector('content-settings-icons');
    assertTrue(!!contentSettingsIcons);

    const contentSettingIcon =
        contentSettingsIcons.shadowRoot.querySelector('content-setting-icon');
    assertTrue(!!contentSettingIcon);

    const iconButton =
        contentSettingIcon.shadowRoot.querySelector('cr-icon-button');
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
});
