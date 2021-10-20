// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FirmwareUpdateAppElement} from 'chrome://firmware-update/firmware_update_app.js';

import {assertEquals} from '../../chai_assert.js';

export function firmwareUpdateAppTest() {
  /** @type {?FirmwareUpdateAppElement} */
  let page = null;

  setup(() => {
    page = /** @type {!FirmwareUpdateAppElement} */ (
        document.createElement('firmware-update-app'));
    document.body.appendChild(page);
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  test('LandingPageLoaded', () => {
    // TODO(michaelcheco): Remove this stub test once the page has more
    // capabilities to test.
    assertEquals(
        'Firmware Update',
        page.shadowRoot.querySelector('#header').textContent);
  });
}