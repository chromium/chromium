// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/strings.m.js';
import 'chrome://nearby/shared/nearby_contact_visibility.js';

import {setContactManagerForTesting} from 'chrome://nearby/shared/nearby_contact_manager.js';
import {DataUsage, FastInitiationNotificationState, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chromeos/chai_assert.js';
import {isChildVisible} from '../../chromeos/test_util.js';

import {FakeContactManager} from './fake_nearby_contact_manager.js';

suite('nearby-contact-visibility', () => {
  /** @type {!NearbyContactVisibilityElement} */
  let visibilityElement;
  /** @type {!FakeContactManager} */
  const fakeContactManager = new FakeContactManager();

  setup(function() {
    document.body.innerHTML = '';

    setContactManagerForTesting(fakeContactManager);

    visibilityElement = /** @type {!NearbyContactVisibilityElement} */ (
        document.createElement('nearby-contact-visibility'));

    visibilityElement.settings = /** @type {!NearbySettings} */ ({
      enabled: false,
      fastInitiationNotificationState: FastInitiationNotificationState.kEnabled,
      isFastInitiationHardwareSupported: true,
      deviceName: 'deviceName',
      dataUsage: DataUsage.kOnline,
      visibility: Visibility.kUnknown,
      isOnboardingComplete: false,
      allowedContacts: [],
    });

    document.body.appendChild(visibilityElement);
  });

  function succeedContactDownload() {
    fakeContactManager.setupContactRecords();
    fakeContactManager.completeDownload();
  }

  /**
   * @return {boolean} true when zero state elements are visible
   */
  function isNoContactsSectionVisible() {
    return isChildVisible(visibilityElement, '#noContactsContainer', false);
  }

  /**
   * @return {boolean} true when zero state elements are visible
   */
  function isZeroStateVisible() {
    return isChildVisible(visibilityElement, '#zeroStateContainer', false);
  }

  /**
   * @return {boolean} true when failed download stat is visible
   */
  function isDownloadContactsFailedVisible() {
    return isChildVisible(visibilityElement, '#contactsFailed', false);
  }

  /**
   * @return {boolean} true when pending contacts state is visible
   */
  function isDownloadContactsPendingVisible() {
    return isChildVisible(visibilityElement, '#contactsPending', false);
  }

  /**
   * @return {boolean} true when the checkboxes for contacts are visible
   */
  function areContactCheckBoxesVisible() {
    const list = visibilityElement.shadowRoot.querySelector('#contactList');
    if (!list) {
      return false;
    }
    return list.querySelectorAll('cr-toggle').length > 0;
  }

  /**
   * @return {boolean} true when visibility selection radio group is disabled
   */
  function isRadioGroupDisabled() {
    return visibilityElement.shadowRoot.querySelector('#visibilityRadioGroup')
        .disabled;
  }

  /**
   * @return {boolean} true when the unreachable contacts message is visibile
   */
  function isUnreachableMessageVisible() {
    return isChildVisible(visibilityElement, '#unreachableMessage', false);
  }

  /**
   * Checks the state of the contacts toggle button group
   * @param {boolean} all is allContacts checked?
   * @param {boolean} some is someContacts checked?
   * @param {boolean} no is noContacts check?
   */
  function assertToggleState(all, some, no) {
    assertEquals(
        all,
        visibilityElement.shadowRoot.querySelector('#allContacts').checked);
    assertEquals(
        some,
        visibilityElement.shadowRoot.querySelector('#someContacts').checked);
    assertEquals(
        no, visibilityElement.shadowRoot.querySelector('#noContacts').checked);
  }


  test('Downloads failed show failure ui', async function() {
    // Failed the download right away so we see the failure screen.
    fakeContactManager.failDownload();
    visibilityElement.set('settings.visibility', Visibility.kSelectedContacts);
    await waitAfterNextRender(visibilityElement);

    assertToggleState(/*all=*/ false, /*some=*/ true, /*no=*/ false);
    assertFalse(isZeroStateVisible());
    assertFalse(isNoContactsSectionVisible());
    assertTrue(isDownloadContactsFailedVisible());
    assertFalse(isDownloadContactsPendingVisible());

    // If we click retry, we should go into pending state.
    visibilityElement.shadowRoot.querySelector('#tryAgainLink').click();
    await waitAfterNextRender(visibilityElement);

    assertFalse(isDownloadContactsFailedVisible());
    assertTrue(isDownloadContactsPendingVisible());

    // If we succeed the download we should see results in the list.
    succeedContactDownload();
    await waitAfterNextRender(visibilityElement);

    assertFalse(isDownloadContactsFailedVisible());
    assertFalse(isDownloadContactsPendingVisible());
    assertTrue(areContactCheckBoxesVisible());
    const items = visibilityElement.shadowRoot.querySelector('#contactList')
                      .querySelectorAll('.contact-item');
    assertEquals(fakeContactManager.contactRecords.length, items.length);
  });

  test('Radio group disabled until successful download', async function() {
    // Radio group disabled after download failure
    fakeContactManager.failDownload();
    await waitAfterNextRender(visibilityElement);
    assertTrue(isDownloadContactsFailedVisible());
    assertTrue(isRadioGroupDisabled());

    // Radio group disabled while downloading
    visibilityElement.shadowRoot.querySelector('#tryAgainLink').click();
    await waitAfterNextRender(visibilityElement);
    assertTrue(isDownloadContactsPendingVisible());
    assertTrue(isRadioGroupDisabled());

    // Radio group enabled after successful download
    succeedContactDownload();
    await waitAfterNextRender(visibilityElement);
    assertFalse(isRadioGroupDisabled());
  });

  test('Visibility component shows zero state for kUnknown', async function() {
    succeedContactDownload();
    // need to wait for the next render to see if the zero
    await waitAfterNextRender(visibilityElement);

    assertToggleState(/*all=*/ false, /*some=*/ false, /*no=*/ false);
    assertTrue(isZeroStateVisible());
    assertFalse(isNoContactsSectionVisible());
  });

  test(
      'Visibility component shows allContacts for kAllContacts',
      async function() {
        succeedContactDownload();
        visibilityElement.set('settings.visibility', Visibility.kAllContacts);

        // need to wait for the next render to see results
        await waitAfterNextRender(visibilityElement);

        assertToggleState(/*all=*/ true, /*some=*/ false, /*no=*/ false);
        assertFalse(isZeroStateVisible());
        assertFalse(areContactCheckBoxesVisible());
        assertFalse(isNoContactsSectionVisible());
      });

  test(
      'Visibility component shows someContacts for kSelectedContacts',
      async function() {
        succeedContactDownload();
        visibilityElement.set(
            'settings.visibility', Visibility.kSelectedContacts);

        // need to wait for the next render to see results
        await waitAfterNextRender(visibilityElement);

        assertToggleState(/*all=*/ false, /*some=*/ true, /*no=*/ false);
        assertFalse(isZeroStateVisible());
        assertTrue(areContactCheckBoxesVisible());
        assertFalse(isNoContactsSectionVisible());
      });

  test('Visibility component shows no contacts for kNoOne', async function() {
    visibilityElement.set('settings.visibility', Visibility.kNoOne);
    succeedContactDownload();
    // need to wait for the next render to see results
    await waitAfterNextRender(visibilityElement);

    assertToggleState(/*all=*/ false, /*some=*/ false, /*no=*/ true);
    assertFalse(isZeroStateVisible());
    assertFalse(areContactCheckBoxesVisible());
    assertFalse(isNoContactsSectionVisible());
  });

  test(
      'Visibility component shows no contacts when there are zero contacts',
      async function() {
        fakeContactManager.contactRecords = [];
        fakeContactManager.completeDownload();
        visibilityElement.set('settings.visibility', Visibility.kAllContacts);
        visibilityElement.set('contacts', []);

        // need to wait for the next render to see results
        await waitAfterNextRender(visibilityElement);

        assertToggleState(/*all=*/ true, /*some=*/ false, /*no=*/ false);
        assertFalse(isZeroStateVisible());
        assertFalse(areContactCheckBoxesVisible());
        assertTrue(isNoContactsSectionVisible());
      });

  test(
      'Unreachable message appears for 1 unreachable contact',
      async function() {
        fakeContactManager.setupContactRecords();
        fakeContactManager.setNumUnreachable(1);
        fakeContactManager.completeDownload();
        visibilityElement.set('settings.visibility', Visibility.kAllContacts);

        // need to wait for the next render to see results
        await waitAfterNextRender(visibilityElement);

        assertTrue(isUnreachableMessageVisible());
      });

  test(
      'Unreachable message appears for more than 1 unreachable contact',
      async function() {
        fakeContactManager.setupContactRecords();
        fakeContactManager.setNumUnreachable(3);
        fakeContactManager.completeDownload();
        visibilityElement.set('settings.visibility', Visibility.kAllContacts);

        // need to wait for the next render to see results
        await waitAfterNextRender(visibilityElement);

        assertTrue(isUnreachableMessageVisible());
      });

  test(
      'Unreachable message hidden for 0 unreachable contacts',
      async function() {
        fakeContactManager.setupContactRecords();
        fakeContactManager.setNumUnreachable(0);
        fakeContactManager.completeDownload();
        visibilityElement.set('settings.visibility', Visibility.kAllContacts);

        // need to wait for the next render to see results
        await waitAfterNextRender(visibilityElement);

        assertFalse(isUnreachableMessageVisible());
      });

  test(
      'Save persists visibility setting and allowed contacts',
      async function() {
        fakeContactManager.setupContactRecords();
        fakeContactManager.setNumUnreachable(0);
        fakeContactManager.completeDownload();
        visibilityElement.set('settings.visibility', Visibility.kAllContacts);
        await waitAfterNextRender(visibilityElement);

        // visibility setting is not immediately updated
        visibilityElement.shadowRoot.querySelector('#someContacts').click();
        await waitAfterNextRender(visibilityElement);
        assertTrue(areContactCheckBoxesVisible());
        assertEquals(
            visibilityElement.get('settings.visibility'),
            Visibility.kAllContacts);

        // allow only contact 2, check that allowed contacts are not yet pushed
        // to the contact manager
        fakeContactManager.setAllowedContacts(['1']);
        for (let i = 0; i < visibilityElement.contacts.length; ++i) {
          visibilityElement.set(
              ['contacts', i, 'checked'],
              visibilityElement.contacts[i].id === '2');
        }
        await waitAfterNextRender(visibilityElement);
        assertEquals(fakeContactManager.allowedContacts.length, 1);
        assertEquals(fakeContactManager.allowedContacts[0], '1');

        // after save, ui state is persisted
        visibilityElement.saveVisibilityAndAllowedContacts();
        assertEquals(
            visibilityElement.get('settings.visibility'),
            Visibility.kSelectedContacts);
        assertEquals(fakeContactManager.allowedContacts.length, 1);
        assertEquals(fakeContactManager.allowedContacts[0], '2');
      });
});
