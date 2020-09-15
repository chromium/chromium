// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://nearby/shared/nearby_contact_visibility.m.js';
// #import {setContactManagerForTesting} from 'chrome://nearby/shared/nearby_contact_manager.m.js';
// #import {setNearbyShareSettingsForTesting} from 'chrome://nearby/shared/nearby_share_settings.m.js';
// #import {FakeContactManager} from './fake_nearby_contact_manager.m.js';
// #import {FakeNearbyShareSettings} from './fake_nearby_share_settings.m.js';
// #import {assertEquals, assertTrue, assertFalse} from '../../chai_assert.js';
// #import {waitAfterNextRender, isChildVisible} from '../../test_util.m.js';
// clang-format on

suite('nearby-contact-visibility', () => {
  /** @type {!NearbyContactVisibilityElement} */
  let visibilityElement;
  /** @type {!nearby_share.FakeContactManager} */
  const fakeContactManager = new nearby_share.FakeContactManager();

  setup(function() {
    document.body.innerHTML = '';

    nearby_share.setContactManagerForTesting(fakeContactManager);

    visibilityElement = /** @type {!NearbyContactVisibilityElement} */ (
        document.createElement('nearby-contact-visibility'));

    visibilityElement.settings = {
      enabled: false,
      deviceName: 'deviceName',
      dataUsage: nearbyShare.mojom.DataUsage.kOnline,
      visibility: nearbyShare.mojom.Visibility.kUnknown,
      allowedContacts: [],
    };

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
    return test_util.isChildVisible(
        visibilityElement, '#noContactsContainer', false);
  }

  /**
   * @return {boolean} true when zero state elements are visible
   */
  function isZeroStateVisible() {
    return test_util.isChildVisible(
        visibilityElement, '#zeroStateContainer', false);
  }

  /**
   * @return {boolean} true when failed download stat is visible
   */
  function isDownloadContactsFailedVisible() {
    return test_util.isChildVisible(
        visibilityElement, '#contactsFailed', false);
  }

  /**
   * @return {boolean} true when pending contacts state is visible
   */
  function isDownloadContactsPendingVisible() {
    return test_util.isChildVisible(
        visibilityElement, '#contactsPending', false);
  }

  /**
   * @return {boolean} true when the checkboxes for contacts are visible
   */
  function areContactCheckBoxesVisible() {
    const list = visibilityElement.$$('#contactList');
    if (!list) {
      return false;
    }
    return list.querySelectorAll('cr-toggle').length > 0;
  }

  /**
   * Checks the state of the contacts toggle button group
   * @param {boolean} all is allContacts checked?
   * @param {boolean} some is someContacts checked?
   * @param {boolean} no is noContacts check?
   */
  function assertToggleState(all, some, no) {
    assertEquals(all, visibilityElement.$$('#allContacts').checked);
    assertEquals(some, visibilityElement.$$('#someContacts').checked);
    assertEquals(no, visibilityElement.$$('#noContacts').checked);
  }


  test('Downloads failed show failure ui', async function() {
    // Failed the download right away so we see the failure screen.
    fakeContactManager.failDownload();
    visibilityElement.set(
        'settings.visibility', nearbyShare.mojom.Visibility.kSelectedContacts);
    await test_util.waitAfterNextRender(visibilityElement);

    assertToggleState(/*all=*/ false, /*some=*/ true, /*no=*/ false);
    assertFalse(isZeroStateVisible());
    assertFalse(isNoContactsSectionVisible());
    assertTrue(isDownloadContactsFailedVisible());
    assertFalse(isDownloadContactsPendingVisible());

    // If we click retry, we should go into pending state.
    visibilityElement.$$('#contactRetryButton').click();
    await test_util.waitAfterNextRender(visibilityElement);

    assertFalse(isDownloadContactsFailedVisible());
    assertTrue(isDownloadContactsPendingVisible());

    // If we succeed the download we should see results in the list.
    succeedContactDownload();
    await test_util.waitAfterNextRender(visibilityElement);

    assertFalse(isDownloadContactsFailedVisible());
    assertFalse(isDownloadContactsPendingVisible());
    assertTrue(areContactCheckBoxesVisible());
    const list = visibilityElement.$$('#contactList');
    assertEquals(fakeContactManager.contactRecords.length, list.items.length);
  });

  test('Visibility component shows zero state for kUnknown', async function() {
    succeedContactDownload();
    // need to wait for the next render to see if the zero
    await test_util.waitAfterNextRender(visibilityElement);

    assertToggleState(/*all=*/ false, /*some=*/ false, /*no=*/ false);
    assertTrue(isZeroStateVisible());
    assertFalse(isNoContactsSectionVisible());
  });

  test(
      'Visibility component shows allContacts for kAllContacts',
      async function() {
        succeedContactDownload();
        visibilityElement.set(
            'settings.visibility', nearbyShare.mojom.Visibility.kAllContacts);

        // need to wait for the next render to see results
        await test_util.waitAfterNextRender(visibilityElement);

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
            'settings.visibility',
            nearbyShare.mojom.Visibility.kSelectedContacts);

        // need to wait for the next render to see results
        await test_util.waitAfterNextRender(visibilityElement);

        assertToggleState(/*all=*/ false, /*some=*/ true, /*no=*/ false);
        assertFalse(isZeroStateVisible());
        assertTrue(areContactCheckBoxesVisible());
        assertFalse(isNoContactsSectionVisible());
      });

  test('Visibility component shows noContacts for kNoOne', async function() {
    visibilityElement.set(
        'settings.visibility', nearbyShare.mojom.Visibility.kNoOne);
    succeedContactDownload();
    // need to wait for the next render to see results
    await test_util.waitAfterNextRender(visibilityElement);

    assertToggleState(/*all=*/ false, /*some=*/ false, /*no=*/ true);
    assertFalse(isZeroStateVisible());
    assertTrue(areContactCheckBoxesVisible());
    assertFalse(isNoContactsSectionVisible());
  });

  test(
      'Visibility component shows no contacts when there are zero contacts',
      async function() {
        fakeContactManager.contactRecords = [];
        fakeContactManager.completeDownload();
        visibilityElement.set(
            'settings.visibility', nearbyShare.mojom.Visibility.kAllContacts);
        visibilityElement.set('contacts', []);

        // need to wait for the next render to see results
        await test_util.waitAfterNextRender(visibilityElement);

        assertToggleState(/*all=*/ true, /*some=*/ false, /*no=*/ false);
        assertFalse(isZeroStateVisible());
        assertFalse(areContactCheckBoxesVisible());
        assertTrue(isNoContactsSectionVisible());
      });
});
