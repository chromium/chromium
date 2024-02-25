// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeMethodResolverTestSuite', () => {
  let resolver = null;

  setup(() => {
    resolver = new FakeMethodResolver();
  });

  teardown(() => {
    resolver = null;
  });

  test('AddingMethodNoResult', () => {
    resolver.register('foo');
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(undefined, result);
    });
  });

  test('AddingMethodWithResult', () => {
    resolver.register('foo');
    const expected = {'foo': 'bar'};
    resolver.setResult('foo', expected);
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(expected, result);
    });
  });

  test('AddingTwoMethodCallingOne', () => {
    resolver.register('foo');
    resolver.register('bar');
    const expected = {'fooKey': 'fooValue'};
    resolver.setResult('foo', expected);
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(expected, result);
    });
  });

  test('AddingMethodWithChangedResult', () => {
    resolver.register('foo');
    const expected = {'foo': 'bar'};
    const second_expected = {'foo': 'baz'};
    resolver.setResult('foo', expected);
    resolver.resolveMethod('foo').then((result) => {
      assertEquals(expected, result);
    });
    resolver.setResult('foo', second_expected);
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(second_expected, result);
    });
  });

  test('AddingMethodWithDelayedResult', () => {
    resolver.register('foo');
    const expected = {'foo': 'bar'};
    resolver.setResult('foo', expected);
    return resolver.resolveMethodWithDelay('foo', 1).then((result) => {
      assertEquals(expected, result);
    });
  });
});
