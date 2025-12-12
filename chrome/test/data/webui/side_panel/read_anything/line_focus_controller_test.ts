// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusController, type LineFocusListener, LineFocusType} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertLT, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('LineFocusController', () => {
  const defaultHeight = 1000;
  let lineFocusController: LineFocusController;
  let lineFocusListener: LineFocusListener;
  let lineFocusMoved: boolean;
  let defaultContainer: HTMLElement;

  function createShortContainer(): HTMLElement {
    const container = document.createElement('p');
    container.innerText =
        'I\'ve heard it said\nThat people come into our lives\nfor a reason.';
    document.body.appendChild(container);
    return container;
  }

  function createLongContainer(): HTMLElement {
    const container = document.createElement('p');
    container.innerText =
        'Bringing something we must learn\nAnd we are lead to those\n' +
        'who help us most to grow\nif we let them and we help them in return\n' +
        'Now I don\'t know if I believe that\'s true\n' +
        'But I know I\'m who I am today because I met you';
    document.body.appendChild(container);
    return container;
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    lineFocusController = new LineFocusController();
    lineFocusMoved = false;
    lineFocusListener = {
      onLineFocusMove() {
        lineFocusMoved = true;
      },
    };
    lineFocusController.addListener(lineFocusListener);
    defaultContainer = document.createElement('div');
  });

  test('isEnabled is false by default', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    assertFalse(lineFocusController.isEnabled());
  });

  test('isEnabled is true for line', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);
    assertTrue(lineFocusController.isEnabled());
  });

  test('isEnabled is true for window', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);
    assertTrue(lineFocusController.isEnabled());
  });

  test('isEnabled is false for none type', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);

    lineFocusController.onLineFocusChange(
        {type: LineFocusType.NONE, lines: 1}, defaultContainer, defaultHeight);

    assertFalse(lineFocusController.isEnabled());
  });

  test('isEnabled is false with flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);
    assertFalse(lineFocusController.isEnabled());
  });

  test('onLineFocusChange updates current line focus', () => {
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusType.LINE, lineFocusController.getCurrentLineFocusType());

    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 1}, defaultContainer,
        defaultHeight);
    assertEquals(
        LineFocusType.WINDOW, lineFocusController.getCurrentLineFocusType());

    lineFocusController.onLineFocusChange(
        {type: LineFocusType.NONE, lines: 1}, defaultContainer, defaultHeight);
    assertEquals(
        LineFocusType.NONE, lineFocusController.getCurrentLineFocusType());
  });

  test('onLineFocusChange to line updates position', () => {
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);

    assertEquals(0, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
  });

  test('onLineFocusChange to window updates position and height', () => {
    const container = createShortContainer();

    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 3}, container, defaultHeight);

    assertEquals(container.offsetTop, lineFocusController.getTop());
    assertLT(0, lineFocusController.getHeight()!);
  });

  test('onLineFocusChange window sizes should be different heights', () => {
    const container = createShortContainer();

    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 3}, container, defaultHeight);
    const height1 = lineFocusController.getHeight();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 1}, container, defaultHeight);
    const height2 = lineFocusController.getHeight();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 5}, container, defaultHeight);
    const height3 = lineFocusController.getHeight();

    assertTrue(!!height1);
    assertTrue(!!height2);
    assertTrue(!!height3);
    assertNotEquals(height1, height2);
    assertNotEquals(height2, height3);
  });

  test('onLineFocusChange to none resets position and height', () => {
    const container = createShortContainer();

    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 3}, container, defaultHeight);
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.NONE, lines: 3}, container, defaultHeight);

    assertEquals(0, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
  });

  test('onMouseMove does nothing if flag disabled', () => {
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = false;

    lineFocusController.onMouseMove(101);

    assertFalse(lineFocusMoved);
  });

  test('onMouseMove notifies listeners', () => {
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.onMouseMove(101);

    assertTrue(lineFocusMoved);
  });

  test('onMouseMove sets new line position', () => {
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, defaultContainer, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    const newPos = 102;

    lineFocusController.onMouseMove(newPos);

    assertEquals(newPos, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
  });

  test('onMouseMove honors container top with line', () => {
    const container = createShortContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.onMouseMove(0);

    assertLT(0, container.offsetTop);
    assertEquals(container.offsetTop, lineFocusController.getTop());
    assertFalse(!!lineFocusController.getHeight());
  });

  test('onMouseMove sets new window position and height', () => {
    const container = createLongContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 3}, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    const newPos = container.offsetTop + 100;

    lineFocusController.onMouseMove(newPos);

    const height = lineFocusController.getHeight();
    assertTrue(!!height);
    assertLT(0, height);
    assertEquals(newPos - height, lineFocusController.getTop());
  });

  test('onMouseMove honors container top with height', () => {
    const container = createShortContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 5}, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;

    lineFocusController.onMouseMove(0);

    assertLT(0, container.offsetTop);
    assertEquals(container.offsetTop, lineFocusController.getTop());
    const height = lineFocusController.getHeight();
    assertTrue(!!height);
    assertLT(0, height);
  });

  test('onTextLocationsChange updates window position and height', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 3}, container, defaultHeight);
    lineFocusController.onMouseMove(100);
    const oldTop = lineFocusController.getTop();
    const oldHeight = lineFocusController.getHeight();
    const heading = document.createElement('h1');
    heading.innerText =
        'Like a comet pulled from orbit as\n\n\n\n\nas it passes the sun\n' +
        'Like a stream that meets a boulder\n\nhalfway through the woods';
    document.body.appendChild(heading);
    lineFocusMoved = false;

    console.error('height', heading.offsetHeight);
    lineFocusController.onTextLocationsChange(heading, defaultHeight);

    assertNotEquals(oldTop, lineFocusController.getTop(), 'top');
    assertNotEquals(oldHeight, lineFocusController.getHeight(), 'height');
    assertTrue(lineFocusMoved);
  });

  test('onTextLocationsChange keeps line position', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createLongContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, defaultHeight);
    lineFocusController.onMouseMove(100);
    const oldTop = lineFocusController.getTop();
    const heading = document.createElement('h1');
    heading.innerText =
        'Who can say if I\'ve been changed for the better\n\n\n\n\nBut\n' +
        'Because I knew you\n\nI have been changed for good';
    document.body.appendChild(heading);
    lineFocusMoved = false;

    lineFocusController.onTextLocationsChange(heading, defaultHeight);

    assertEquals(oldTop, lineFocusController.getTop());
    assertFalse(lineFocusMoved);
  });
});
