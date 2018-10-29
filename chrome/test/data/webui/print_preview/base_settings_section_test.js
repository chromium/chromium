// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('base_settings_section_test', function() {
  /** @enum {string} */
  const TestNames = {
    ManagedShowsEnterpriseIcon: 'managed shows enterprise icon',
  };

  const suiteName = 'BaseSettingsSectionTest';
  suite(suiteName, function() {
    let settingsSection = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      settingsSection =
          document.createElement('print-preview-settings-section');
      document.body.appendChild(settingsSection);
      Polymer.dom.flush();
    });

    // Test that key events that would result in invalid values are blocked.
    test(assert(TestNames.ManagedShowsEnterpriseIcon), function() {
      // No icons until managed is set.
      let icons = settingsSection.shadowRoot.querySelectorAll('iron-icon');
      assertEquals(0, icons.length);
      assertFalse(settingsSection.managed);

      // Simulate setting the section to managed and verify icon appears.
      settingsSection.managed = true;
      Polymer.dom.flush();
      icons = settingsSection.shadowRoot.querySelectorAll('iron-icon');
      assertEquals(1, icons.length);
      assertFalse(icons[0].hidden);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
