// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for page availability in the CrOS Settings UI.
 * Separated into a separate file to mitigate test timeouts.
 */

import {CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsUiElement, PageDisplayerElement, routesMojom} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertGT, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

const {Section} = routesMojom;
type PageName = keyof typeof Section;

suite('<os-settings-ui> page availability', () => {
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement;
  let mainPageContainer: MainPageContainerElement;

  async function createUi() {
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    await CrSettingsPrefs.initialized;
    const mainElement = ui.shadowRoot!.querySelector('os-settings-main');
    assert(mainElement);
    settingsMain = mainElement;

    const pageElement =
        settingsMain.shadowRoot!.querySelector('main-page-container');
    assert(pageElement);
    mainPageContainer = pageElement;

    const idleRender =
        mainPageContainer.shadowRoot!.querySelector('settings-idle-load');
    assert(idleRender);
    await idleRender.get();
    flush();
  }

  /**
   * Verifies the section has a visible #main element and that any possible
   * sub-pages are hidden.
   */
  function verifySubpagesHidden(page: PageDisplayerElement): void {
    // Check if there are any sub-pages to verify, being careful to filter out
    // any dom-if and template noise when we search.
    const pages = page.firstElementChild!.shadowRoot!.querySelector(
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
        1, main.length, 'default card not found for section ' + page.section);
    assertGT(main[0]!.offsetHeight, 0);

    // Any other stamped subpages should not be visible.
    const subpages = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') !== 'default';
    });
    for (const subpage of subpages) {
      assertEquals(
          0, subpage.offsetHeight,
          'Expected subpage #' + subpage.id + ' in ' + page.section +
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

    const availablePages: PageName[] = [
      'kAccessibility',
      'kApps',
      'kBluetooth',
      'kCrostini',
      'kDateAndTime',
      'kDevice',
      'kFiles',
      'kKerberos',
      'kMultiDevice',
      'kLanguagesAndInput',
      'kNetwork',
      'kPeople',
      'kPersonalization',
      'kPrinting',
      'kPrivacyAndSecurity',
      'kReset',
      'kSearchAndAssistant',
    ];
    for (const pageName of availablePages) {
      test(`${pageName} page should be stamped and subpages hidden`, () => {
        const page =
            mainPageContainer.shadowRoot!.querySelector<PageDisplayerElement>(
                `page-displayer[section="${Section[pageName]}"]`);
        assertTrue(!!page, `Expected to find ${pageName} page stamped.`);
        verifySubpagesHidden(page);
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

    const unavailablePages: PageName[] = [
      'kFiles',
      'kMultiDevice',
      'kPeople',
      'kPersonalization',
    ];
    for (const pageName of unavailablePages) {
      test(`${pageName} page should not be stamped`, () => {
        const section =
            mainPageContainer.shadowRoot!.querySelector<PageDisplayerElement>(
                `page-displayer[section="${Section[pageName]}"]`);
        assertNull(section, `Found unexpected page ${pageName}.`);
      });
    }

    const availablePages: PageName[] = [
      'kAccessibility',
      'kApps',
      'kBluetooth',
      'kDateAndTime',
      'kDevice',
      'kKerberos',
      'kLanguagesAndInput',
      'kNetwork',
      'kPrivacyAndSecurity',
      'kReset',
      'kSearchAndAssistant',
    ];
    for (const pageName of availablePages) {
      test(`${pageName} page should be stamped and subpages hidden`, () => {
        const page =
            mainPageContainer.shadowRoot!.querySelector<PageDisplayerElement>(
                `page-displayer[section="${Section[pageName]}"]`);
        assertTrue(!!page, `Expected to find ${pageName} page stamped.`);
        verifySubpagesHidden(page);
      });
    }
  });
});
