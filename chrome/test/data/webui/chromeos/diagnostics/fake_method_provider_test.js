// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://diagnostics/fake_method_resolver.js';

import {assertEquals} from '../../chai_assert.js';

export function fakeMethodResolverTestSuite() {
  /** @type {?FakeMethodResolver} */
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
}
