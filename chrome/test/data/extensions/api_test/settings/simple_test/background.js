// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var succeed = chrome.test.succeed;

function test(stage0, sessionSuported = false) {
  let apis = [chrome.storage.sync, chrome.storage.local];
  if (sessionSuported) {
    apis.push(chrome.storage.session)
  }
  apis.forEach(function(api) {
    api.succeed = chrome.test.callbackPass(api.clear.bind(api));
    stage0.call(api);
  });
}

chrome.test.runTests([
  function getWhenEmpty() {
    function stage0() {
      this.get('foo', stage1.bind(this));
    }
    function stage1(settings) {
      assertEq({}, settings);
      this.get(['foo', 'bar'], stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({}, settings);
      this.get(undefined, stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({}, settings);
      this.succeed();
    }
    test(stage0, true);
  },

  function getWhenNonempty() {
    function stage0() {
      this.set({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, stage1.bind(this));
    }
    function stage1() {
      this.get(['foo', 'baz'], stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({
        'foo': 'bar',
        'baz': 'qux'
      }, settings);
      this.get(['nothing', 'baz', 'hello', 'ignore'], stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({
        'baz'  : 'qux',
        'hello': 'world'
      }, settings);
      this.get(null, stage4.bind(this));
    }
    function stage4(settings) {
      assertEq({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, settings);
      this.succeed();
    }
    test(stage0, true);
  },

  function removeWhenEmpty() {
    function stage0() {
      this.remove('foo', stage1.bind(this));
    }
    function stage1() {
      this.remove(['foo', 'bar'], this.succeed);
    }
    test(stage0);
  },

  function removeWhenNonempty() {
    function stage0() {
      this.set({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, stage1.bind(this));
    }
    function stage1() {
      this.remove('foo', stage2.bind(this));
    }
    function stage2() {
      this.get(null, stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({
        'baz'  : 'qux',
        'hello': 'world'
      }, settings);
      this.remove(['baz', 'nothing'], stage4.bind(this));
    }
    function stage4() {
      this.get(null, stage5.bind(this));
    }
    function stage5(settings) {
      assertEq({
        'hello': 'world'
      }, settings);
      this.remove('hello', stage6.bind(this));
    }
    function stage6() {
      this.get(null, stage7.bind(this));
    }
    function stage7(settings) {
      assertEq({}, settings);
      this.succeed();
    }
    test(stage0);
  },

  function setWhenOverwriting() {
    function stage0() {
      this.set({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, stage1.bind(this));
    }
    function stage1() {
      this.set({
        'foo'  : 'otherBar',
        'baz'  : 'otherQux'
      }, stage2.bind(this));
    }
    function stage2() {
      this.get(null, stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({
        'foo'  : 'otherBar',
        'baz'  : 'otherQux',
        'hello': 'world'
      }, settings);
      this.set({
        'baz'  : 'anotherQux',
        'hello': 'otherWorld',
        'some' : 'value'
      }, stage4.bind(this));
    }
    function stage4() {
      this.get(null, stage5.bind(this));
    }
    function stage5(settings) {
      assertEq({
        'foo'  : 'otherBar',
        'baz'  : 'anotherQux',
        'hello': 'otherWorld',
        'some' : 'value'
      }, settings);
      this.succeed();
    }
    test(stage0, true);
  },

  function clearWhenEmpty() {
    function stage0() {
      this.clear(stage1.bind(this));
    }
    function stage1() {
      this.get(null, stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({}, settings);
      this.succeed();
    }
    test(stage0);
  },

  function clearWhenNonempty() {
    function stage0() {
      this.set({
        'foo'  : 'bar',
        'baz'  : 'qux',
        'hello': 'world'
      }, stage1.bind(this));
    }
    function stage1() {
      this.clear(stage2.bind(this));
    }
    function stage2() {
      this.get(null, stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({}, settings);
      this.succeed();
    }
    test(stage0);
  },

  function keysWithDots() {
    function stage0() {
      this.set({
        'foo.bar' : 'baz',
        'one'     : {'two': 'three'}
      }, stage1.bind(this));
    }
    function stage1() {
      this.get(['foo.bar', 'one'], stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({
        'foo.bar' : 'baz',
        'one'     : {'two': 'three'}
      }, settings);
      this.get('one.two', stage3.bind(this));
    }
    function stage3(settings) {
      assertEq({}, settings);
      this.remove(['foo.bar', 'one.two'], stage4.bind(this));
    }
    function stage4() {
      this.get(null, stage5.bind(this));
    }
    function stage5(settings) {
      assertEq({
        'one'     : {'two': 'three'}
      }, settings);
      this.succeed();
    }
    test(stage0);
  },

  function getWithDefaultValues() {
    function stage0() {
      this.get({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, stage1.bind(this));
    }
    function stage1(settings) {
      assertEq({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, settings);
      this.get(null, stage2.bind(this));
    }
    function stage2(settings) {
      assertEq({}, settings);
      this.set({'foo': 'bar'}, stage3.bind(this));
    }
    function stage3() {
      this.get({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, stage4.bind(this));
    }
    function stage4(settings) {
      assertEq({
        'foo': 'bar',
        'baz': [1, 2, 3]
      }, settings);
      this.set({'baz': {}}, stage5.bind(this));
    }
    function stage5() {
      this.get({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, stage6.bind(this));
    }
    function stage6(settings) {
      assertEq({
        'foo': 'bar',
        'baz': {}
      }, settings);
      this.remove('foo', stage7.bind(this));
    }
    function stage7() {
      this.get({
        'foo': 'defaultBar',
        'baz': [1, 2, 3]
      }, stage8.bind(this));
    }
    function stage8(settings) {
      assertEq({
        'foo': 'defaultBar',
        'baz': {}
      }, settings);
      this.succeed();
    }
    test(stage0);
  },

  // TODO(crbug.com/1185226): Temporary function for `session` to test default
  // values until `remove` and `clear` are implemented. `getWithDefaultValues()`
  // uses `remove`, and `clear` between test calls, and `session` only has `set`
  // and `get` implemented.
  function getWithDefaultValuesSession() {
    var area = chrome.storage.session;
    function stage0() {
      area.get({a: 'defaultA', b: ['b', 'b', 'b']}, stage1);
    }
    function stage1(settings) {
      assertEq({a: 'defaultA', b: ['b', 'b', 'b']}, settings);
      area.set({a: 'A'}, stage2);
    }
    function stage2() {
      area.get({a: 'defaultA', b: ['b', 'b', 'b']}, stage3);
    }
    function stage3(settings) {
      assertEq({a: 'A', b: ['b', 'b', 'b']}, settings);
      area.set({b: {}}, stage4);
    }
    function stage4() {
      area.get({a: 'defaultA', b: ['b', 'b', 'b']}, stage5);
    }
    function stage5(settings) {
      assertEq({a: 'A', b: {}}, settings);
      succeed();
    }
    area.clear(stage0);
  },

  function quota() {
    // Just check that the constants are defined; no need to be forced to
    // update them here as well if/when they change.
    assertTrue(chrome.storage.sync.QUOTA_BYTES > 0);
    assertTrue(chrome.storage.sync.QUOTA_BYTES_PER_ITEM > 0);
    assertTrue(chrome.storage.sync.MAX_ITEMS > 0);

    assertTrue(chrome.storage.local.QUOTA_BYTES > 0);
    assertEq('undefined', typeof chrome.storage.local.QUOTA_BYTES_PER_ITEM);
    assertEq('undefined', typeof chrome.storage.local.MAX_ITEMS);

    assertTrue(chrome.storage.session.QUOTA_BYTES > 0);

    var area = chrome.storage.sync;
    function stage0() {
      area.getBytesInUse(null, stage1);
    }
    function stage1(bytesInUse) {
      assertEq(0, bytesInUse);
      area.set({ a: 42, b: 43, c: 44 }, stage2);
    }
    function stage2() {
      area.getBytesInUse(null, stage3);
    }
    function stage3(bytesInUse) {
      assertEq(9, bytesInUse);
      area.getBytesInUse('a', stage4);
    }
    function stage4(bytesInUse) {
      assertEq(3, bytesInUse);
      area.getBytesInUse(['a', 'b'], stage5);
    }
    function stage5(bytesInUse) {
      assertEq(6, bytesInUse);
      succeed();
    }
    area.clear(stage0);
  },

  function nullsInArgs() {
    var area = chrome.storage.local;
    function stage0() {
      area.get({
        foo: 'foo',
        bar: null,
        baz: undefined
      }, stage1);
    }
    function stage1(values) {
      assertEq({
        foo: 'foo',
        bar: null,
      }, values);
      area.set({
        foo: 'foo',
        bar: null,
        baz: undefined
      }, area.get.bind(area, stage2));
    }
    function stage2(values) {
      assertEq({
        foo: 'foo',
        bar: null,
      }, values);
      succeed();
    }
    area.clear(stage0);
  },
]);
