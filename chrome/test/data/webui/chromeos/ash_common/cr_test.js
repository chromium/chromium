// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

test('DefineProperty', function() {
  const obj = new EventTarget();
  Object.defineProperty(obj, 'test', cr.getPropertyDescriptor('test'));

  obj.test = 1;
  assertEquals(1, obj.test);
  assertEquals(1, obj.test_);
});

test('DefinePropertyOnClass', function() {
  class C extends EventTarget {}

  Object.defineProperty(C.prototype, 'test', cr.getPropertyDescriptor('test'));

  const obj = new C();
  assertEquals(undefined, obj.test);

  obj.test = 1;
  assertEquals(1, obj.test);
  assertEquals(1, obj.test_);
});

test('DefinePropertyWithSetter', function() {
  const obj = new EventTarget();

  let hit = false;
  function onTestSet(value, oldValue) {
    assertEquals(obj, this);
    assertEquals(2, this.test);
    assertEquals(undefined, oldValue);
    assertEquals(2, value);
    hit = true;
  }
  Object.defineProperty(
      obj, 'test',
      cr.getPropertyDescriptor('test', cr.PropertyKind.JS, onTestSet));
  obj.test = 2;
  assertTrue(hit);
});

test('DefinePropertyEvent', function() {
  const obj = new EventTarget();
  Object.defineProperty(obj, 'test', cr.getPropertyDescriptor('test'));
  obj.test = 1;

  let count = 0;
  function f(e) {
    assertEquals('testChange', e.type);
    assertEquals('test', e.propertyName);
    assertEquals(1, e.oldValue);
    assertEquals(2, e.newValue);
    count++;
  }

  obj.addEventListener('testChange', f);
  obj.test = 2;
  assertEquals(2, obj.test);
  assertEquals(1, count, 'Should have called the property change listener');

  obj.test = 2;
  assertEquals(1, count);
});

test('DefinePropertyEventWithDefault', function() {
  const obj = new EventTarget();
  Object.defineProperty(
      obj, 'test', cr.getPropertyDescriptor('test', cr.PropertyKind.JS));

  let count = 0;
  function f(e) {
    assertEquals('testChange', e.type);
    assertEquals('test', e.propertyName);
    assertEquals(undefined, e.oldValue);
    assertEquals(2, e.newValue);
    count++;
  }

  obj.addEventListener('testChange', f);

  obj.test = undefined;
  assertEquals(0, count, 'Should not have called the property change listener');

  obj.test = 2;
  assertEquals(2, obj.test);
  assertEquals(1, count, 'Should have called the property change listener');

  obj.test = 2;
  assertEquals(1, count);
});

test('DefinePropertyAttr', function() {
  const obj = document.createElement('div');
  Object.defineProperty(
      obj, 'test', cr.getPropertyDescriptor('test', cr.PropertyKind.ATTR));

  obj.test = 'a';
  assertEquals('a', obj.test);
  assertEquals('a', obj.getAttribute('test'));

  obj.test = undefined;
  assertEquals(null, obj.test);
  assertFalse(obj.hasAttribute('test'));
});

test('DefinePropertyAttrOnClass', function() {
  const obj = document.createElement('button');
  Object.defineProperty(
      obj, 'test', cr.getPropertyDescriptor('test', cr.PropertyKind.ATTR));

  assertEquals(null, obj.test);

  obj.test = 'a';
  assertEquals('a', obj.test);
  assertEquals('a', obj.getAttribute('test'));

  obj.test = undefined;
  assertEquals(null, obj.test);
  assertFalse(obj.hasAttribute('test'));
});

test('DefinePropertyAttrWithSetter', function() {
  const obj = document.createElement('div');

  let hit = false;

  function onTestSet(value, oldValue) {
    assertEquals(obj, this);
    assertEquals(null, oldValue);
    assertEquals('b', value);
    assertEquals('b', this.test);
    hit = true;
  }
  Object.defineProperty(
      obj, 'test',
      cr.getPropertyDescriptor('test', cr.PropertyKind.ATTR, onTestSet));
  obj.test = 'b';
  assertTrue(hit);
});

test('DefinePropertyAttrEvent', function() {
  const obj = document.createElement('div');
  Object.defineProperty(
      obj, 'test', cr.getPropertyDescriptor('test', cr.PropertyKind.ATTR));

  let count = 0;
  function f(e) {
    assertEquals('testChange', e.type);
    assertEquals('test', e.propertyName);
    assertEquals(null, e.oldValue);
    assertEquals('b', e.newValue);
    count++;
  }

  obj.addEventListener('testChange', f);

  obj.test = null;
  assertEquals(0, count, 'Should not have called the property change listener');

  obj.test = 'b';
  assertEquals('b', obj.test);
  assertEquals(1, count, 'Should have called the property change listener');

  obj.test = 'b';
  assertEquals(1, count);
});

test('DefinePropertyBoolAttr', function() {
  const obj = document.createElement('div');
  Object.defineProperty(
      obj, 'test', cr.getPropertyDescriptor('test', cr.PropertyKind.BOOL_ATTR));

  assertFalse(obj.test);
  assertFalse(obj.hasAttribute('test'));

  obj.test = true;
  assertTrue(obj.test);
  assertTrue(obj.hasAttribute('test'));

  obj.test = false;
  assertFalse(obj.test);
  assertFalse(obj.hasAttribute('test'));
});

test('DefinePropertyBoolAttrEvent', function() {
  const obj = document.createElement('div');
  Object.defineProperty(
      obj, 'test', cr.getPropertyDescriptor('test', cr.PropertyKind.BOOL_ATTR));

  let count = 0;
  function f(e) {
    assertEquals('testChange', e.type);
    assertEquals('test', e.propertyName);
    assertEquals(false, e.oldValue);
    assertEquals(true, e.newValue);
    count++;
  }

  obj.addEventListener('testChange', f);
  obj.test = true;
  assertTrue(obj.test);
  assertEquals(1, count, 'Should have called the property change listener');

  obj.test = true;
  assertEquals(1, count);
});

test('DefinePropertyBoolAttrEventWithHook', function() {
  const obj = document.createElement('div');
  let hit = false;

  function onTestSet(value, oldValue) {
    assertEquals(obj, this);
    assertTrue(this.test);
    assertFalse(oldValue);
    assertTrue(value);
    hit = true;
  }
  Object.defineProperty(
      obj, 'test',
      cr.getPropertyDescriptor('test', cr.PropertyKind.BOOL_ATTR, onTestSet));
  obj.test = true;
  assertTrue(hit);
});

test('DefineWithGetter', function() {
  let v = 0;
  cr.define('foo', function() {
    return {
      get v() {
        return v;
      },
    };
  });

  assertEquals(0, foo.v);

  v = 1;
  assertEquals(1, foo.v);
});
