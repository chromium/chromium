// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

/** @fileoverview Suite of tests for the OS Settings ui and main page. */

suite('OSSettingsUi', function() {
  let settingsMain = null;
  let settingsPage = null;
  let ui;

  suiteSetup(async function() {
    document.body.innerHTML = '';
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    await CrSettingsPrefs.initialized;
    settingsMain = document.querySelector('os-settings-ui')
                       .shadowRoot.querySelector('os-settings-main');

    settingsMain = document.querySelector('os-settings-ui')
                       .shadowRoot.querySelector('os-settings-main');
    assert(!!settingsMain);

    settingsPage = settingsMain.shadowRoot.querySelector('os-settings-page');
    assertTrue(!!settingsPage);

    // Simulate Kerberos enabled.
    settingsPage.showKerberosSection = true;

    const idleRender = settingsMain.shadowRoot.querySelector('os-settings-page')
                           .shadowRoot.querySelector('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    flush();
  });

  /**
   * Verifies the section has a visible #main element and that any possible
   * sub-pages are hidden.
   * @param {!Node} The DOM node for the section.
   */
  function verifySubpagesHidden(section) {
    // Check if there are any sub-pages to verify, being careful to filter out
    // any dom-if and template noise when we search.
    const pages = section.querySelector(`:not(dom-if, template)`)
                      .shadowRoot.querySelector('os-settings-animated-pages');
    if (!pages) {
      return;
    }

    const children = pages.shadowRoot.querySelector('slot')
                         .assignedNodes({flatten: true})
                         .filter(n => n.nodeType === Node.ELEMENT_NODE);
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
      const section = settingsPage.shadowRoot.querySelector(
          `os-settings-section[section=${name}]`);
      assertTrue(!!section, 'Did not find ' + name);
      verifySubpagesHidden(section);
    }
  });

  test('AdvancedSections', async function() {
    // Open the Advanced section.
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
      const section = settingsPage.shadowRoot.querySelector(
          `os-settings-section[section=${name}]`);
      assertTrue(!!section, 'Did not find ' + name);
      verifySubpagesHidden(section);
    }
  });

  test('Guest mode', async function() {
    // Simulate guest mode.
    settingsPage.isGuestMode_ = true;

    // Ensure Advanced is open.
    settingsMain.advancedToggleExpanded = true;
    flush();
    await flushTasks();

    const hiddenSections = ['multidevice', 'osPeople', 'personalization'];
    for (const name of hiddenSections) {
      const section = settingsPage.shadowRoot.querySelector(
          `os-settings-section[section=${name}]`);
      assertFalse(!!section, 'Found unexpected section ' + name);
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
      const section = settingsPage.shadowRoot.querySelector(
          `os-settings-section[section=${name}]`);
      assertTrue(!!section, 'Expected section ' + name);
    }
  });

  test('Update required end of life banner visibility', function() {
    flush();
    assertFalse(settingsPage.showUpdateRequiredEolBanner_);
    assertFalse(
        !!settingsPage.shadowRoot.querySelector('#updateRequiredEolBanner'));

    settingsPage.showUpdateRequiredEolBanner_ = true;
    flush();
    assertTrue(
        !!settingsPage.shadowRoot.querySelector('#updateRequiredEolBanner'));
  });

  test('Update required end of life banner close button click', function() {
    settingsPage.showUpdateRequiredEolBanner_ = true;
    flush();
    const banner =
        settingsPage.shadowRoot.querySelector('#updateRequiredEolBanner');
    assertTrue(!!banner);

    const closeButton = assert(
        settingsPage.shadowRoot.querySelector('#closeUpdateRequiredEol'));
    closeButton.click();
    flush();
    assertFalse(settingsPage.showUpdateRequiredEolBanner_);
    assertEquals('none', banner.style.display);
  });

  test('clicking icon closes drawer', async () => {
    flush();
    const drawer = ui.shadowRoot.querySelector('#drawer');
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);

    // Clicking the drawer icon closes the drawer.
    ui.shadowRoot.querySelector('#iconButton').click();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
    assertTrue(drawer.wasCanceled());
  });
});
