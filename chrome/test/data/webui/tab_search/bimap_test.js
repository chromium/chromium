// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BiMap} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals} from '../../chai_assert.js';

suite('BiMapTest', () => {
  test('Base', async () => {
    let biMap = new BiMap({
      a: 1,
      b: 2,
    });
    assertEquals(2, biMap.size());

    biMap = new BiMap();
    biMap.set('a', 1);
    biMap.set('b', 2);
    assertEquals(2, biMap.size());
    assertEquals(1, biMap.get('a'));
    assertEquals(2, biMap.get('b'));
    assertEquals('a', biMap.invGet(1));
    assertEquals('b', biMap.invGet(2));
  });

  // TODO(romanarora): Add more tests
});
