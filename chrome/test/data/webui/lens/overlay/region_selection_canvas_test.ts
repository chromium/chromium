// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/region_selection.js';

import type {RegionSelectionElement} from 'chrome-untrusted://lens/region_selection.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

suite('ManualRegionSelectionCanvas', function() {
  let regionSelectionElement: RegionSelectionElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    regionSelectionElement = document.createElement('region-selection');
    document.body.appendChild(regionSelectionElement);
    return waitAfterNextRender(regionSelectionElement);
  });

  test('verify canvas resizes according to parent', async () => {
    const parent = regionSelectionElement.parentElement;
    assertTrue(parent !== null);

    parent.style.width = '50px';
    parent.style.height = '50px';
    await waitAfterNextRender(regionSelectionElement);
    assertEquals(50, regionSelectionElement.$.regionSelectionCanvas.width);
    assertEquals(50, regionSelectionElement.$.regionSelectionCanvas.height);

    parent.style.width = '100px';
    parent.style.height = '100px';
    await waitAfterNextRender(regionSelectionElement);
    assertEquals(100, regionSelectionElement.$.regionSelectionCanvas.width);
    assertEquals(100, regionSelectionElement.$.regionSelectionCanvas.height);
  });
});
