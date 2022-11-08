// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {setContactManagerForTesting, setNearbyShareSettingsForTesting, setReceiveManagerForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {FakeReceiveManager} from './fake_receive_manager.js';

suite('NearbyShare', function() {
  /** @type {!NearbyShareReceiveDialogElement} */
  let dialog;
  /** @type {!FakeReceiveManager} */
  let fakeReceiveManager;
  /** @type {!FakeContactManager} */
  let fakeContactManager;
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings;

  /**
   * This allows both sub-suites to share the same setup logic but with a
   * different enabled state which changes the routing of the first view.
   * @param {boolean} enabled The value of the enabled setting.
   * @param {boolean} isOnboardingComplete The value of the
   *     isOnboardingComplete setting.
   */
  function sharedSetup(enabled, isOnboardingComplete) {
    fakeReceiveManager = new FakeReceiveManager();
    fakeContactManager = new FakeContactManager();
    fakeSettings = new FakeNearbyShareSettings();

    setReceiveManagerForTesting(fakeReceiveManager);
    setContactManagerForTesting(fakeContactManager);
    setNearbyShareSettingsForTesting(fakeSettings);

    PolymerTest.clearBody();
    fakeSettings.setEnabled(enabled);
    fakeSettings.setIsOnboardingComplete(isOnboardingComplete);

    dialog = document.createElement('nearby-share-receive-dialog');
    dialog.settings = {
      enabled: enabled,
      isOnboardingComplete: isOnboardingComplete,
      visibility: nearbyShare.mojom.Visibility.kUnknown,
    };
    dialog.isSettingsRetreived = true;
    document.body.appendChild(dialog);
    flush();
  }

  /**
   * @param {string} selector
   * @return {boolean} Returns true if the element is visible in the shadow dom.
   */
  function isVisible(selector) {
    return isChildVisible(dialog, selector, false);
  }

  /**
   *
   * @param {string} page page element name
   * @param {*} button button selector (i.e. #actionButton)
   */
  function getButton(page, button) {
    return dialog.shadowRoot.querySelector(page)
        .shadowRoot.querySelector('nearby-page-template')
        .shadowRoot.querySelector(button);
  }

  function selectAllContacts() {
    dialog.shadowRoot.querySelector('nearby-visibility-page')
        .shadowRoot.querySelector('nearby-contact-visibility')
        .shadowRoot.querySelector('#allContacts')
        .click();
  }

  suite('EnabledTests', function() {
    setup(function() {
      sharedSetup(/*enabled=*/ true, /*isOnboardingComplete=*/ true);
      dialog.showHighVisibilityPage(/*shutoffTimeoutInSeconds=*/ 5 * 60);
      flush();
    });

    teardown(function() {
      dialog.remove();
    });

    test('show high visibility page, get a target, accept', async function() {
      await waitAfterNextRender(dialog);
      // When attached we enter high visibility mode by default
      assertTrue(isVisible('nearby-share-high-visibility-page'));
      assertFalse(isVisible('nearby-share-confirm-page'));
      // If a share target comes in, we show it.
      const target =
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
      const confirmPage =
          dialog.shadowRoot.querySelector('nearby-share-confirm-page');
      flush();

      const progressIcon =
          confirmPage.shadowRoot.querySelector('#progressIcon');
      assertTrue(!!progressIcon.shareTarget);
      assertEquals(target.name, progressIcon.shareTarget.name);
      assertTrue(confirmPage.shadowRoot.querySelector('#connectionToken')
                     .textContent.includes('1234'));
      assertTrue(isChildVisible(confirmPage, 'nearby-preview'));

      confirmPage.shadowRoot.querySelector('nearby-page-template')
          .shadowRoot.querySelector('#actionButton')
          .click();
      const shareTargetId = await fakeReceiveManager.whenCalled('accept');
      assertEquals(target.id, shareTargetId);
    });

    test('show high visibility page, get a target, reject', async function() {
      await waitAfterNextRender(dialog);
      // When attached we enter high visibility mode by default
      assertTrue(isVisible('nearby-share-high-visibility-page'));
      assertFalse(isVisible('nearby-share-confirm-page'));
      // If a share target comes in, we show it.
      const target =
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
      const confirmPage =
          dialog.shadowRoot.querySelector('nearby-share-confirm-page');
      flush();

      const progressIcon =
          confirmPage.shadowRoot.querySelector('#progressIcon');
      assertTrue(!!progressIcon.shareTarget);
      assertEquals(target.name, progressIcon.shareTarget.name);
      assertTrue(confirmPage.shadowRoot.querySelector('#connectionToken')
                     .textContent.includes('1234'));
      assertTrue(isChildVisible(confirmPage, 'nearby-preview'));

      confirmPage.shadowRoot.querySelector('nearby-page-template')
          .shadowRoot.querySelector('#cancelButton')
          .click();
      const shareTargetId = await fakeReceiveManager.whenCalled('reject');
      assertEquals(target.id, shareTargetId);
    });

    test(
        'show high visibility page, unregister surface, closes dialog',
        async function() {
          await waitAfterNextRender(dialog);
          // When attached we enter high visibility mode by default
          assertTrue(isVisible('nearby-share-high-visibility-page'));
          assertFalse(isVisible('nearby-share-confirm-page'));
          // If a share target comes in, we show it.
          await fakeReceiveManager.unregisterForegroundReceiveSurface();
          flush();
          assertTrue(dialog.closing_);
          assertFalse(isVisible('cr-dialog'));
        });

    test(
        'OnTransferUpdate, unregister surface, does not close dialog',
        async function() {
          await waitAfterNextRender(dialog);
          // When attached we enter high visibility mode by default
          assertTrue(isVisible('nearby-share-high-visibility-page'));
          assertFalse(isVisible('nearby-share-confirm-page'));
          // If a share target comes in, we show it.
          const target =
              fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
          flush();
          assertFalse(dialog.closing_);
          await fakeReceiveManager.unregisterForegroundReceiveSurface();
          flush();
          assertFalse(dialog.closing_);
        });

    test('onStartAdvertisingFailure shows an error', async function() {
      await waitAfterNextRender(dialog);
      assertTrue(isVisible('nearby-share-high-visibility-page'));
      const highVisibilityPage =
          dialog.shadowRoot.querySelector('nearby-share-high-visibility-page');
      assertFalse(!!highVisibilityPage.shadowRoot.querySelector('#errorTitle'));

      dialog.onStartAdvertisingFailure();
      await waitAfterNextRender(dialog);

      const errorTitle =
          highVisibilityPage.shadowRoot.querySelector('#errorTitle');
      assertTrue(!!errorTitle && errorTitle.textContent.length > 0);
    });
  });

  suite('DisabledTests', function() {
    setup(function() {
      sharedSetup(/*enabled=*/ false, /*isOnboardingComplete=*/ false);
    });

    teardown(function() {
      dialog.remove();
    });

    test('when disabled, one-page onboarding is shown first', async function() {
      dialog.showHighVisibilityPage();
      await waitAfterNextRender(dialog);

      assertFalse(isVisible('nearby-onboarding-page'));
      assertTrue(isVisible('nearby-onboarding-one-page'));
      // Finish onboarding
      getButton('nearby-onboarding-one-page', '#actionButton').click();

      await waitAfterNextRender(dialog);

      assertTrue(dialog.settings.enabled);
      assertEquals(
          nearbyShare.mojom.Visibility.kAllContacts,
          dialog.settings.visibility);
      assertTrue(isVisible('nearby-share-high-visibility-page'));
    });

    test('when showing onboarding, close when complete.', async function() {
      dialog.showOnboarding();
      await waitAfterNextRender(dialog);

      assertFalse(isVisible('nearby-onboarding-page'));
      assertTrue(isVisible('nearby-onboarding-one-page'));
      // Select visibility button and advance to the next page.
      dialog.shadowRoot.querySelector('nearby-onboarding-one-page')
          .shadowRoot.querySelector('#visibilityButton')
          .click();

      await waitAfterNextRender(dialog);

      assertTrue(isVisible('nearby-visibility-page'));
      // All contacts should be selected and confirm should close the dialog.
      fakeContactManager.completeDownload();
      assertTrue(dialog.shadowRoot.querySelector('nearby-visibility-page')
                     .shadowRoot.querySelector('nearby-contact-visibility')
                     .shadowRoot.querySelector('#allContacts')
                     .checked);
      getButton('nearby-visibility-page', '#actionButton').click();

      assertTrue(dialog.closing_);

      await waitAfterNextRender(dialog);

      assertFalse(dialog.shadowRoot.querySelector('#dialog').open);
    });
  });
});
