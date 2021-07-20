// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ip_config_info_drawer.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function ipConfigInfoDrawerTestSuite() {
  /** @type {?IpConfigInfoDrawerElement} */
  let ipConfigInfoDrawerElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    ipConfigInfoDrawerElement.remove();
    ipConfigInfoDrawerElement = null;
  });

  function initializeIpConfigInfoDrawerElement() {
    ipConfigInfoDrawerElement = /** @type {!IpConfigInfoDrawerElement} */ (
        document.createElement('ip-config-info-drawer'));
    document.body.appendChild(ipConfigInfoDrawerElement);
    assertTrue(!!ipConfigInfoDrawerElement);
    return flushTasks();
  }

  /**
   * Selects the drawer's hideable content area if the drawer is expanded.
   * @return {HTMLElement}
   */
  function getDrawerContentContainer() {
    return /** @type {!HTMLElement} */ (
        ipConfigInfoDrawerElement.$$('#ipConfigInfoElement'));
  }

  /**
   * Selects the drawer's toggle button.
   * @return {!HTMLElement}
   */
  function getDrawerToggle() {
    const toggleButton = ipConfigInfoDrawerElement.$$('cr-expand-button');
    assertTrue(!!toggleButton);
    return /** @type {!HTMLElement} */ (toggleButton);
  }

  test('IpConfigInfoDrawerInitialized', () => {
    return initializeIpConfigInfoDrawerElement().then(() => {
      const expectedDrawerHeader =
          ipConfigInfoDrawerElement.i18n('ipConfigInfoDrawerTitle');
      const toggleButtonLabel = /** @type {HTMLElement} */ (
          ipConfigInfoDrawerElement.$$('cr-expand-button > span'));

      assertTrue(isVisible(getDrawerToggle()));
      dx_utils.assertElementContainsText(
          toggleButtonLabel, expectedDrawerHeader);
    });
  });

  test('IpConfigInfoDrawerContentVisibilityTogglesOnClick', () => {
    return initializeIpConfigInfoDrawerElement()
        .then(() => {
          // Initial state is unexpanded causing element to be hidden.
          assertFalse(!!getDrawerContentContainer());
        })
        // Click toggle button to expand drawer.
        .then(() => getDrawerToggle().click())
        .then(() => {
          // Confirm expanded state visibility is correctly updated.
          assertTrue(!!(getDrawerContentContainer()));
        });
  });
}
