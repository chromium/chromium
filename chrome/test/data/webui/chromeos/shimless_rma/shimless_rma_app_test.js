// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ShimlessRmaElement} from 'chrome://shimless-rma/shimless_rma.js';

import {assertEquals} from '../../chai_assert.js';

export function shimlessRMAAppTest() {
  /** @type {?ShimlessRmaElement} */
  let component = null;

  setup(() => {
    document.body.innerHTML = '';
    component = /** @type {!ShimlessRmaElement} */ (
        document.createElement('shimless-rma'));
    document.body.appendChild(component);
  });

  teardown(() => {
    component.remove();
    component = null;
  });

  test('ShimlessRMALoaded', () => {
    assertEquals(
        'hello world',
        component.shadowRoot.querySelector('#header').textContent);
  });
}