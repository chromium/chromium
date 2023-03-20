// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, OsSettingsMainElement, OsSettingsPageElement, OsSettingsSectionElement, OsSettingsUiElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
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

    // Simulate Kerberos enabled.
    settingsPage.showKerberosSection = true;

    const idleRender =
        settingsPage.shadowRoot!.querySelector('settings-idle-load');
    assert(idleRender);
    await idleRender.get();
    flush();
  });

  /**
   * Verifies the section has a visible #main element and that any possible
   * sub-pages are hidden.
   */
  function verifySubpagesHidden(section: OsSettingsSectionElement): void {
    // Check if there are any sub-pages to verify, being careful to filter out
    // any dom-if and template noise when we search.
    const pages = section.firstElementChild!.shadowRoot!.querySelector(
        'settings-animated-pages');
    if (!pages) {
      return;
    }

    const children =
        pages.shadowRoot!.querySelector('slot')!.assignedNodes({flatten: true})
            .filter(n => n.nodeType === Node.ELEMENT_NODE) as HTMLElement[];

    const stampedChildren = children.filter(function(element) {
      return element.tagName !== 'TEMPLATE';
    });

    // The section's main child should be stamped and visible.
    const main = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') === 'default';
    });
    assertEquals(
        1, main.length,
        'default card not found for section ' + section.section);
    assertGT(main[0]!.offsetHeight, 0);

    // Any other stamped subpages should not be visible.
    const subpages = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') !== 'default';
    });
    for (const subpage of subpages) {
      assertEquals(
          0, subpage.offsetHeight,
          'Expected subpage #' + subpage.id + ' in ' + section.section +
              ' not to be visible.');
    }
  }

  test('Basic sections', function() {
    const sectionNames = [
      'internet',
      'bluetooth',
      'multidevice',
      'osPeople',
      'kerberos',
      'device',
      'personalization',
      'osSearch',
      'apps',
    ];

    for (const name of sectionNames) {
      const section =
          settingsPage!.shadowRoot!.querySelector<OsSettingsSectionElement>(
              `os-settings-section[section=${name}]`);
      assertTrue(!!section, 'Did not find ' + name);
      verifySubpagesHidden(section);
    }
  });

  test('AdvancedSections', async function() {
    // Open the Advanced section.
    assert(settingsMain);
    settingsMain.advancedToggleExpanded = true;
    flush();
    await flushTasks();

    const sectionNames = [
      'osPrivacy',
      'osLanguages',
      'files',
      'osReset',
      'dateTime',
      'osAccessibility',
    ];

    for (const name of sectionNames) {
      const section =
          settingsPage!.shadowRoot!.querySelector<OsSettingsSectionElement>(
              `os-settings-section[section=${name}]`);
      assertTrue(!!section, 'Did not find ' + name);
      verifySubpagesHidden(section);
    }
  });

  test('Guest mode', async function() {
    // Simulate guest mode.
    settingsPage!.set('isGuestMode_', true);

    // Ensure Advanced is open.
    settingsMain!.advancedToggleExpanded = true;
    flush();
    await flushTasks();

    const hiddenSections = ['multidevice', 'osPeople', 'personalization'];
    for (const name of hiddenSections) {
      const section =
          settingsPage!.shadowRoot!.querySelector<OsSettingsSectionElement>(
              `os-settings-section[section=${name}]`);
      assertEquals(null, section, 'Found unexpected section ' + name);
    }

    const visibleSections = [
      'internet',
      'bluetooth',
      'kerberos',
      'device',
      'osSearch',
      'apps',
      'osPrivacy',
      'osLanguages',
      'files',
      'osReset',
      'dateTime',
      'osAccessibility',
    ];
    for (const name of visibleSections) {
      const section =
          settingsPage!.shadowRoot!.querySelector<OsSettingsSectionElement>(
              `os-settings-section[section=${name}]`);
      assertTrue(!!section, 'Expected section ' + name);
    }
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
