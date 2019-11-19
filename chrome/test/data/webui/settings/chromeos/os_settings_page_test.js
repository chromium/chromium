// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the OS Settings main page. */

suite('OSSettingsPage', function() {
  let settingsMain = null;
  let settingsPage = null;

  suiteSetup(async function() {
    await CrSettingsPrefs.initialized;

    settingsMain =
        document.querySelector('os-settings-ui').$$('os-settings-main');
    assert(!!settingsMain);

    settingsPage = settingsMain.$$('os-settings-page');
    assertTrue(!!settingsPage);

    const idleRender =
        settingsMain.$$('os-settings-page').$$('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    Polymer.dom.flush();
  });

  /**
   * Verifies the section has a visible #main element and that any possible
   * sub-pages are hidden.
   * @param {!Node} The DOM node for the section.
   */
  function verifySubpagesHidden(section) {
    // Check if there are sub-pages to verify.
    const pages = section.firstElementChild.shadowRoot.querySelector(
        'settings-animated-pages');
    if (!pages) {
      return;
    }

    const children = pages.getContentChildren();
    const stampedChildren = children.filter(function(element) {
      return element.tagName != 'TEMPLATE';
    });

    // The section's main child should be stamped and visible.
    const main = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') == 'default';
    });
    assertEquals(
        main.length, 1,
        'default card not found for section ' + section.section);
    assertGT(main[0].offsetHeight, 0);

    // Any other stamped subpages should not be visible.
    const subpages = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') != 'default';
    });
    for (const subpage of subpages) {
      assertEquals(
          subpage.offsetHeight, 0,
          'Expected subpage #' + subpage.id + ' in ' + section.section +
              ' not to be visible.');
    }
  }

  test('Basic sections', function() {
    const sectionNames = [
      'internet', 'bluetooth', 'multidevice', 'people', 'device',
      'personalization', 'search', 'apps'
    ];

    for (const name of sectionNames) {
      const section = settingsPage.shadowRoot.querySelector(
          `settings-section[section=${name}]`);
      assertTrue(!!section, 'Did not find ' + name);
      verifySubpagesHidden(section);
    }
  });

  test('AdvancedSections', async function() {
    // Open the Advanced section.
    settingsMain.advancedToggleExpanded = true;
    Polymer.dom.flush();
    await test_util.flushTasks();

    const sectionNames =
        ['privacy', 'languages', 'files', 'reset', 'dateTime', 'a11y'];

    for (const name of sectionNames) {
      const section = settingsPage.shadowRoot.querySelector(
          `settings-section[section=${name}]`);
      assertTrue(!!section, 'Did not find ' + name);
      verifySubpagesHidden(section);
    }
  });

  test('Guest mode', async function() {
    // Simulate guest mode.
    settingsPage.isGuestMode_ = true;

    // Ensure Advanced is open.
    settingsMain.advancedToggleExpanded = true;
    Polymer.dom.flush();
    await test_util.flushTasks();

    const hiddenSections = ['multidevice', 'people', 'personalization'];
    for (const name of hiddenSections) {
      const section = settingsPage.shadowRoot.querySelector(
          `settings-section[section=${name}]`);
      assertFalse(!!section, 'Found unexpected section ' + name);
    }

    const visibleSections = [
      'internet', 'bluetooth', 'device', 'search', 'apps', 'privacy',
      'languages', 'files', 'reset', 'dateTime', 'a11y'
    ];
    for (const name of visibleSections) {
      const section = settingsPage.shadowRoot.querySelector(
          `settings-section[section=${name}]`);
      assertTrue(!!section, 'Expected section ' + name);
    }
  });
});
