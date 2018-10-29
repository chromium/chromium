// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_personalization_options', function() {
  suite('SafeBrowsingExtendedReportingOfficialBuild', function() {
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

    test('displaying toggles depending on unified consent', function() {
      testElement.unifiedConsentEnabled = false;
      Polymer.dom.flush();
      assertEquals(
          7,
          testElement.root.querySelectorAll('settings-toggle-button').length);
      testElement.unifiedConsentEnabled = true;
      Polymer.dom.flush();
      assertEquals(
          8,
          testElement.root.querySelectorAll('settings-toggle-button').length);
    });

    test('hide spellcheck toggle when there is no dictionary', function() {
      testElement.unifiedConsentEnabled = true;
      testElement.prefs = {spellcheck: {dictionaries: {value: ['en-US']}}};
      Polymer.dom.flush();
      assertFalse(testElement.$.spellCheckControl.hidden);

      testElement.prefs = {spellcheck: {dictionaries: {value: []}}};
      Polymer.dom.flush();
      assertTrue(testElement.$.spellCheckControl.hidden);
    });
  });
});
