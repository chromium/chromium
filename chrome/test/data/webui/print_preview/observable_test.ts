// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Observable, setValueAtPath} from 'chrome://print/print_preview.js';
import type {WildcardChangeRecord} from 'chrome://print/print_preview.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

suite('Observable', function() {
  interface Prefs {
    foo: {value: number};
    bar: {value: number};
  }

  function createPrefs(): Prefs {
    return {
      foo: {value: 1},
      bar: {value: 2},
    };
  }

  let observable: Observable<Prefs>;
  let prefs: Prefs;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    observable = new Observable(createPrefs());
    prefs = observable.getProxy();
  });

  test('SetValueAtPath', function() {
    setValueAtPath(['foo', 'value'], prefs, 3);
    assertEquals(3, prefs.foo.value);

    setValueAtPath(['foo'], prefs, {value: 4});
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
    const notifications: Map<string, any[]> = new Map();

    observable.addObserver('foo', (...args) => {
      notifications.set('foo', args);
    });
    observable.addObserver('foo.value', (...args) => {
      notifications.set('foo.value', args);
    });
    observable.addObserver('foo.*', change => {
      notifications.set('foo.*', change);
    });

    observable.addObserver('foo.value.*', change => {
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


suite('ObservablePolymerCompatibility', function() {
  interface Prefs {
    foo: {value: number};
    bar: {value: number[]};
  }

  function createPrefs(): Prefs {
    return {
      foo: {value: 1},
      bar: {value: [0, 1]},
    };
  }

  class TestElement extends PolymerElement {
    static get is() {
      return 'test-element';
    }

    static get properties() {
      return {
        prefs: Object,
      };
    }

    declare prefs: Prefs;
    observable: Observable<Prefs>;

    polymerNotifications: Map<string, any[]> = new Map();
    observableNotifications: Map<string, any[]> = new Map();

    // Register Polymer observers.
    static get observers() {
      return [
        // Regular observers.
        'onFooChanged_(prefs.foo)',
        'onFooValueChanged_(prefs.foo.value)',
        // Wildcard observers.
        'onFooStarChanged_(prefs.foo.*)',
        'onFooValueStarChanged_(prefs.foo.value.*)',

        // Regular array observers.
        'onBarValueChanged_(prefs.bar.value)',
        'onBarValueZeroChanged_(prefs.bar.value.0)',
        'onBarValueOneChanged_(prefs.bar.value.1)',
        'onBarValueLengthChanged_(prefs.bar.value.length)',
        // Wildcard array observers.
        'onBarValueStarChanged_(prefs.bar.value.*)',
      ];
    }

    constructor() {
      super();

      this.observable = new Observable<Prefs>(createPrefs());
      this.prefs = this.observable.getProxy();

      // Register `Observable` observers (alternative non-Polymer mechanism).
      this.observable.addObserver('foo', (...args) => {
        this.observableNotifications.set('foo', args);
      });
      this.observable.addObserver('foo.value', (...args) => {
        this.observableNotifications.set('foo.value', args);
      });
      this.observable.addObserver('foo.*', (change: WildcardChangeRecord) => {
        this.observableNotifications.set('foo.*', [change]);
      });
      this.observable.addObserver(
          'foo.value.*', (change: WildcardChangeRecord) => {
            this.observableNotifications.set('foo.value.*', [change]);
          });

      // Register `Observable` observers for an array property.
      this.observable.addObserver('bar.value', (...args) => {
        this.observableNotifications.set('bar.value', args);
      });
      this.observable.addObserver('bar.value.0', (...args) => {
        this.observableNotifications.set('bar.value.0', args);
      });
      this.observable.addObserver('bar.value.1', (...args) => {
        this.observableNotifications.set('bar.value.1', args);
      });
      this.observable.addObserver('bar.value.length', (...args) => {
        this.observableNotifications.set('bar.value.length', args);
      });
      this.observable.addObserver(
          'bar.value.*', (change: WildcardChangeRecord) => {
            this.observableNotifications.set('bar.value.*', [change]);
          });
    }

    protected onFooChanged_(...args: any[]) {
      this.polymerNotifications.set('foo', args);
    }

    protected onFooValueChanged_(...args: any[]) {
      this.polymerNotifications.set('foo.value', args);
    }

    protected onFooStarChanged_(change: WildcardChangeRecord) {
      this.polymerNotifications.set('foo.*', [change]);
    }

    protected onFooValueStarChanged_(change: WildcardChangeRecord) {
      this.polymerNotifications.set('foo.value.*', [change]);
    }

    /**
     * @param directAccess Whether to skip Polymer's set() helper and directly
     *     operate on the 'prefs' object. This is used to ensure that changes
     *     made either way propagate to bot types of observers (Polymer and
     *     Observable).
     */
    modifyFoo(directAccess: boolean) {
      if (directAccess) {
        this.prefs.foo = {value: 3};
        this.notifyPath('prefs.foo');
        return;
      }

      this.set('prefs.foo', {value: 3});
    }

    /**
     * @param directAccess Whether to skip Polymer's set() helper and directly
     *     operate on the 'prefs' object. This is used to ensure that changes
     *     made either way propagate to bot types of observers (Polymer and
     *     Observable).
     */
    modifyFooValue(directAccess: boolean) {
      if (directAccess) {
        this.prefs.foo.value = 4;
        this.notifyPath('prefs.foo.value');
        return;
      }

      this.set('prefs.foo.value', 4);
    }

    protected onBarValueChanged_(...args: any[]) {
      this.polymerNotifications.set('bar.value', args);
    }

    protected onBarValueZeroChanged_(...args: any[]) {
      this.polymerNotifications.set('bar.value.0', args);
    }

    protected onBarValueOneChanged_(...args: any[]) {
      this.polymerNotifications.set('bar.value.1', args);
    }

    protected onBarValueLengthChanged_(...args: any[]) {
      this.polymerNotifications.set('bar.value.length', args);
    }

    protected onBarValueStarChanged_(...args: any[]) {
      this.polymerNotifications.set('bar.value.*', args);
    }

    modifyBarValue(directAccess: boolean) {
      if (directAccess) {
        this.prefs.bar.value = [4, 5];
        this.notifyPath('prefs.bar.value');
        return;
      }

      this.set('prefs.bar.value', [4, 5]);
    }

    modifyBarValueReplaceItem(directAccess: boolean) {
      if (directAccess) {
        this.prefs.bar.value[0] = 100;
        this.notifyPath('prefs.bar.value.0');
        return;
      }

      this.set('prefs.bar.value.0', 100);
    }

    // TODO(crbug.com/331681689): Implement notification checks for push(),
    // splice() and add tests.
    /*modifyBarValuePushItem() {
      this.push('prefs.bar.value', 200);
    }*/

    clear() {
      this.polymerNotifications.clear();
      this.observableNotifications.clear();
    }
  }

  customElements.define(TestElement.is, TestElement);

  let element: TestElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);
  });

  function assertNotifications(
      polymerExpectation: any[], observableExpectation: any[],
      observerPath: string) {
    assertEquals(
        JSON.stringify(polymerExpectation),
        JSON.stringify(element.polymerNotifications.get(observerPath)));
    assertEquals(
        JSON.stringify(observableExpectation),
        JSON.stringify(element.observableNotifications.get(observerPath)));
  }

  function testNotifications(directAccess: boolean) {
    element.clear();

    // Case1: Modify an entire object (non-leaf node).
    element.modifyFoo(directAccess);
    assertEquals(4, element.polymerNotifications.size);
    assertEquals(4, element.observableNotifications.size);

    // Regular observers.
    assertNotifications(
        [{'value': 3}], [{'value': 3}, {'value': 1}, 'foo'], 'foo');
    assertNotifications([3], [3, 1, 'foo.value'], 'foo.value');
    // Wildcard observers.
    assertNotifications(
        [{'path': 'prefs.foo', 'value': {'value': 3}, 'base': {'value': 3}}],
        [{'path': 'foo', 'value': {'value': 3}, 'base': {'value': 3}}],
        'foo.*');
    assertNotifications(
        [{'path': 'prefs.foo.value', 'value': 3, 'base': 3}],
        [{'path': 'foo.value', 'value': 3, 'base': 3}], 'foo.value.*');

    element.clear();

    // Case2: Modify a value (leaf node).
    element.modifyFooValue(directAccess);
    assertEquals(3, element.polymerNotifications.size);
    assertEquals(3, element.observableNotifications.size);

    // Regular observers.
    assertNotifications([4], [4, 3, 'foo.value'], 'foo.value');

    // Wildcard observers.
    assertNotifications(
        [{'path': 'prefs.foo.value', 'value': 4, 'base': {'value': 4}}],
        [{'path': 'foo.value', 'value': 4, 'base': {'value': 4}}], 'foo.*');
    assertNotifications(
        [{'path': 'prefs.foo.value', 'value': 4, 'base': 4}],
        [{'path': 'foo.value', 'value': 4, 'base': 4}], 'foo.value.*');
  }

  test('MutateViaPolymer', function() {
    testNotifications(/*directAccess=*/ false);
  });

  test('MutateViaDirectAccess', function() {
    testNotifications(/*directAccess=*/ true);
  });

  function testArrayNotifications(directAccess: boolean) {
    element.clear();
    assertEquals(0, element.polymerNotifications.size);
    assertEquals(0, element.observableNotifications.size);

    // Case1: Modify the entire array (non-leaf node).
    element.modifyBarValue(directAccess);
    assertEquals(5, element.polymerNotifications.size);
    assertEquals(5, element.observableNotifications.size);

    // Regular observers.
    assertNotifications([[4, 5]], [[4, 5], [0, 1], 'bar.value'], 'bar.value');
    assertNotifications([4], [4, 0, 'bar.value.0'], 'bar.value.0');
    assertNotifications([5], [5, 1, 'bar.value.1'], 'bar.value.1');
    assertNotifications([2], [2, 2, 'bar.value.length'], 'bar.value.length');

    // Wildcard observers.
    assertNotifications(
        [{'path': 'prefs.bar.value', 'value': [4, 5], 'base': [4, 5]}],
        [{'path': 'bar.value', 'value': [4, 5], 'base': [4, 5]}],
        'bar.value.*');

    element.clear();

    // Case2: Modify a specific position in the array (leaf node).
    element.modifyBarValueReplaceItem(directAccess);
    assertEquals(2, element.polymerNotifications.size);
    assertEquals(2, element.observableNotifications.size);

    // Regular observers.
    assertNotifications([100], [100, 4, 'bar.value.0'], 'bar.value.0');

    // Wildcard observers.
    assertNotifications(
        [{'path': 'prefs.bar.value.0', 'value': 100, 'base': [100, 5]}],
        [{'path': 'bar.value.0', 'value': 100, 'base': [100, 5]}],
        'bar.value.*');
  }

  test('MutateArrayViaPolymer', function() {
    testArrayNotifications(/*directAccess=*/ false);
  });

  test('MutateArrayViaDirectAccess', function() {
    testArrayNotifications(/*directAccess=*/ true);
  });
});
