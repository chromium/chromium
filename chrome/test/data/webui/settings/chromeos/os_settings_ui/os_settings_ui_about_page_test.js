// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, Router, routes, routesMojom, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

const {Section} = routesMojom;

suite('<os-settings-ui> about page', () => {
  let ui;
  let fakeNearbySettings;

  suiteSetup(() => {
    fakeNearbySettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbySettings);
  });

  async function createElement() {
    const element = document.createElement('os-settings-ui');
    document.body.appendChild(element);
    await CrSettingsPrefs.initialized;
    flush();
    return element;
  }

  teardown(() => {
    ui.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'Clicking "About ChromeOS" menu item should focus About section',
      async () => {
        ui = await createElement();
        const router = Router.getInstance();
        const settingsMenu = ui.shadowRoot.querySelector('os-settings-menu');
        assertTrue(!!settingsMenu);

        // As of iron-selector 2.x, need to force iron-selector to update before
        // clicking items on it, or wait for 'iron-items-changed'
        const ironSelector =
            settingsMenu.shadowRoot.querySelector('iron-selector');
        ironSelector.forceSynchronousItemUpdate();

        const {aboutItem} = settingsMenu.$;
        aboutItem.click();
        flush();

        assertEquals(routes.ABOUT_ABOUT, router.currentRoute);
        assertNotEquals(aboutItem, settingsMenu.shadowRoot.activeElement);

        const settingsMain = ui.shadowRoot.querySelector('os-settings-main');
        const aboutPage =
            settingsMain.shadowRoot.querySelector('os-settings-about-page');
        await waitBeforeNextRender(aboutPage);

        const aboutSection = aboutPage.shadowRoot.querySelector(
            `page-displayer[section="${Section.kAboutChromeOs}"]`);
        assertEquals(aboutSection, aboutPage.shadowRoot.activeElement);
      });

  test('Detailed build info subpage is directly navigable', async () => {
    Router.getInstance().navigateTo(routes.DETAILED_BUILD_INFO);
    ui = await createElement();
    flush();

    const settingsMain = ui.shadowRoot.querySelector('os-settings-main');
    const aboutPage =
        settingsMain.shadowRoot.querySelector('os-settings-about-page');
    const detailedBuildInfoSubpage = aboutPage.shadowRoot.querySelector(
        'settings-detailed-build-info-subpage');
    await waitBeforeNextRender(detailedBuildInfoSubpage);
    assertTrue(!!detailedBuildInfoSubpage);
  });
});
