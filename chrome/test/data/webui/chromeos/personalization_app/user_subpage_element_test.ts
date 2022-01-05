// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UserSubpage} from 'chrome://personalization/trusted/user/user_subpage_element.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

export function UserSubpageTest() {
  let userSubpageElement: UserSubpage|null;

  setup(function() {});

  teardown(async () => {
    await teardownElement(userSubpageElement);
    userSubpageElement = null;
  });

  test('displays content', async () => {
    userSubpageElement = initElement(UserSubpage);
    assertEquals(
        'User', userSubpageElement.shadowRoot!.querySelector('h2')!.innerText);
  });
}
