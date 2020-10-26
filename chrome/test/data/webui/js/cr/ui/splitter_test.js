// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {decorate} from 'chrome://resources/js/cr/ui.m.js';
// #import {Splitter} from 'chrome://resources/js/cr/ui/splitter.m.js';

/* #export */ function setUp() {
  const html = `
    <div id="previous"></div>
    <div id="splitter"></div>
    <div id="next"></div>
  `;
  document.body.innerHTML = html;
}

/* #export */ function testSplitter_IgnoresRightMouse() {
  var splitter = document.getElementById('splitter');
  cr.ui.decorate(splitter, cr.ui.Splitter);

  var downRight = new MouseEvent('mousedown', {button: 1, cancelable: true});
  assertTrue(splitter.dispatchEvent(downRight));
  assertFalse(downRight.defaultPrevented);

  var downLeft = new MouseEvent('mousedown', {button: 0, cancelable: true});
  assertFalse(splitter.dispatchEvent(downLeft));
  assertTrue(downLeft.defaultPrevented);
}

/* #export */ function testSplitter_ResizePreviousElement() {
  var splitter = document.getElementById('splitter');
  cr.ui.decorate(splitter, cr.ui.Splitter);
  splitter.resizeNextElement = false;

  var previousElement = document.getElementById('previous');
  previousElement.style.width = '0px';
  var beforeWidth = parseFloat(previousElement.style.width);

  var down =
      new MouseEvent('mousedown', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(down);

  var move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 50});
  splitter.dispatchEvent(move);

  move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(move);

  var up =
      new MouseEvent('mouseup', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(up);

  var afterWidth = parseFloat(previousElement.style.width);
  assertEquals(100, afterWidth - beforeWidth);
}

/* #export */ function testSplitter_ResizeNextElement() {
  var splitter = document.getElementById('splitter');
  cr.ui.decorate(splitter, cr.ui.Splitter, true);
  splitter.resizeNextElement = true;
  var nextElement = document.getElementById('next');
  nextElement.style.width = '0px';
  var beforeWidth = parseFloat(nextElement.style.width);

  var down =
      new MouseEvent('mousedown', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(down);

  var move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 50});
  splitter.dispatchEvent(move);

  move = new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(move);

  var up = new MouseEvent('mouseup', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(up);

  var afterWidth = parseFloat(nextElement.style.width);
  assertEquals(100, afterWidth - beforeWidth);
}

Object.assign(window, {
  setUp,
  testSplitter_IgnoresRightMouse,
  testSplitter_ResizePreviousElement,
  testSplitter_ResizeNextElement,
});
