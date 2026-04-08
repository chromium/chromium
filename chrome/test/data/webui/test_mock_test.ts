// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertEquals, assertNotReached} from './chai_assert.js';
import {TestMock} from './test_mock.js';

class Foo {
  bar() {}

  baz(_a: string, _b: number): string {
    return 'default';
  }

  async asyncMethod(): Promise<string> {
    return 'default';
  }
}

suite('TestMockTest', () => {
  let mock: Foo&TestMock<Foo>;

  setup(() => {
    mock = TestMock.fromClass(Foo);
  });

  test('whenCalled', async () => {
    const promise = mock.whenCalled('bar');
    mock.bar();
    await promise;
    assertEquals(1, mock.getCallCount('bar'));
  });

  test('getCallCount', () => {
    assertEquals(0, mock.getCallCount('bar'));
    mock.bar();
    assertEquals(1, mock.getCallCount('bar'));
    mock.bar();
    assertEquals(2, mock.getCallCount('bar'));
  });

  test('getArgs', () => {
    mock.baz('a', 1);
    mock.baz('b', 2);
    assertArrayEquals([['a', 1], ['b', 2]], mock.getArgs('baz'));
  });

  test('setResultFor', () => {
    mock.setResultFor('baz', 'custom');
    assertEquals('custom', mock.baz('a', 1));
  });

  test('setResultMapperFor', () => {
    mock.setResultMapperFor('baz', (a: string, b: number) => a + b);
    assertEquals('a1', mock.baz('a', 1));
    assertEquals('b2', mock.baz('b', 2));
  });

  test('setPromiseResolveFor with value', async function() {
    mock.setPromiseResolveFor('asyncMethod', 'resolvedValue');
    const result = await mock.asyncMethod();
    assertEquals('resolvedValue', result);
  });

  test('setPromiseResolveFor without value', async () => {
    mock.setPromiseResolveFor('asyncMethod');
    const result = await mock.asyncMethod();
    assertEquals(undefined, result);
  });


  test('setPromiseRejectFor with value', async () => {
    mock.setPromiseRejectFor('asyncMethod', 'rejectedError');
    try {
      await mock.asyncMethod();
      assertNotReached('Should have rejected');
    } catch (e) {
      assertEquals('rejectedError', e);
    }
  });

  test('setPromiseRejectFor without value', async () => {
    mock.setPromiseRejectFor('asyncMethod');
    try {
      await mock.asyncMethod();
      assertNotReached('Should have rejected');
    } catch (e) {
      assertEquals(undefined, e);
    }
  });

  test('resetResolver', async () => {
    const promise1 = mock.whenCalled('bar');
    mock.bar();
    await promise1;

    mock.resetResolver('bar');
    const promise2 = mock.whenCalled('bar');
    let called = false;
    promise2.then(() => {
      called = true;
    });

    assertEquals(false, called);
    mock.bar();
    await promise2;
    assertEquals(true, called);
  });

  test('reset', async () => {
    mock.bar();
    mock.baz('a', 1);
    assertEquals(1, mock.getCallCount('bar'));
    assertEquals(1, mock.getCallCount('baz'));

    mock.reset();
    assertEquals(0, mock.getCallCount('bar'));
    assertEquals(0, mock.getCallCount('baz'));
  });
});
