// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Observable, setValueAtPath} from 'chrome://print/print_preview.js';
import type {Indexable, WildcardChangeRecord} from 'chrome://print/print_preview.js';
import {assertDeepEquals, assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

suite('Observable', function() {
  interface Prefs {
    foo: {value: number};
    bar: {value: number};
  }

  type IndexablePrefs = Indexable<Prefs>;

  function createPrefs(): Prefs {
    return {
      foo: {value: 1},
      bar: {value: 2},
    };
  }

  let observable: Observable<IndexablePrefs>;
  let prefs: Prefs;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    observable = new Observable<IndexablePrefs>(createPrefs());
    prefs = observable.getProxy();
  });

  test('SetValueAtPath', function() {
    setValueAtPath(['foo', 'value'], prefs as IndexablePrefs, 3);
    assertEquals(3, prefs.foo.value);

    setValueAtPath(['foo'], prefs as IndexablePrefs, {value: 4});
    assertEquals(4, prefs.foo.value);
  });

  test('ObserversAddedRemoved', () => {
    let fired: string[] = [];

    // Add observers and remember their IDs.
    const ids = [
      observable.addObserver('foo.*', () => fired.push('foo.*')),
      observable.addObserver('foo', () => fired.push('foo')),
      observable.addObserver('foo.value', () => fired.push('foo.value')),
    ];

    // Case1: Modifying an entire subtree.
    prefs.foo = {value: 3};
    assertDeepEquals(['foo.*', 'foo', 'foo.value'], fired);

    // Case2: Modifying a leaf node.
    fired = [];
    prefs.foo.value = 4;
    assertDeepEquals(['foo.*', 'foo.value'], fired);

    // Case3: Modifying an entire subtree for a node that has no observers.
    // Ensure no error is thrown.
    fired = [];
    prefs.bar = {value: 5};
    assertDeepEquals([], fired);

    // Case4: Modifying a leaf node for a node that has no observers. Ensure no
    // error is thrown.
    fired = [];
    prefs.bar.value = 6;
    assertDeepEquals([], fired);

    // Remove observers and ensure they no longer trigger.

    fired = [];
    observable.removeObserver(ids[0]!);
    prefs.foo = {value: 7};
    assertDeepEquals(['foo', 'foo.value'], fired);

    fired = [];
    observable.removeObserver(ids[1]!);
    prefs.foo = {value: 8};
    assertDeepEquals(['foo.value'], fired);

    fired = [];
    observable.removeAllObservers();
    prefs.foo = {value: 9};
    assertDeepEquals([], fired);
  });

  test('ObserverParmetersRelativeToObservedPath', () => {
    const notifications: Map<string, unknown> = new Map();

    observable.addObserver('foo', (...args: any[]) => {
      notifications.set('foo', args);
    });
    observable.addObserver('foo.value', (...args: any[]) => {
      notifications.set('foo.value', args);
    });
    observable.addObserver('foo.*', (change: WildcardChangeRecord) => {
      notifications.set('foo.*', change);
    });

    observable.addObserver('foo.value.*', (change: WildcardChangeRecord) => {
      notifications.set('foo.value.*', change);
    });

    // ------------- Case1: Modify an entire subtree. -------------------------
    notifications.clear();
    prefs.foo = {value: 3};

    // Check for notifications for observers of the changed node itself.
    assertDeepEquals([{value: 3}, {value: 1}, 'foo'], notifications.get('foo'));

    // Check notifications for observers below the changed node.
    assertDeepEquals([3, 1, 'foo.value'], notifications.get('foo.value'));

    // Check notifications for "star" observers at the changed node.
    assertDeepEquals(
        {path: 'foo', value: {value: 3}, base: {value: 3}},
        notifications.get('foo.*'));

    // Check notifications for "star" observers below the changed node.
    assertDeepEquals(
        {'path': 'foo.value', 'value': 3, 'base': 3},
        notifications.get('foo.value.*'));

    // ------------- Case2: Modify a leaf node. --------------------------------
    notifications.clear();
    prefs.foo.value = 4;

    // Check notifications for non-star observers above the changed node (there
    // should be no notifications).
    assertFalse(notifications.has('foo'));

    // Check notifications for observers of the changed node itself.
    assertDeepEquals([4, 3, 'foo.value'], notifications.get('foo.value'));

    // Check notifications for "star" observers above the changed node.
    assertDeepEquals(
        {path: 'foo.value', value: 4, base: {value: 4}},
        notifications.get('foo.*'));

    // Check notifications for "star" observers at the changed node.
    assertDeepEquals(
        {path: 'foo.value', value: 4, base: 4},
        notifications.get('foo.value.*'));
  });
});
