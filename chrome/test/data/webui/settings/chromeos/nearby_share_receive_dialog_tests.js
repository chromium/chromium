// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals} from '../../chai_assert.js';
// #import {isChildVisible, waitAfterNextRender} from '../../test_util.m.js';
// #import 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NearbyShare', function() {
  /** @type {?NearbyShareReceiveDialogElement} */
  let dialog = null;

  setup(function() {
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

  test('show high visibility page', function() {
    dialog.showHighVisibilityPage();
    Polymer.dom.flush();
    assertTrue(isVisible('nearby-share-high-visibility-page'));
    assertFalse(isVisible('nearby-share-confirm-page'));
  });

  test('show confirm page', function() {
    dialog.showConfirmPage();
    Polymer.dom.flush();
    assertFalse(isVisible('nearby-share-high-visibility-page'));
    assertTrue(isVisible('nearby-share-confirm-page'));
  });
});
