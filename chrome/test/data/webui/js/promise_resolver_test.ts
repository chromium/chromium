// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('PromiseResolverModuleTest', function() {
  test('members read only', function() {
    const resolver = new PromiseResolver<void>();
    assertThrows(function() {
      // Ignore 'readonly' TS warning to check that a runtime exception is
      // thrown as well.
      // @ts-ignore:next-line
      resolver.promise = Promise.resolve();
    });
    assertThrows(function() {
      resolver.resolve = function() {};
    });
    assertThrows(function() {
      resolver.reject = function() {};
    });
  });

  test('resolves', function(done) {
    const resolver = new PromiseResolver<void>();
    resolver.promise.then(done);
    resolver.resolve();
  });

  test('rejects', function(done) {
    const resolver = new PromiseResolver();
    resolver.promise.catch(done);
    resolver.reject();
  });

  test('is fulfilled', function() {
    const promises = [];
    const resolver1 = new PromiseResolver<void>();
    assertFalse(resolver1.isFulfilled);
    promises.push(resolver1.promise.then(() => {
      assertTrue(resolver1.isFulfilled);
    }));
    resolver1.resolve();

    const resolver2 = new PromiseResolver<boolean>();
    assertFalse(resolver2.isFulfilled);
    promises.push(resolver2.promise.then(arg => {
      assertTrue(resolver2.isFulfilled);
      assertTrue(arg);
    }));
    resolver2.resolve(true);

    const resolver3 = new PromiseResolver<void>();
    assertFalse(resolver3.isFulfilled);
    promises.push(resolver3.promise.catch(() => {
      assertTrue(resolver3.isFulfilled);
    }));
    resolver3.reject(new Error('dummy error'));

    // Don't allow the test to end before all promises are fulfilled.
    return Promise.all(promises);
  });
});
