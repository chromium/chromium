// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings menu. */

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {buildRouter, loadTimeData, pageVisibility, Router, SettingsMenuElement, SettingsRoutes} from 'chrome://settings/settings.js';
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
  test('clearsUrlSearchParam', function() {
    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector = settingsMenu.$.menu;
    ironSelector.forceSynchronousItemUpdate();

    const urlParams = new URLSearchParams('search=foo');
    Router.getInstance().navigateTo(
        Router.getInstance().getRoutes().BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        Router.getInstance().getQueryParameters().toString());
    settingsMenu.$.people.click();
    assertEquals('', Router.getInstance().getQueryParameters().toString());
  });

  test('openResetSection', function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    const path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/reset', path);
  });

  test('navigateToAnotherSection', function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    let path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/reset', path);

    Router.getInstance().navigateTo(routes.PEOPLE);
    flush();

    path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/people', path);
  });

  test('navigateToBasic', function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    const path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/reset', path);

    Router.getInstance().navigateTo(routes.BASIC);
    flush();

    // BASIC has no sub page selected.
    assertFalse(!!selector.selected);
  });

  // <if expr="_google_chrome">
  test('navigateToGetMostChrome', function() {
    loadTimeData.overrideValues({showGetTheMostOutOfChromeSection: true});
    Router.resetInstanceForTesting(buildRouter());
    createSettingsMenu();
    Router.getInstance().navigateTo(routes.GET_MOST_CHROME);
    flush();

    // GET_MOST_CHROME should select the 'About Chrome' entry.
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    const path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/help', path);
  });
  // </if>

  test('noExperimental', async function() {
    loadTimeData.overrideValues({showAdvancedFeaturesMainControl: false});
    Router.resetInstanceForTesting(buildRouter());
    createSettingsMenu();
    await flushTasks();

    const entry = settingsMenu.shadowRoot!.querySelector('a[href=\'/ai\']');
    assertTrue(!!entry);
    assertFalse(isVisible(entry));
  });

  test('navigateToExperimental', async function() {
    loadTimeData.overrideValues({showAdvancedFeaturesMainControl: true});
    Router.resetInstanceForTesting(buildRouter());
    createSettingsMenu();
    Router.getInstance().navigateTo(routes.AI);
    await flushTasks();

    const entry = settingsMenu.shadowRoot!.querySelector('a[href=\'/ai\']');
    assertTrue(!!entry);
    assertTrue(isVisible(entry));

    const selector = settingsMenu.$.menu;
    const path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/ai', path);
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
