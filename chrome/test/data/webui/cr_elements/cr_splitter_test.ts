// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';

import type {CrSplitterElement} from 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('cr-splitter', function() {
  let crSplitter: CrSplitterElement;

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
      <div id="previous"></div>
      <cr-splitter id="splitter"></cr-splitter>
      <div id="next"></div>`;

    crSplitter = document.querySelector('#splitter')!;
  });

  test('ignores right mouse', function() {
    const downRight =
        new MouseEvent('mousedown', {button: 1, cancelable: true});
    assertTrue(crSplitter.dispatchEvent(downRight));
    assertFalse(downRight.defaultPrevented);

    const downLeft = new MouseEvent('mousedown', {button: 0, cancelable: true});
    assertFalse(crSplitter.dispatchEvent(downLeft));
    assertTrue(downLeft.defaultPrevented);
  });

  test('resize previous element', function() {
    crSplitter.resizeNextElement = false;

    const previousElement = document.getElementById('previous')!;
    previousElement.style.width = '0px';
    const beforeWidth = previousElement.getBoundingClientRect().width;

    const down =
        new MouseEvent('mousedown', {button: 0, cancelable: true, clientX: 0});
    crSplitter.dispatchEvent(down);

    let move =
        new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 50});
    crSplitter.dispatchEvent(move);

    move = new MouseEvent(
        'mousemove', {button: 0, cancelable: true, clientX: 100});
    crSplitter.dispatchEvent(move);

    const up =
        new MouseEvent('mouseup', {button: 0, cancelable: true, clientX: 100});
    crSplitter.dispatchEvent(up);

    const afterWidth = previousElement.getBoundingClientRect().width;
    assertEquals(100, afterWidth - beforeWidth);
  });

  test('resize next element', function() {
    crSplitter.resizeNextElement = true;
    const nextElement = document.getElementById('next')!;
    nextElement.style.width = '0px';
    const beforeWidth = nextElement.getBoundingClientRect().width;

    const down = new MouseEvent(
        'mousedown', {button: 0, cancelable: true, clientX: 100});
    crSplitter.dispatchEvent(down);

    let move =
        new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 50});
    crSplitter.dispatchEvent(move);

    move =
        new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 0});
    crSplitter.dispatchEvent(move);

    const up =
        new MouseEvent('mouseup', {button: 0, cancelable: true, clientX: 0});
    crSplitter.dispatchEvent(up);

    const afterWidth = nextElement.getBoundingClientRect().width;
    assertEquals(100, afterWidth - beforeWidth);
  });
});
