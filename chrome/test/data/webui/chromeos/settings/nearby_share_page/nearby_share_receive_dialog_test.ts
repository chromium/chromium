// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {NearbyShareReceiveDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrCardRadioButtonElement, CrDialogElement, NearbyProgressElement, setContactManagerForTesting, setNearbyShareSettingsForTesting, setReceiveManagerForTesting} from 'chrome://os-settings/os_settings.js';
import {DataUsage, FastInitiationNotificationState, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeReceiveManager} from '../fake_receive_manager.js';

suite('<nearby-share-receive-dialog>', () => {
  let dialog: NearbyShareReceiveDialogElement;
  let fakeReceiveManager: FakeReceiveManager;
  let fakeContactManager: FakeContactManager;
  let fakeSettings: FakeNearbyShareSettings;

  /**
   * This allows both sub-suites to share the same setup logic but with a
   * different enabled state which changes the routing of the first view.
   * @param enabled The value of the enabled setting.
   * @param isOnboardingComplete The value of the
   *     isOnboardingComplete setting.
   */
  function sharedSetup(enabled: boolean, isOnboardingComplete: boolean): void {
    fakeReceiveManager = new FakeReceiveManager();
    fakeContactManager = new FakeContactManager();
    fakeSettings = new FakeNearbyShareSettings();

    setReceiveManagerForTesting(fakeReceiveManager);
    setContactManagerForTesting(fakeContactManager);
    setNearbyShareSettingsForTesting(fakeSettings);

    fakeSettings.setEnabled(enabled);
    fakeSettings.setIsOnboardingComplete(isOnboardingComplete);

    dialog = document.createElement('nearby-share-receive-dialog');
    dialog.settings = {
      enabled,
      isOnboardingComplete,
      visibility: Visibility.kUnknown,
      fastInitiationNotificationState:
          FastInitiationNotificationState.MIN_VALUE,
      isFastInitiationHardwareSupported: false,
      deviceName: '',
      dataUsage: DataUsage.kUnknown,
      allowedContacts: [],
    };
    dialog.isSettingsRetreived = true;
    document.body.appendChild(dialog);
    flush();
  }

  /**
   * @param page page element name
   * @param button button selector (i.e. #actionButton)
   */
  function getButton(page: string, button: string): HTMLButtonElement {
    const element = dialog.shadowRoot!.querySelector(page);
    assertTrue(!!element);
    const template = element.shadowRoot!.querySelector('nearby-page-template');
    assertTrue(!!template);
    const actionButton =
        template.shadowRoot!.querySelector<HTMLButtonElement>(button);
    assertTrue(!!actionButton);
    return actionButton;
  }

  suite('EnabledTests', () => {
    setup(async () => {
      sharedSetup(/*enabled=*/ true, /*isOnboardingComplete=*/ true);
      dialog.showHighVisibilityPage(/*shutoffTimeoutInSeconds=*/ 5 * 60);
      flush();
      await waitAfterNextRender(dialog);
    });

    teardown(() => {
      dialog.remove();
    });

    test('show high visibility page, get a target, accept', async () => {
      // When attached we enter high visibility mode by default
      assertTrue(isVisible(dialog.shadowRoot!.querySelector(
          'nearby-share-high-visibility-page')));
      assertFalse(isVisible(
          dialog.shadowRoot!.querySelector('nearby-share-confirm-page')));
      // If a share target comes in, we show it.
      const target =
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
      const confirmPage =
          dialog.shadowRoot!.querySelector('nearby-share-confirm-page');
      assertTrue(!!confirmPage);
      flush();

      const progressIcon =
          confirmPage.shadowRoot!.querySelector<NearbyProgressElement>(
              '#progressIcon');
      assertTrue(!!progressIcon);
      assertTrue(!!progressIcon.shareTarget);
      assertEquals(target.name, progressIcon.shareTarget.name);
      const connectionToken =
          confirmPage.shadowRoot!.querySelector('#connectionToken');
      assertTrue(!!connectionToken);
      assertStringContains(connectionToken.textContent!, '1234');
      assertTrue(isChildVisible(confirmPage, 'nearby-preview'));

      const page =
          confirmPage.shadowRoot!.querySelector('nearby-page-template');
      assertTrue(!!page);
      const button =
          page.shadowRoot!.querySelector<HTMLButtonElement>('#actionButton');
      assertTrue(!!button);
      button.click();
      const shareTargetId = await fakeReceiveManager.whenCalled('accept');
      assertEquals(target.id, shareTargetId);
    });

    test('show high visibility page, get a target, reject', async () => {
      // When attached we enter high visibility mode by default
      assertTrue(isVisible(dialog.shadowRoot!.querySelector(
          'nearby-share-high-visibility-page')));
      assertFalse(isVisible(
          dialog.shadowRoot!.querySelector('nearby-share-confirm-page')));
      // If a share target comes in, we show it.
      const target =
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
      const confirmPage =
          dialog.shadowRoot!.querySelector('nearby-share-confirm-page');
      assertTrue(!!confirmPage);
      flush();

      const progressIcon =
          confirmPage.shadowRoot!.querySelector<NearbyProgressElement>(
              '#progressIcon');
      assertTrue(!!progressIcon);
      assertTrue(!!progressIcon.shareTarget);
      assertEquals(target.name, progressIcon.shareTarget.name);
      const connectionToken =
          confirmPage.shadowRoot!.querySelector('#connectionToken');
      assertTrue(!!connectionToken);
      assertStringContains(connectionToken.textContent!, '1234');
      assertTrue(isChildVisible(confirmPage, 'nearby-preview'));

      const page =
          confirmPage.shadowRoot!.querySelector('nearby-page-template');
      assertTrue(!!page);
      const button =
          page.shadowRoot!.querySelector<HTMLButtonElement>('#cancelButton');
      assertTrue(!!button);
      button.click();
      const shareTargetId = await fakeReceiveManager.whenCalled('reject');
      assertEquals(target.id, shareTargetId);
    });

    test(
        'show high visibility page, unregister surface, closes dialog',
        async () => {
          // When attached we enter high visibility mode by default
          assertTrue(isVisible(dialog.shadowRoot!.querySelector(
              'nearby-share-high-visibility-page')));
          assertFalse(isVisible(
              dialog.shadowRoot!.querySelector('nearby-share-confirm-page')));
          // If a share target comes in, we show it.
          await fakeReceiveManager.unregisterForegroundReceiveSurface();
          flush();
          assertTrue(dialog.get('closing_'));
          assertFalse(isVisible(dialog.shadowRoot!.querySelector('cr-dialog')));
        });

    test(
        'OnTransferUpdate, unregister surface, does not close dialog',
        async () => {
          // When attached we enter high visibility mode by default
          assertTrue(isVisible(dialog.shadowRoot!.querySelector(
              'nearby-share-high-visibility-page')));
          assertFalse(isVisible(
              dialog.shadowRoot!.querySelector('nearby-share-confirm-page')));
          fakeReceiveManager.simulateShareTargetArrival('testName', '1234');
          flush();
          assertFalse(dialog.get('closing_'));
          await fakeReceiveManager.unregisterForegroundReceiveSurface();
          flush();
          assertFalse(dialog.get('closing_'));
        });

    test('onStartAdvertisingFailure shows an error', async () => {
      assertTrue(isVisible(dialog.shadowRoot!.querySelector(
          'nearby-share-high-visibility-page')));
      const highVisibilityPage =
          dialog.shadowRoot!.querySelector('nearby-share-high-visibility-page');
      assertTrue(!!highVisibilityPage);
      assertNull(highVisibilityPage.shadowRoot!.querySelector('#errorTitle'));

      dialog.onStartAdvertisingFailure();
      await waitAfterNextRender(dialog);

      const errorTitle =
          highVisibilityPage.shadowRoot!.querySelector('#errorTitle');
      assertTrue(!!errorTitle && errorTitle.textContent!.length > 0);
    });
  });

  suite('DisabledTests', () => {
    setup(() => {
      sharedSetup(/*enabled=*/ false, /*isOnboardingComplete=*/ false);
    });

    teardown(() => {
      dialog.remove();
    });

    test('when disabled, one-page onboarding is shown first', async () => {
      dialog.showHighVisibilityPage(60);
      await waitAfterNextRender(dialog);

      assertFalse(isVisible(
          dialog.shadowRoot!.querySelector('nearby-onboarding-page')));
      assertTrue(isVisible(
          dialog.shadowRoot!.querySelector('nearby-onboarding-one-page')));
      // Finish onboarding
      getButton('nearby-onboarding-one-page', '#actionButton').click();

      await waitAfterNextRender(dialog);

      assertTrue(dialog.settings.enabled);
      assertEquals(Visibility.kAllContacts, dialog.settings.visibility);
      assertTrue(isVisible(dialog.shadowRoot!.querySelector(
          'nearby-share-high-visibility-page')));
    });

    test('when showing onboarding, close when complete.', async () => {
      dialog.showOnboarding();
      await waitAfterNextRender(dialog);

      assertFalse(isVisible(
          dialog.shadowRoot!.querySelector('nearby-onboarding-page')));
      assertTrue(isVisible(
          dialog.shadowRoot!.querySelector('nearby-onboarding-one-page')));
      // Select visibility button and advance to the next page.
      const page =
          dialog.shadowRoot!.querySelector('nearby-onboarding-one-page');
      assertTrue(!!page);
      const button = page.shadowRoot!.querySelector<HTMLButtonElement>(
          '#visibilityButton');
      assertTrue(!!button);
      button.click();

      await waitAfterNextRender(dialog);

      assertTrue(isVisible(
          dialog.shadowRoot!.querySelector('nearby-visibility-page')));
      // All contacts should be selected and confirm should close the dialog.
      fakeContactManager.completeDownload();

      const contactElement =
          dialog.shadowRoot!.querySelector('nearby-visibility-page');
      assertTrue(!!contactElement);
      const contactVisibilityElement =
          contactElement.shadowRoot!.querySelector('nearby-contact-visibility');
      assertTrue(!!contactVisibilityElement);
      const allContacts =
          contactVisibilityElement.shadowRoot!
              .querySelector<CrCardRadioButtonElement>('#contacts');
      assertTrue(!!allContacts);
      assertTrue(allContacts.checked);
      getButton('nearby-visibility-page', '#actionButton').click();

      assertTrue(dialog.get('closing_'));

      await waitAfterNextRender(dialog);

      const element =
          dialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
      assertTrue(!!element);
      assertFalse(element.open);
    });
  });
});
