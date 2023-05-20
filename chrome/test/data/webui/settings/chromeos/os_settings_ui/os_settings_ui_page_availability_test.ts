// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for page availability in the CrOS Settings UI.
 * Separated into a separate file to mitigate test timeouts.
 */

import {CrSettingsPrefs, OsSettingsMainElement, OsSettingsPageElement, OsSettingsSectionElement, OsSettingsUiElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<os-settings-ui> page availability', () => {
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement;
  let settingsPage: OsSettingsPageElement;

  async function createUi() {
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    await CrSettingsPrefs.initialized;
    const mainElement = ui.shadowRoot!.querySelector('os-settings-main');
    assert(mainElement);
    settingsMain = mainElement;

    const pageElement =
        settingsMain.shadowRoot!.querySelector('os-settings-page');
    assert(pageElement);
    settingsPage = pageElement;

    const idleRender =
        settingsPage.shadowRoot!.querySelector('settings-idle-load');
    assert(idleRender);
    await idleRender.get();
    flush();
  }

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

  suite('For normal user', () => {
    suiteSetup(async () => {
      loadTimeData.overrideValues({
        isGuest: false,           // Default to normal user
        isKerberosEnabled: true,  // Simulate kerberos page available
        allowPowerwash: true,     // Simulate reset page available
      });
      await createUi();
    });

    suiteTeardown(() => {
      ui.remove();
    });

    const availablePages = [
      'apps',
      'bluetooth',
      'crostini',
      'dateTime',
      'device',
      'files',
      'internet',
      'kerberos',
      'multidevice',
      'osAccessibility',
      'osLanguages',
      'osPeople',
      'osPrinting',
      'osPrivacy',
      'osReset',
      'osSearch',
      'personalization',
    ];
    for (const name of availablePages) {
      test(`${name} page should be stamped and subpages hidden`, () => {
        const section =
            settingsPage.shadowRoot!.querySelector<OsSettingsSectionElement>(
                `os-settings-section[section=${name}]`);
        assertTrue(!!section, `Expected to find ${name} page stamped`);
        verifySubpagesHidden(section);
      });
    }
  });

  suite('For guest user', () => {
    suiteSetup(async () => {
      loadTimeData.overrideValues({
        isGuest: true,            // Simulate guest mode
        isKerberosEnabled: true,  // Simulate kerberos page available
        allowPowerwash: true,     // Simulate reset page available
      });
      await createUi();
    });

    suiteTeardown(() => {
      ui.remove();
    });

    const unavailablePages = [
      'files',
      'multidevice',
      'osPeople',
      'personalization',
    ];
    for (const name of unavailablePages) {
      test(`${name} page should not be stamped`, () => {
        const section =
            settingsPage.shadowRoot!.querySelector<OsSettingsSectionElement>(
                `os-settings-section[section=${name}]`);
        assertEquals(null, section, `Found unexpected page ${name}`);
      });
    }

    const availablePages = [
      'apps',
      'bluetooth',
      'dateTime',
      'device',
      'internet',
      'kerberos',
      'osAccessibility',
      'osLanguages',
      'osPrivacy',
      'osReset',
      'osSearch',
    ];
    for (const name of availablePages) {
      test(`${name} page should be stamped and subpages hidden`, () => {
        const section =
            settingsPage.shadowRoot!.querySelector<OsSettingsSectionElement>(
                `os-settings-section[section=${name}]`);
        assertTrue(!!section, `Expected to find ${name} page stamped`);
        verifySubpagesHidden(section);
      });
    }
  });
});
