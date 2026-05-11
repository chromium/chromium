// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

export function assertShown(
    allDivs: string[], dialog: HTMLElement, expectedId: string) {
  assertTrue(allDivs.includes(expectedId));

  const allShown = allDivs.filter(id => {
    return dialog.shadowRoot!.querySelector(`#${id}`)!.classList.contains(
        'selected');
  });
  assertEquals(1, allShown.length);
  assertEquals(expectedId, allShown[0]);
}
