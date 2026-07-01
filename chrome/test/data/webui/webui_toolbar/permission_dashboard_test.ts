// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {PermissionAction, PermissionChipTheme, PermissionPromptStyle} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {PermissionChipState, PermissionDashboardElement, PermissionDashboardState} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('PermissionDashboardTest', function() {
  let dashboard: PermissionDashboardElement;

  function createChipState(isVisible: boolean): PermissionChipState {
    return {
      isFullyCollapsed: false,
      accessibilityName: 'Camera',
      tooltip: 'Camera in use',
      isVisible: isVisible,
      iconName: 'kVideocamChromeRefreshIcon',
      theme: PermissionChipTheme.kNormalVisibility,
      promptStyle: PermissionPromptStyle.kChip,
      userDecision: PermissionAction.kGranted,
      shouldShowBlockedIcon: false,
      message: 'Camera',
    };
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Set required CSS variables that are usually provided by the location bar.
    document.body.style.setProperty('--location-bar-chip-padding', '4px');
    document.body.style.setProperty(
        '--location-bar-child-corner-radius', '16px');
    dashboard = document.createElement('permission-dashboard');
    document.body.appendChild(dashboard);
  });

  test('Render only indicator chip', async function() {
    const state: PermissionDashboardState = {
      indicatorChip: createChipState(true),
      requestChip: createChipState(false),
      isDividerVisible: false,
    };
    dashboard.dashboardState = state;
    await microtasksFinished();

    const indicator = dashboard.shadowRoot.querySelector('#indicator-chip');
    const request = dashboard.shadowRoot.querySelector('#request-chip');

    assertTrue(!!indicator);
    assertFalse(!!request);
  });

  test('Render only request chip', async function() {
    const state: PermissionDashboardState = {
      indicatorChip: createChipState(false),
      requestChip: createChipState(true),
      isDividerVisible: false,
    };
    dashboard.dashboardState = state;
    await microtasksFinished();

    const indicator = dashboard.shadowRoot.querySelector('#indicator-chip');
    const request = dashboard.shadowRoot.querySelector('#request-chip');

    assertFalse(!!indicator);
    assertTrue(!!request);

    // Request chip should not have has-divider attribute
    assertFalse(request.hasAttribute('has-divider'));
  });

  test('Render both chips and divider', async function() {
    const state: PermissionDashboardState = {
      indicatorChip: createChipState(true),
      requestChip: createChipState(true),
      isDividerVisible: true,
    };
    dashboard.dashboardState = state;
    await microtasksFinished();

    const indicator = dashboard.shadowRoot.querySelector('#indicator-chip');
    const request = dashboard.shadowRoot.querySelector('#request-chip');

    assertTrue(!!indicator);
    assertTrue(!!request);

    // Request chip should have has-divider attribute
    assertTrue(request.hasAttribute('has-divider'));
    assertFalse(indicator.hasAttribute('has-divider'));

    // Check request chip styles
    const requestStyle = window.getComputedStyle(request);
    assertEquals('-2px', requestStyle.marginLeft);

    // Check internal chip border-radius and mask
    const innerChip = request.shadowRoot!.querySelector('#chip');
    assertTrue(!!innerChip);
    const innerChipStyle = window.getComputedStyle(innerChip);
    assertEquals('0px', innerChipStyle.borderTopLeftRadius);
    assertEquals('0px', innerChipStyle.borderBottomLeftRadius);

    // Check mask-image contains radial-gradient
    assertTrue(innerChipStyle.maskImage.includes('radial-gradient'));
  });

  test('Render both chips and divider in RTL', async function() {
    document.documentElement.dir = 'rtl';
    const state: PermissionDashboardState = {
      indicatorChip: createChipState(true),
      requestChip: createChipState(true),
      isDividerVisible: true,
    };
    dashboard.dashboardState = state;
    await microtasksFinished();

    const request = dashboard.shadowRoot.querySelector('#request-chip');
    assertTrue(!!request);

    // Check request chip styles in RTL
    const requestStyle = window.getComputedStyle(request);
    // In RTL, the inline-start margin applies to margin-right
    assertEquals('-2px', requestStyle.marginRight);

    // Check internal chip border-radius and mask
    const innerChip = request.shadowRoot!.querySelector('#chip');
    assertTrue(!!innerChip);
    const innerChipStyle = window.getComputedStyle(innerChip);
    // In RTL, the straight edge is on the right side
    assertEquals('0px', innerChipStyle.borderTopRightRadius);
    assertEquals('0px', innerChipStyle.borderBottomRightRadius);

    // Mask image should flip to 100% logic
    assertTrue(innerChipStyle.maskImage.includes('radial-gradient'));
    assertTrue(innerChipStyle.maskImage.includes('100%'));

    // Cleanup
    document.documentElement.dir = 'ltr';
  });
});
