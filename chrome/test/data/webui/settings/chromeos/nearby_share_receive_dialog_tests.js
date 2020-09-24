// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals} from '../../chai_assert.js';
// #import {isChildVisible, waitAfterNextRender} from '../../test_util.m.js';
// #import {setReceiveManagerForTesting, setContactManagerForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {FakeContactManager} from '../../nearby_share/shared/fake_nearby_contact_manager.m.js';
// #import {FakeReceiveManager} from './fake_receive_manager.m.js'
// clang-format on

suite('NearbyShare', function() {
  /** @type {!NearbyShareReceiveDialogElement} */
  let dialog;
  /** @type {!FakeReceiveManager} */
  let fakeReceiveManager;
  /** @type {!nearby_share.FakeContactManager} */
  let fakeContactManager;

  /**
   * This allows both sub-suites to share the same setup logic but with a
   * different enabled state which changes the routing of the first view.
   * @param {boolean} enabled The value of the enabled setting.
   */
  function sharedSetup(enabled) {
    fakeReceiveManager = new nearby_share.FakeReceiveManager();
    fakeContactManager = new nearby_share.FakeContactManager();

    nearby_share.setReceiveManagerForTesting(fakeReceiveManager);
    nearby_share.setContactManagerForTesting(fakeContactManager);

    PolymerTest.clearBody();

    dialog = document.createElement('nearby-share-receive-dialog');
    dialog.settings = {
      enabled: enabled,
    };
    document.body.appendChild(dialog);
    Polymer.dom.flush();
  }

  /**
   * @param {string} selector
   * @return {boolean} Returns true if the element is visible in the shadow dom.
   */
  function isVisible(selector) {
    return test_util.isChildVisible(dialog, selector, false);
  }

  /**
   *
   * @param {string} page page element name
   * @param {*} button button selector (i.e. #actionButton)
   */
  function getButton(page, button) {
    return dialog.$$(page).$$('nearby-page-template').$$(button);
  }

  suite('EnabledTests', function() {
    setup(function() {
      sharedSetup(true);
      dialog.showHighVisibilityPage();
      Polymer.dom.flush();
    });

    teardown(function() {
      dialog.remove();
    });

    test('show high visibility page, get a target, accept', async function() {
      // When attached we enter high visibility mode by default
      assertTrue(isVisible('nearby-share-high-visibility-page'));
      assertFalse(isVisible('nearby-share-confirm-page'));
      // If a share target comes in, we show it.
      const target =
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
      const confirmPage = dialog.$$('nearby-share-confirm-page');
      Polymer.dom.flush();

      assertEquals(
          target.name, confirmPage.$$('#shareTargetName').textContent.trim());
      assertEquals(
          '1234', confirmPage.$$('#connectionToken').textContent.trim());

      confirmPage.$$('nearby-page-template').$$('#actionButton').click();
      const shareTargetId = await fakeReceiveManager.whenCalled('accept');
      assertEquals(target.id, shareTargetId);
    });

    test('show high visibility page, get a target, reject', async function() {
      // When attached we enter high visibility mode by default
      assertTrue(isVisible('nearby-share-high-visibility-page'));
      assertFalse(isVisible('nearby-share-confirm-page'));
      // If a share target comes in, we show it.
      const target =
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
      const confirmPage = dialog.$$('nearby-share-confirm-page');
      Polymer.dom.flush();

      assertEquals(
          target.name, confirmPage.$$('#shareTargetName').textContent.trim());
      assertEquals(
          '1234', confirmPage.$$('#connectionToken').textContent.trim());

      confirmPage.$$('nearby-page-template').$$('#cancelButton').click();
      const shareTargetId = await fakeReceiveManager.whenCalled('reject');
      assertEquals(target.id, shareTargetId);
    });

    test(
        'show high visibility page, exitHighVisibility, closes dialog',
        async function() {
          // When attached we enter high visibility mode by default
          assertTrue(isVisible('nearby-share-high-visibility-page'));
          assertFalse(isVisible('nearby-share-confirm-page'));
          // If a share target comes in, we show it.
          await fakeReceiveManager.exitHighVisibility();
          Polymer.dom.flush();
          assertFalse(isVisible('cr-dialog'));
        });
  });

  suite('DisabledTests', function() {
    setup(function() {
      sharedSetup(false);
    });

    teardown(function() {
      dialog.remove();
    });

    test('when disabled, onboarding is shown first', async function() {
      dialog.showHighVisibilityPage();
      await test_util.waitAfterNextRender(dialog);

      assertTrue(isVisible('nearby-onboarding-page'));
      // Advance to the next page.
      getButton('nearby-onboarding-page', '#actionButton').click();

      await test_util.waitAfterNextRender(dialog);

      assertTrue(isVisible('nearby-visibility-page'));
      // Advance to the next page.
      getButton('nearby-visibility-page', '#actionButton').click();

      await test_util.waitAfterNextRender(dialog);

      assertTrue(dialog.settings.enabled);
      assertTrue(isVisible('nearby-share-high-visibility-page'));
    });

    test('when showing onboarding, close when complete.', async function() {
      dialog.showOnboarding();
      await test_util.waitAfterNextRender(dialog);

      assertTrue(isVisible('nearby-onboarding-page'));
      // Advance to the next page.
      getButton('nearby-onboarding-page', '#actionButton').click();

      await test_util.waitAfterNextRender(dialog);

      assertTrue(isVisible('nearby-visibility-page'));
      // This should close the dialog.
      getButton('nearby-visibility-page', '#actionButton').click();

      assertTrue(dialog.closing_);

      await test_util.waitAfterNextRender(dialog);

      assertFalse(dialog.$$('#dialog').open);
    });
  });
});
