// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals, assertTrue, assertFalse} from '../../../chai_assert.js';
// #import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
// clang-format on

/* #ignore */ /* @const */ var EventTarget;

function setUp() {
  /* #ignore */ EventTarget = cr.EventTarget;
}

function testFunctionListener() {
  var fi = 0;
  function f(e) {
    fi++;
  }

  var gi = 0;
  function g(e) {
    gi++;
  }

  var et = new EventTarget;
  et.addEventListener('f', f);
  et.addEventListener('g', g);

  // Adding again should not cause it to be called twice
  et.addEventListener('f', f);
  et.dispatchEvent(new Event('f'));
  assertEquals(1, fi, 'Should have been called once');
  assertEquals(0, gi);

  et.removeEventListener('f', f);
  et.dispatchEvent(new Event('f'));
  assertEquals(1, fi, 'Should not have been called again');

  et.dispatchEvent(new Event('g'));
  assertEquals(1, gi, 'Should have been called once');
}

function testHandleEvent() {
  var fi = 0;
  var f = /** @type {!EventListener} */ ({
    handleEvent: function(e) {
      fi++;
    }
  });

  var gi = 0;
  var g = /** @type {!EventListener} */ ({
    handleEvent: function(e) {
      gi++;
    }
  });

  var et = new EventTarget;
  et.addEventListener('f', f);
  et.addEventListener('g', g);

  // Adding again should not cause it to be called twice
  et.addEventListener('f', f);
  et.dispatchEvent(new Event('f'));
  assertEquals(1, fi, 'Should have been called once');
  assertEquals(0, gi);

  et.removeEventListener('f', f);
  et.dispatchEvent(new Event('f'));
  assertEquals(1, fi, 'Should not have been called again');

  et.dispatchEvent(new Event('g'));
  assertEquals(1, gi, 'Should have been called once');
}

function testPreventDefault() {
  var i = 0;
  function prevent(e) {
    i++;
    e.preventDefault();
  }

  var j = 0;
  function pass(e) {
    j++;
  }

  var et = new EventTarget;
  et.addEventListener('test', pass);

  assertTrue(et.dispatchEvent(new Event('test', {cancelable: true})));
  assertEquals(1, j);

  et.addEventListener('test', prevent);

  console.log('NOW');
  assertFalse(et.dispatchEvent(new Event('test', {cancelable: true})));
  assertEquals(2, j);
  assertEquals(1, i);
}

Object.assign(window, {
  setUp,
  testFunctionListener,
  testHandleEvent,
  testPreventDefault,
});
