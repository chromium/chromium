// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals} from '../../chai_assert.js';
// #import {isChildVisible, waitAfterNextRender} from '../../test_util.m.js';
// #import {setReceiveManagerForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {FakeReceiveManager} from './fake_receive_manager.m.js'
// clang-format on

suite('NearbyShare', function() {
  /** @type {?NearbyShareReceiveDialogElement} */
  let dialog = null;
  /** @type {?FakeReceiveManager} */
  let fakeReceiveManager = null;

  setup(function() {
    fakeReceiveManager = new nearby_share.FakeReceiveManager();
    nearby_share.setReceiveManagerForTesting(fakeReceiveManager);
    PolymerTest.clearBody();
    dialog = document.createElement('nearby-share-receive-dialog');
    document.body.appendChild(dialog);
    Polymer.dom.flush();
  });

  teardown(function() {
    dialog.remove();
  });

  /**
   * @param {string} selector
   * @return {boolean} Returns true if the element is visible in the shadow dom.
   */
  function isVisible(selector) {
    return test_util.isChildVisible(dialog, selector, false);
  }

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
    assertEquals('1234', confirmPage.$$('#connectionToken').textContent.trim());

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
    assertEquals('1234', confirmPage.$$('#connectionToken').textContent.trim());

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
