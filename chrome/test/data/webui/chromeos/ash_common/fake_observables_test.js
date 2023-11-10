// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeObservablesTestSuite', () => {
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

    const resolver = new PromiseResolver();
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
    const resolver = new PromiseResolver();
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
    const resolver = new PromiseResolver();
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

  test('RegisterSimpleSharedObservable', () => {
    observables.registerObservableWithArg('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar'];
    observables.setObservableDataForArg(
        'ObserveFoo_OnFooUpdated', 'foo', expected);

    const resolver = new PromiseResolver();
    observables.observeWithArg('ObserveFoo_OnFooUpdated', 'foo', (foo) => {
      assertEquals(expected[0], foo);
      resolver.resolve();
    });

    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'foo');
    return resolver.promise;
  });

  test('SharedObservableRegisteredTwice', () => {
    observables.registerObservableWithArg('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar1', 'bar2'];
    observables.setObservableDataForArg(
        'ObserveFoo_OnFooUpdated', 'bar', expected);
    observables.setObservableDataForArg(
        'ObserveFoo_OnFooUpdated', 'bar', expected);

    // With 3 calls and 2 observable values the response should cycle
    // 'bar1', 'bar2', 'bar1'
    const resolver = new PromiseResolver();
    const expectedCallCount = 3;
    let callCount = 0;
    observables.observeWithArg('ObserveFoo_OnFooUpdated', 'bar', (foo) => {
      assertEquals(expected[callCount % expected.length], foo);
      callCount++;
      if (callCount === expectedCallCount) {
        resolver.resolve();
      }
    });

    // Trigger the observer three times.
    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'bar');
    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'bar');
    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'bar');
    return resolver.promise;
  });

  test('SharedObservableRegisteredTwiceWithNewData', () => {
    observables.registerObservableWithArg('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    let expected = ['bar1', 'bar2'];
    observables.setObservableDataForArg(
        'ObserveFoo_OnFooUpdated', 'bar', expected);

    // With 4 calls and 4 observable values the response should cycle
    // 'bar1', 'bar2', 'bar3', 'bar4'
    const resolver = new PromiseResolver();
    const expectedCallCount = 4;
    let callCount = 0;
    observables.observeWithArg('ObserveFoo_OnFooUpdated', 'bar', (foo) => {
      assertEquals(expected[callCount % expected.length], foo);
      callCount++;
      if (callCount === expectedCallCount) {
        resolver.resolve();
      }
    });

    // Trigger the observer twice.
    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'bar');
    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'bar');

    // Update observable data.
    expected = ['bar3', 'bar4'];
    // Change observable data for 'ObserveFoo_OnFooUpdated_bar'.
    observables.setObservableDataForArg(
        'ObserveFoo_OnFooUpdated', 'bar', expected);

    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'bar');
    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'bar');
    return resolver.promise;
  });

  test('ObservableRegisteredWithTwoParameters', () => {
    observables.registerObservableWithArg('ObserveFoo_OnFooUpdated');
    /** @type {!Array<!Array<string>>} */
    const expected = [['bar', 'baz']];
    observables.setObservableDataForArg(
        'ObserveFoo_OnFooUpdated', 'foo', expected);

    const resolver = new PromiseResolver();
    observables.observeWithArg('ObserveFoo_OnFooUpdated', 'foo', (foo, bar) => {
      assertEquals(expected[0][0], foo);
      assertEquals(expected[0][1], bar);
      resolver.resolve();
    });

    observables.triggerWithArg('ObserveFoo_OnFooUpdated', 'foo');
    return resolver.promise;
  });

  test('ObservableTriggeredOnInterval', () => {
    observables.register('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar'];
    observables.setObservableData('ObserveFoo_OnFooUpdated', expected);

    const resolver = new PromiseResolver();
    observables.observe('ObserveFoo_OnFooUpdated', (foo) => {
      assertEquals(expected[0], foo);
      resolver.resolve();
    });
    observables.startTriggerOnInterval('ObserveFoo_OnFooUpdated', 0);

    return resolver.promise;
  });

  test('ObservableTriggeredOnIntervalWithArg', () => {
    observables.registerObservableWithArg('ObserveFoo_OnFooUpdated');
    /** @type !Array<string> */
    const expected = ['bar'];
    observables.setObservableDataForArg(
        'ObserveFoo_OnFooUpdated', 'foo', expected);

    const resolver = new PromiseResolver();
    observables.observeWithArg('ObserveFoo_OnFooUpdated', 'foo', (foo) => {
      assertEquals(expected[0], foo);
      resolver.resolve();
    });
    observables.startTriggerOnIntervalWithArg(
        'ObserveFoo_OnFooUpdated', 'foo', 0);
    return resolver.promise;
  });
});
