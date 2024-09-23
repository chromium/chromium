// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings performance menu item. */

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsMenuElement} from 'chrome://settings/settings.js';
import {pageVisibility, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('SettingsMenuPerformance', function() {
  let settingsMenu: SettingsMenuElement;

  function getPerformanceMenuItem() {
    return settingsMenu.shadowRoot!.querySelector<HTMLElement>('#performance');
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    Router.getInstance().navigateTo(routes.PERFORMANCE, undefined);
    settingsMenu = document.createElement('settings-menu');
    settingsMenu.pageVisibility = pageVisibility;
    document.body.appendChild(settingsMenu);
    flush();
  });

  test('navigateToPerformanceSection', function() {
    const menu = settingsMenu.$.menu;

    assertTrue(
        !!menu.selected,
        'a menu item should be selected when directly navigating to the ' +
            'performance route');
    assertEquals(
        '/performance', menu.selected.toString(),
        'the selected menu item should be for the performance settings');
  });

  test('performanceMenuItemExistsAndVisible', function() {
    const menuItem = getPerformanceMenuItem();
    assertTrue(
        !!menuItem,
        'performance menu item should exist when features are available');
    assertFalse(
        menuItem.hidden,
        'performance menu item should be visible under default pageVisibility');
  });

  test('performanceMenuItemHidden', function() {
    settingsMenu.pageVisibility =
        Object.assign(settingsMenu.pageVisibility || {}, {performance: false});
    assertTrue(
        getPerformanceMenuItem()!.hidden,
        'performance menu item should be hidden when pageVisibility is false');
  });
});
