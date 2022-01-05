// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientSubpage} from 'chrome://personalization/trusted/ambient/ambient_subpage_element.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

export function AmbientSubpageTest() {
  let ambientSubpageElement: AmbientSubpage|null;

  setup(function() {});

  teardown(async () => {
    await teardownElement(ambientSubpageElement);
    ambientSubpageElement = null;
  });

  test('displays content', async () => {
    ambientSubpageElement = initElement(AmbientSubpage);
    assertEquals(
        'Ambient',
        ambientSubpageElement.shadowRoot!.querySelector('h2')!.innerText);
  });
}
