// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, dummyDescriptor} from 'chrome://new-tab-page/new_tab_page.js';
import {isVisible} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesDummyModuleTest', () => {
  setup(() => {
    PolymerTest.clearBody();
  });

  test('creates module', async () => {
    // Act.
    await dummyDescriptor.initialize();
    const module = dummyDescriptor.element;
    document.body.append(module);
    module.$.tileList.render();

    // Assert.
    assertTrue(isVisible(module.$.tiles));
    const tiles = module.shadowRoot.querySelectorAll('#tiles .tile-item');
    assertEquals(6, tiles.length);
    assertEquals('item3', tiles[2].getAttribute('title'));
    assertEquals('baz', tiles[2].querySelector('span').textContent);
    assertEquals(
        'https://lh6.googleusercontent.com/proxy/' +
            '4IP40Q18w6aDF4oS4WRnUj0MlCCKPK-vLHqSd4r-RfS6Jx' +
            'gblG5WJuRYpkJkoTzLMS0qv3Sxhf9wdaKkn3vHnyy6oe7Ah' +
            '5y0=w170-h85-p-k-no-nd-mv',
        tiles[2].querySelector('img').autoSrc);
  });
});
