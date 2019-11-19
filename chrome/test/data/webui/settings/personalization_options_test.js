// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_personalization_options', function() {
  suite('PersonalizationOptionsTests_AllBuilds', function() {
    /** @type {settings.TestPrivacyPageBrowserProxy} */
    let testBrowserProxy;

    /** @type {SettingsPersonalizationOptionsElement} */
    let testElement;

    suiteSetup(function() {
      loadTimeData.overrideValues({
        driveSuggestAvailable: true,
      });
    });

    setup(function() {
      testBrowserProxy = new TestPrivacyPageBrowserProxy();
      settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
      PolymerTest.clearBody();
      testElement = document.createElement('settings-personalization-options');
      document.body.appendChild(testElement);
      Polymer.dom.flush();
    });

    teardown(function() {
      testElement.remove();
    });

    test('DriveSearchSuggestControl', function() {
      assertFalse(!!testElement.$$('#driveSuggestControl'));

      testElement.syncStatus = {
        signedIn: true,
        statusAction: settings.StatusAction.NO_ACTION
      };
      Polymer.dom.flush();
      assertTrue(!!testElement.$$('#driveSuggestControl'));

      testElement.syncStatus = {
        signedIn: true,
        statusAction: settings.StatusAction.REAUTHENTICATE
      };
      Polymer.dom.flush();
      assertFalse(!!testElement.$$('#driveSuggestControl'));
    });
  });

  suite('PersonalizationOptionsTests_OfficialBuild', function() {
    /** @type {settings.TestPrivacyPageBrowserProxy} */
    let testBrowserProxy;

    /** @type {SettingsPersonalizationOptionsElement} */
    let testElement;

    setup(function() {
      testBrowserProxy = new TestPrivacyPageBrowserProxy();
      settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
      PolymerTest.clearBody();
      testElement = document.createElement('settings-personalization-options');
      document.body.appendChild(testElement);
    });

    teardown(function() {
      testElement.remove();
    });

    test('Spellcheck toggle', function() {
      testElement.prefs = {spellcheck: {dictionaries: {value: ['en-US']}}};
      Polymer.dom.flush();
      assertFalse(testElement.$.spellCheckControl.hidden);

      testElement.prefs = {spellcheck: {dictionaries: {value: []}}};
      Polymer.dom.flush();
      assertTrue(testElement.$.spellCheckControl.hidden);

      testElement.prefs = {
        browser: {enable_spellchecking: {value: false}},
        spellcheck: {
          dictionaries: {value: ['en-US']},
          use_spelling_service: {value: false}
        }
      };
      Polymer.dom.flush();
      testElement.$.spellCheckControl.click();
      assertTrue(testElement.prefs.spellcheck.use_spelling_service.value);
    });
  });
});
