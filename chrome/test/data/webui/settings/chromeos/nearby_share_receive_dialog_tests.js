// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals} from '../../chai_assert.js';
// #import {isChildVisible, waitAfterNextRender} from '../../test_util.m.js';
// #import {setNearbyShareSettingsForTesting, setReceiveManagerForTesting, setContactManagerForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {FakeContactManager} from '../../nearby_share/shared/fake_nearby_contact_manager.m.js';
// #import {FakeNearbyShareSettings} from '../../nearby_share/shared/fake_nearby_share_settings.m.js';
// #import {FakeReceiveManager} from './fake_receive_manager.m.js'
// clang-format on

suite('NearbyShare', function() {
  /** @type {!NearbyShareReceiveDialogElement} */
  let dialog;
  /** @type {!FakeReceiveManager} */
  let fakeReceiveManager;
  /** @type {!nearby_share.FakeContactManager} */
  let fakeContactManager;
  /** @type {!nearby_share.FakeNearbyShareSettings} */
  let fakeSettings;

  /**
   * This allows both sub-suites to share the same setup logic but with a
   * different enabled state which changes the routing of the first view.
   * @param {boolean} enabled The value of the enabled setting.
   */
  function sharedSetup(enabled) {
    fakeReceiveManager = new nearby_share.FakeReceiveManager();
    fakeContactManager = new nearby_share.FakeContactManager();
    fakeSettings = new nearby_share.FakeNearbyShareSettings();
    fakeSettings.setEnabled(true);

    nearby_share.setReceiveManagerForTesting(fakeReceiveManager);
    nearby_share.setContactManagerForTesting(fakeContactManager);
    nearby_share.setNearbyShareSettingsForTesting(fakeSettings);

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

  function selectAllContacts() {
    dialog.$$('nearby-visibility-page')
        .$$('nearby-contact-visibility')
        .$$('#allContacts')
        .click();
  }

  suite('EnabledTests', function() {
    setup(function() {
      sharedSetup(true);
      dialog.showHighVisibilityPage(/*shutoffTimeoutInSeconds=*/ 5 * 60);
      Polymer.dom.flush();
    });

    teardown(function() {
      dialog.remove();
    });

    test('show high visibility page, get a target, accept', async function() {
      await test_util.waitAfterNextRender(dialog);
      // When attached we enter high visibility mode by default
      assertTrue(isVisible('nearby-share-high-visibility-page'));
      assertFalse(isVisible('nearby-share-confirm-page'));
      // If a share target comes in, we show it.
      const target =
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
      const confirmPage = dialog.$$('nearby-share-confirm-page');
      Polymer.dom.flush();

      const progressIcon = confirmPage.$$('#progressIcon');
      assertTrue(!!progressIcon.shareTarget);
      assertEquals(target.name, progressIcon.shareTarget.name);
      assertTrue(
          confirmPage.$$('#connectionToken').textContent.includes('1234'));
      assertTrue(test_util.isChildVisible(confirmPage, 'nearby-preview'));

      confirmPage.$$('nearby-page-template').$$('#actionButton').click();
      const shareTargetId = await fakeReceiveManager.whenCalled('accept');
      assertEquals(target.id, shareTargetId);
    });

    test('show high visibility page, get a target, reject', async function() {
      await test_util.waitAfterNextRender(dialog);
      // When attached we enter high visibility mode by default
      assertTrue(isVisible('nearby-share-high-visibility-page'));
      assertFalse(isVisible('nearby-share-confirm-page'));
      // If a share target comes in, we show it.
      const target =
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
      const confirmPage = dialog.$$('nearby-share-confirm-page');
      Polymer.dom.flush();

      const progressIcon = confirmPage.$$('#progressIcon');
      assertTrue(!!progressIcon.shareTarget);
      assertEquals(target.name, progressIcon.shareTarget.name);
      assertTrue(
          confirmPage.$$('#connectionToken').textContent.includes('1234'));
      assertTrue(test_util.isChildVisible(confirmPage, 'nearby-preview'));

      confirmPage.$$('nearby-page-template').$$('#cancelButton').click();
      const shareTargetId = await fakeReceiveManager.whenCalled('reject');
      assertEquals(target.id, shareTargetId);
    });

    test(
        'show high visibility page, unregister surface, closes dialog',
        async function() {
          await test_util.waitAfterNextRender(dialog);
          // When attached we enter high visibility mode by default
          assertTrue(isVisible('nearby-share-high-visibility-page'));
          assertFalse(isVisible('nearby-share-confirm-page'));
          // If a share target comes in, we show it.
          await fakeReceiveManager.unregisterForegroundReceiveSurface();
          Polymer.dom.flush();
          assertTrue(dialog.closing_);
          assertFalse(isVisible('cr-dialog'));
        });

    test(
        'OnTransferUpdate, unregister surface, does not close dialog',
        async function() {
          await test_util.waitAfterNextRender(dialog);
          // When attached we enter high visibility mode by default
          assertTrue(isVisible('nearby-share-high-visibility-page'));
          assertFalse(isVisible('nearby-share-confirm-page'));
          // If a share target comes in, we show it.
          const target =
              fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
          Polymer.dom.flush();
          assertFalse(dialog.closing_);
          await fakeReceiveManager.unregisterForegroundReceiveSurface();
          Polymer.dom.flush();
          assertFalse(dialog.closing_);
        });

    test('onStartAdvertisingFailure shows an error', async function() {
      await test_util.waitAfterNextRender(dialog);
      assertTrue(isVisible('nearby-share-high-visibility-page'));
      const highVisibilityPage = dialog.$$('nearby-share-high-visibility-page');
      assertFalse(!!highVisibilityPage.$$('#errorTitle'));

      dialog.onStartAdvertisingFailure();
      await test_util.waitAfterNextRender(dialog);

      const errorTitle = highVisibilityPage.$$('#errorTitle');
      assertTrue(!!errorTitle && errorTitle.textContent.length > 0);
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
      fakeContactManager.completeDownload();
      selectAllContacts();
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
      fakeContactManager.completeDownload();
      selectAllContacts();
      getButton('nearby-visibility-page', '#actionButton').click();

      assertTrue(dialog.closing_);

      await test_util.waitAfterNextRender(dialog);

      assertFalse(dialog.$$('#dialog').open);
    });
  });
});
