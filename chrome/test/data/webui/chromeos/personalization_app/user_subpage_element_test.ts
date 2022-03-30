// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {UserSubpage} from 'chrome://personalization/trusted/personalization_app.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('UserSubpageTest', function() {
  let userSubpageElement: UserSubpage|null;

  setup(() => {
    baseSetup();
  });

  teardown(async () => {
    await teardownElement(userSubpageElement);
    userSubpageElement = null;
  });

  test('displays content', async () => {
    userSubpageElement = initElement(UserSubpage);
    const userPreview =
        userSubpageElement.shadowRoot!.querySelector('user-preview');
    assertTrue(!!userPreview);
    const avatarList =
        userSubpageElement.shadowRoot!.querySelector('avatar-list');
    assertTrue(!!avatarList);
  });
});
