// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testMembersReadOnly() {
  const resolver = new PromiseResolver;
  assertThrows(function() {
    resolver.promise = new Promise;
  });
  assertThrows(function() {
    resolver.resolve = function() {};
  });
  assertThrows(function() {
    resolver.reject = function() {};
  });
}

function testResolves(done) {
  const resolver = new PromiseResolver;
  resolver.promise.then(done);
  resolver.resolve();
}

function testRejects(done) {
  const resolver = new PromiseResolver;
  resolver.promise.catch(done);
  resolver.reject();
}

function testisFulfilled() {
  const resolver1 = new PromiseResolver;
  assertFalse(resolver1.isFulfilled);
  resolver1.resolve();
  assertTrue(resolver1.isFulfilled);

  const resolver2 = new PromiseResolver;
  assertFalse(resolver2.isFulfilled);
  resolver2.resolve(true);
  assertTrue(resolver2.isFulfilled);

  const resolver3 = new PromiseResolver;
  assertFalse(resolver3.isFulfilled);
  resolver3.reject(new Error);
  assertTrue(resolver3.isFulfilled);
}
