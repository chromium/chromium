// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

/** @fileoverview Suite of tests for the OS Settings ui and main page. */

suite('OSSettingsUi', function() {
  let settingsMain = null;
  let settingsPage = null;
  let ui;

  suiteSetup(async function() {
    document.body.innerHTML = '';
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    Polymer.dom.flush();

    await CrSettingsPrefs.initialized;
    settingsMain =
        document.querySelector('os-settings-ui').$$('os-settings-main');

    settingsMain =
        document.querySelector('os-settings-ui').$$('os-settings-main');
    assert(!!settingsMain);

    settingsPage = settingsMain.$$('os-settings-page');
    assertTrue(!!settingsPage);

    // Simulate Kerberos settings section enabled.
    settingsPage.showKerberosSection = true;

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
      return element.tagName !== 'TEMPLATE';
    });

    // The section's main child should be stamped and visible.
    const main = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') === 'default';
    });
    assertEquals(
        main.length, 1,
        'default card not found for section ' + section.section);
    assertGT(main[0].offsetHeight, 0);

    // Any other stamped subpages should not be visible.
    const subpages = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') !== 'default';
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
      'internet', 'bluetooth', 'multidevice', 'osPeople', 'kerberos', 'device',
      'personalization', 'osSearch', 'apps'
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

    const sectionNames = [
      'osPrivacy', 'osLanguages', 'files', 'osReset', 'dateTime',
      'osAccessibility'
    ];

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

    const hiddenSections = ['multidevice', 'osPeople', 'personalization'];
    for (const name of hiddenSections) {
      const section = settingsPage.shadowRoot.querySelector(
          `settings-section[section=${name}]`);
      assertFalse(!!section, 'Found unexpected section ' + name);
    }

    const visibleSections = [
      'internet', 'bluetooth', 'kerberos', 'device', 'osSearch', 'apps',
      'osPrivacy', 'osLanguages', 'files', 'osReset', 'dateTime',
      'osAccessibility'
    ];
    for (const name of visibleSections) {
      const section = settingsPage.shadowRoot.querySelector(
          `settings-section[section=${name}]`);
      assertTrue(!!section, 'Expected section ' + name);
    }
  });

  test('Update required end of life banner visibility', function() {
    Polymer.dom.flush();
    assertFalse(settingsPage.showUpdateRequiredEolBanner_);
    assertFalse(!!settingsPage.$$('#updateRequiredEolBanner'));

    settingsPage.showUpdateRequiredEolBanner_ = true;
    Polymer.dom.flush();
    assertTrue(!!settingsPage.$$('#updateRequiredEolBanner'));
  });

  test('Update required end of life banner close button click', function() {
    settingsPage.showUpdateRequiredEolBanner_ = true;
    Polymer.dom.flush();
    const banner = settingsPage.$$('#updateRequiredEolBanner');
    assertTrue(!!banner);

    const closeButton = assert(settingsPage.$$('#closeUpdateRequiredEol'));
    closeButton.click();
    Polymer.dom.flush();
    assertFalse(settingsPage.showUpdateRequiredEolBanner_);
    assertEquals('none', banner.style.display);
  });
});
