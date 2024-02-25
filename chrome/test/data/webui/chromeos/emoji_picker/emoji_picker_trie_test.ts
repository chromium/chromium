// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Trie} from 'chrome://emoji-picker/emoji_picker.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('TrieUnitTest', () => {
  let trie: Trie;
  setup(() => {
    trie = new Trie();
  });

  test('Trie should contain keys added.', () => {
    trie.add('abcd');
    trie.add('abde');

    assertTrue(trie.containsKey('abcd'));
    assertTrue(trie.containsKey('abde'));
    assertFalse(trie.containsKey('ab'));
    assertFalse(trie.containsKey('abcde'));
  });

  test('Trie should return all keys that share the same prefix.', () => {
    trie.add('cable');
    trie.add('camera');
    trie.add('card');
    trie.add('cat');
    trie.add('carton');
    trie.add('cello');
    trie.add('hard');
    trie.add('harton');
    trie.add('hat');

    const actualPrefixResults1 = trie.getKeys('ca');
    assertTrue(Array.isArray(actualPrefixResults1));
    assertEquals(5, actualPrefixResults1.length);
    assertTrue(actualPrefixResults1.includes('cable'));
    assertTrue(actualPrefixResults1.includes('camera'));
    assertTrue(actualPrefixResults1.includes('card'));
    assertTrue(actualPrefixResults1.includes('cat'));
    assertTrue(actualPrefixResults1.includes('carton'));
    assertFalse(actualPrefixResults1.includes('car'));

    const actualPrefixResults2 = trie.getKeys('har');
    assertTrue(Array.isArray(actualPrefixResults2));
    assertEquals(2, actualPrefixResults2.length);
    assertTrue(actualPrefixResults2.includes('harton'));
    assertTrue(actualPrefixResults2.includes('hard'));
    assertFalse(actualPrefixResults2.includes('hat'));

    const actualPrefixResults3 = trie.getKeys('chat');
    assertTrue(Array.isArray(actualPrefixResults3));
    assertEquals(0, actualPrefixResults3.length);
  });

  test(
      'Trie.getKeys() (no argument) should be able to return all keys.', () => {
        trie.add('grin');
        trie.add('message');
        trie.add('moon');
        trie.add('mouse');
        const actualAllKeys = trie.getKeys();
        assertTrue(Array.isArray(actualAllKeys));
        assertEquals(4, actualAllKeys.length);
        assertTrue(actualAllKeys.includes('grin'));
        assertTrue(actualAllKeys.includes('message'));
        assertTrue(actualAllKeys.includes('moon'));
        assertTrue(actualAllKeys.includes('mouse'));
      });

  test('Clearing a trie should remove all keys inside it.', () => {
    trie.add('fish');
    trie.add('helen');
    trie.add('hello');
    trie.clear();
    assertFalse(trie.containsKey('hello'));
    assertFalse(trie.containsKey('helen'));
    assertFalse(trie.containsKey('fish'));
  });
});
