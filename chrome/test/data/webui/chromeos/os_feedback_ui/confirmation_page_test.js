// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConfirmationPageElement} from 'chrome://os-feedback/confirmation_page.js';

import {assertEquals} from '../../chai_assert.js';

export function confirmationPageTest() {
  /** @type {?ConfirmationPageElement} */
  let component = null;

  setup(() => {
    document.body.innerHTML = '';
    component = /** @type {!ConfirmationPageElement} */ (
        document.createElement('confirmation-page'));
    document.body.appendChild(component);
  });

  teardown(() => {
    component.remove();
    component = null;
  });

  /**TODO(xiangdongkong): replace with real test  */
  test('confirmationPageLoaded', () => {
    assertEquals(
        'Thank you for your feedback',
        component.shadowRoot.querySelector('#header').textContent);
  });
}
