// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PersonalizationMain} from 'chrome://personalization/trusted/personalization_main_element.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

export function PersonalizationMainTest() {
  let personalizationMainElement: PersonalizationMain|null;

  setup(function() {});

  teardown(async () => {
    await teardownElement(personalizationMainElement);
    personalizationMainElement = null;
  });

  test('displays content', async () => {
    personalizationMainElement = initElement(PersonalizationMain);
    assertEquals(
        'Personalization',
        personalizationMainElement.shadowRoot!.querySelector('h1')!.innerText);
  });
}
