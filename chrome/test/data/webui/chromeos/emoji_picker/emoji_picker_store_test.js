// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RecentlyUsedStore} from 'chrome://emoji-picker/store.js';
import {assert} from 'chrome://resources/ash/common/assert.js';

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

const TEST_EMOJI = {
  waving: {string: '👋', codepoints: [128075]},
  grin: {string: '😀', codepoints: [128512]},
};

suite('RecentEmojiStore', () => {
  let store;

  setup(() => {
    window.localStorage.clear();
    store = new RecentlyUsedStore('emoji-recently-used');
  });

  test('store should be initially empty', () => {
    assert(store.data.length === 0);
  });

  test('store data should be persisted', () => {
    store.bumpItem(TEST_EMOJI.grin.string);

    const newStore = new RecentlyUsedStore('emoji-recently-used');
    assertDeepEquals(store.data, newStore.data);
  });

  test('one emoji in recently used', () => {
    store.bumpItem(TEST_EMOJI.grin.string);
    assertDeepEquals([TEST_EMOJI.grin.string], store.data);

    store.bumpItem(TEST_EMOJI.grin.string);
    assertDeepEquals(
        [TEST_EMOJI.grin.string], store.data,
        'clicking an existing emoji should not duplicate it');
  });

  test('two emoji in recently used', () => {
    store.bumpItem(TEST_EMOJI.grin.string);
    store.bumpItem(TEST_EMOJI.waving.string);
    assertDeepEquals(
        [TEST_EMOJI.waving.string, TEST_EMOJI.grin.string], store.data);

    store.bumpItem(TEST_EMOJI.grin.string);
    assertDeepEquals(
        [TEST_EMOJI.grin.string, TEST_EMOJI.waving.string], store.data,
        'clicking an existing emoji should move it to the front');
  });
});
