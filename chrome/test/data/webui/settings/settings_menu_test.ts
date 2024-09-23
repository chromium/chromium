// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings menu. */

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsMenuElement, SettingsRoutes} from 'chrome://settings/settings.js';
import {resetRouterForTesting, loadTimeData, pageVisibility, Router} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

// clang-format on

suite('SettingsMenu', function() {
  let settingsMenu: SettingsMenuElement;
  let routes: SettingsRoutes;

  function createSettingsMenu() {
    routes = Router.getInstance().getRoutes();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
    flush();
  }

  setup(function() {
    createSettingsMenu();
  });

  // Test that navigating via the paper menu always clears the current
  // search URL parameter.
  test('clearsUrlSearchParam', async () => {
    const urlParams = new URLSearchParams('search=foo');
    Router.getInstance().navigateTo(
        Router.getInstance().getRoutes().BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        Router.getInstance().getQueryParameters().toString());
    settingsMenu.$.people.click();
    await settingsMenu.$.menu.updateComplete;
    assertEquals('', Router.getInstance().getQueryParameters().toString());
  });

  test('openResetSection', function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected.toString());
  });

  test('navigateToAnotherSection', function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected.toString());

    Router.getInstance().navigateTo(routes.PEOPLE);
    flush();

    assertTrue(!!selector.selected);
    assertEquals('/people', selector.selected.toString());
  });

  test('navigateToBasic', function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected.toString());

    Router.getInstance().navigateTo(routes.BASIC);
    flush();

    // BASIC has no sub page selected.
    assertFalse(!!selector.selected);
  });

  test('noExperimental', async function() {
    loadTimeData.overrideValues({showAdvancedFeaturesMainControl: false});
    resetRouterForTesting();
    createSettingsMenu();
    await flushTasks();

    const entry = settingsMenu.shadowRoot!.querySelector('a[href=\'/ai\']');
    assertTrue(!!entry);
    assertFalse(isVisible(entry));
  });

  test('navigateToExperimental', async function() {
    loadTimeData.overrideValues({showAdvancedFeaturesMainControl: true});
    resetRouterForTesting();
    createSettingsMenu();
    Router.getInstance().navigateTo(routes.AI);
    await flushTasks();

    const entry = settingsMenu.shadowRoot!.querySelector('a[href=\'/ai\']');
    assertTrue(!!entry);
    assertTrue(isVisible(entry));

    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/ai', selector.selected.toString());
  });

  test('pageVisibility', function() {
    function assertPagesHidden(expectedHidden: boolean) {
      const ids = [
        'accessibility', 'appearance',
        // <if expr="not is_chromeos">
        'defaultBrowser',
        // </if>
        'downloads', 'languages', 'onStartup', 'people', 'reset',
        // <if expr="not chromeos_ash">
        'system',
        // </if>
      ];

      for (const id of ids) {
        assertEquals(
            expectedHidden,
            settingsMenu.shadowRoot!.querySelector<HTMLElement>(
                                        `#${id}`)!.hidden);
      }
    }

    // The default pageVisibility should not cause menu items to be hidden.
    assertPagesHidden(false);

    // Set the visibility of the pages under test to "false".
    settingsMenu.pageVisibility = Object.assign(pageVisibility || {}, {
      a11y: false,
      advancedSettings: false,
      appearance: false,
      defaultBrowser: false,
      downloads: false,
      languages: false,
      multidevice: false,
      onStartup: false,
      people: false,
      reset: false,
      safetyCheck: false,
      system: false,
    });
    flush();

    // Now, the menu items should be hidden.
    assertPagesHidden(true);
  });
});
