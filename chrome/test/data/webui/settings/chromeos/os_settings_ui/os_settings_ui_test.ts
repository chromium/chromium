// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, OsSettingsMainElement, OsSettingsPageElement, OsSettingsUiElement} from 'chrome://os-settings/os_settings.js';
import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/** @fileoverview Suite of tests for the OS Settings ui and main page. */

suite('OSSettingsUi', function() {
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement|null;
  let settingsPage: OsSettingsPageElement|null;

  suiteSetup(async function() {
    document.body.innerHTML = '';
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    await CrSettingsPrefs.initialized;
    settingsMain = ui.shadowRoot!.querySelector('os-settings-main');
    assert(settingsMain);

    settingsPage = settingsMain.shadowRoot!.querySelector('os-settings-page');
    assert(settingsPage);

    const idleRender =
        settingsPage.shadowRoot!.querySelector('settings-idle-load');
    assert(idleRender);
    await idleRender.get();
    flush();
  });

  test('Update required end of life banner visibility', function() {
    flush();
    assert(settingsPage);
    assertEquals(
        null,
        settingsPage.shadowRoot!.querySelector('#updateRequiredEolBanner'));

    settingsPage!.set('showUpdateRequiredEolBanner_', true);
    flush();
    assertTrue(
        !!settingsPage.shadowRoot!.querySelector('#updateRequiredEolBanner'));
  });

  test('Update required end of life banner close button click', function() {
    assert(settingsPage);
    settingsPage.set('showUpdateRequiredEolBanner_', true);
    flush();
    const banner = settingsPage.shadowRoot!.querySelector<HTMLElement>(
        '#updateRequiredEolBanner');
    assertTrue(!!banner);

    const closeButton = settingsPage.shadowRoot!.querySelector<HTMLElement>(
        '#closeUpdateRequiredEol');
    assert(closeButton);
    closeButton.click();
    flush();
    assertEquals('none', banner.style.display);
  });

  test('clicking icon closes drawer', async () => {
    flush();
    const drawer = ui.shadowRoot!.querySelector<CrDrawerElement>('#drawer');
    assert(drawer);
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);

    // Clicking the drawer icon closes the drawer.
    ui.shadowRoot!.querySelector<HTMLElement>('#iconButton')!.click();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
    assertTrue(drawer.wasCanceled());
  });
});
