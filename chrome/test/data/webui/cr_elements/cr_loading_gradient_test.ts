// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('CrLoadingGradientElement', () => {
  test('AssignsUniqueIds', async () => {
    document.body.innerHTML = getTrustedHTML`
      <cr-loading-gradient>
        <svg width="100" height="100">
          <clipPath><circle cx="100" cy="100" r="100"></circle></clipPath>
        </svg>
      </cr-loading-gradient>
      <cr-loading-gradient>
        <svg width="100" height="100">
          <clipPath>
            <rect x="0" y="0" width="100%" height="100" rx="5"></rect>
          </clipPath>
        </svg>
      </cr-loading-gradient>
    `;
    await microtasksFinished();

    const gradients = document.querySelectorAll('cr-loading-gradient');
    assertEquals(
        'crLoadingGradient0', gradients[0]!.querySelector('clipPath')!.id);
    assertEquals(
        'url("#crLoadingGradient0")',
        gradients[0]!.style.getPropertyValue('clip-path'));
    assertEquals(
        'crLoadingGradient1', gradients[1]!.querySelector('clipPath')!.id);
    assertEquals(
        'url("#crLoadingGradient1")',
        gradients[1]!.style.getPropertyValue('clip-path'));
  });
});
