// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('split_settings_flag', function() {
  suite('splitSettingsFlag', function() {
    let browserSettings = null;

    /**
     * Names of settings sections that affect Chrome browser (and possibly CrOS)
     * and therefore should appear in browser settings even when SettingsSplit
     * feature is enabled.
     * @type {!Array<string>}
     */
    const BROWSER_SETTINGS_SECTIONS = [
      'a11y',
      'appearance',
      'autofill',
      'downloads',
      'languages',
      'onStartup',
      'people',
      'printing',
      'privacy',
      'reset',
      'search',
    ];

    setup(async function() {
      PolymerTest.clearBody();
      browserSettings = document.createElement('settings-basic-page');
      // In prod, page visibility is passed via Polymer binding layers but it
      // is always set to settings.pageVisibility.
      browserSettings.pageVisibility = settings.pageVisibility;
      document.body.appendChild(browserSettings);
      Polymer.dom.flush();

      // Expand <settings-idle-load> containing advanced section
      await browserSettings.$$('#advancedPageTemplate').get();
    });

    test('Attached sections are exactly browser settings sections', function() {
      const unattachedBrowserSettingsSections =
          new Set(BROWSER_SETTINGS_SECTIONS);
      const allAttachedSettingsSectionElements =
          browserSettings.shadowRoot.querySelectorAll('settings-section');
      for (const element of allAttachedSettingsSectionElements) {
        assertTrue(unattachedBrowserSettingsSections.delete(element.section));
      }
      assertEquals(unattachedBrowserSettingsSections.size, 0);
    });

    test('Assistant is hidden in browser search settings', function() {
      const searchSection =
          browserSettings.$$('settings-section[section=search]');
      assertTrue(!!searchSection);
      const searchPage = searchSection.querySelector('settings-search-page');
      assertTrue(!!searchPage);
      assertTrue(!!searchPage.$.enginesSubpageTrigger);
      assertFalse(!!searchPage.$.assistantSubpageTrigger);
    });
  });
});
