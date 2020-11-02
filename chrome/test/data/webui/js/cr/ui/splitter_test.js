// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals, assertTrue, assertFalse} from '../../../chai_assert.js';
// #import {getRequiredElement} from 'chrome://resources/js/util.m.js';
// #import {decorate} from 'chrome://resources/js/cr/ui.m.js';
// #import {Splitter} from 'chrome://resources/js/cr/ui/splitter.m.js';
// clang-format on

function setUp() {
  const html = `
    <div id="previous"></div>
    <div id="splitter"></div>
    <div id="next"></div>
  `;
  document.body.innerHTML = html;
}

function testSplitter_IgnoresRightMouse() {
  const splitter = getRequiredElement('splitter');
  cr.ui.decorate(splitter, cr.ui.Splitter);

  const downRight = new MouseEvent('mousedown', {button: 1, cancelable: true});
  assertTrue(splitter.dispatchEvent(downRight));
  assertFalse(downRight.defaultPrevented);

  const downLeft = new MouseEvent('mousedown', {button: 0, cancelable: true});
  assertFalse(splitter.dispatchEvent(downLeft));
  assertTrue(downLeft.defaultPrevented);
}

function testSplitter_ResizePreviousElement() {
  const splitter = getRequiredElement('splitter');
  cr.ui.decorate(splitter, cr.ui.Splitter);
  splitter.resizeNextElement = false;

  const previousElement = document.getElementById('previous');
  previousElement.style.width = '0px';
  const beforeWidth = parseFloat(previousElement.style.width);

  const down =
      new MouseEvent('mousedown', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(down);

  let move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 50});
  splitter.dispatchEvent(move);

  move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(move);

  const up =
      new MouseEvent('mouseup', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(up);

  const afterWidth = parseFloat(previousElement.style.width);
  assertEquals(100, afterWidth - beforeWidth);
}

function testSplitter_ResizeNextElement() {
  const splitter = getRequiredElement('splitter');
  cr.ui.decorate(splitter, cr.ui.Splitter);
  splitter.resizeNextElement = true;
  const nextElement = document.getElementById('next');
  nextElement.style.width = '0px';
  const beforeWidth = parseFloat(nextElement.style.width);

  const down =
      new MouseEvent('mousedown', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(down);

  let move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 50});
  splitter.dispatchEvent(move);

  move = new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(move);

  const up =
      new MouseEvent('mouseup', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(up);

  const afterWidth = parseFloat(nextElement.style.width);
  assertEquals(100, afterWidth - beforeWidth);
}

Object.assign(window, {
  setUp,
  testSplitter_IgnoresRightMouse,
  testSplitter_ResizePreviousElement,
  testSplitter_ResizeNextElement,
});
