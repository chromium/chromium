// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://diagnostics/fake_observables.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {assertEquals} from '../../chai_assert.js';

export function fakeObservablesTestSuite() {
  /** @type {?FakeObservables} */
  let observables = null;

  setup(() => {
    observables = new FakeObservables();
  });

  teardown(() => {
    observables = null;
  });

  test('RegisterSimpleObservable', () => {
    observables.register('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar'];
    observables.setObservableData('ObserveFoo_OnFooUpdated', expected);

    let resolver = new PromiseResolver();
    observables.observe('ObserveFoo_OnFooUpdated', (foo) => {
      assertEquals(expected[0], foo);
      resolver.resolve();
    });

    observables.trigger('ObserveFoo_OnFooUpdated');
    return resolver.promise;
  });

  test('TwoResults', () => {
    observables.register('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar1', 'bar2'];
    observables.setObservableData('ObserveFoo_OnFooUpdated', expected);

    // The first call will get 'bar1', and the second 'bar2'.
    let resolver = new PromiseResolver();
    const expectedCallCount = 2;
    let callCount = 0;
    observables.observe('ObserveFoo_OnFooUpdated', (foo) => {
      assertEquals(expected[callCount % expected.length], foo);
      callCount++;
      if (callCount === expectedCallCount) {
        resolver.resolve();
      }
    });

    // Trigger the observer twice.
    observables.trigger('ObserveFoo_OnFooUpdated');
    observables.trigger('ObserveFoo_OnFooUpdated');
    return resolver.promise;
  });

  test('ObservableDataWraps', () => {
    observables.register('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar1', 'bar2'];
    observables.setObservableData('ObserveFoo_OnFooUpdated', expected);

    // With 3 calls and 2 observable values the response should cycle
    // 'bar1', 'bar2', 'bar1'
    let resolver = new PromiseResolver();
    const expectedCallCount = 3;
    let callCount = 0;
    observables.observe('ObserveFoo_OnFooUpdated', (foo) => {
      assertEquals(expected[callCount % expected.length], foo);
      callCount++;
      if (callCount === expectedCallCount) {
        resolver.resolve();
      }
    });

    // Trigger the observer three times.
    observables.trigger('ObserveFoo_OnFooUpdated');
    observables.trigger('ObserveFoo_OnFooUpdated');
    observables.trigger('ObserveFoo_OnFooUpdated');
    return resolver.promise;
  });
}
