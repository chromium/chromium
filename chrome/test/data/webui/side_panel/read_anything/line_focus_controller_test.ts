// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {currentReadHighlightClass, LineFocusController, LineFocusType, PARENT_OF_HIGHLIGHT_CLASS, previousReadHighlightClass, setInstance, SpeechBrowserProxyImpl, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LineFocusListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertLT, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setContent, setupBasicSpeech} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('LineFocusController', () => {
  const defaultHeight = 1000;
  let lineFocusController: LineFocusController;
  let lineFocusListener: LineFocusListener;
  let lineFocusMoved: boolean;
  let defaultContainer: HTMLElement;
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;
  let readAloudModel: TestReadAloudModelBrowserProxy;

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
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);

    readAloudModel = new TestReadAloudModelBrowserProxy();
    setInstance(readAloudModel);
    readAloudModel.setInitialized(true);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
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
    lineFocusMoved = false;

    lineFocusController.onMouseMove(101);

    assertTrue(lineFocusMoved);
  });

  test('onMouseMove does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    speechController.onPlayPauseToggle(container);
    lineFocusMoved = false;

    lineFocusController.onMouseMove(101);

    assertFalse(lineFocusMoved);
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

    assertNotEquals(NaN, lineFocusController.getTop());
    assertNotEquals(NaN, lineFocusController.getHeight());
    assertNotEquals(oldTop, lineFocusController.getTop());
    assertNotEquals(oldHeight, lineFocusController.getHeight());
    assertTrue(lineFocusMoved);
  });

  test('onTextLocationsChange during speech, updates window position', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = document.createElement('p');
    const text =
        setContent(
            'It well may be\nthat we will never meet again\nin this lifetime' +
                '\nso let me say before we part\nso much of me\n' +
                'is made from what I learned from you\nyou\'ll be with me\n' +
                'like a handprint on my heart\n' +
                'and now whatever way our stories end\n' +
                'Know you have rewritten mine\nby being my friend',
            readAloudModel) as HTMLElement;
    container.appendChild(text);
    document.body.appendChild(container);
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 3}, container, 100);
    lineFocusController.onMouseMove(100);
    const oldTop = lineFocusController.getTop();
    const oldHeight = lineFocusController.getHeight();
    setupBasicSpeech(speech);
    speechController.onPlayPauseToggle(container);
    lineFocusMoved = false;

    lineFocusController.onTextLocationsChange(container, 100);


    const highlights = container.querySelectorAll<HTMLElement>(
        `.${currentReadHighlightClass}`);
    assertEquals(1, highlights.length);
    const newHeight = lineFocusController.getHeight();
    assertTrue(!!newHeight);
    assertEquals(
        highlights.item(0).getBoundingClientRect().bottom,
        lineFocusController.getTop() + newHeight);
    assertNotEquals(oldTop, lineFocusController.getTop());
    assertEquals(oldHeight, newHeight);
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

  test('onTextLocationsChange during speech, updates line position', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = document.createElement('p');
    const text = setContent('that we will never meet again', readAloudModel) as
        HTMLElement;
    container.appendChild(text);
    document.body.appendChild(container);
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, defaultHeight);
    const oldTop = lineFocusController.getTop();
    setupBasicSpeech(speech);
    speechController.onPlayPauseToggle(container);
    lineFocusMoved = false;

    lineFocusController.onTextLocationsChange(container, defaultHeight);

    const highlights = container.querySelectorAll<HTMLElement>(
        `.${currentReadHighlightClass}`);
    assertEquals(1, highlights.length);
    assertEquals(
        highlights.item(0).getBoundingClientRect().bottom,
        lineFocusController.getTop());
    assertNotEquals(oldTop, lineFocusController.getTop());
    assertTrue(lineFocusMoved);
  });

  test('follows current highlights', async () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, defaultHeight);
    lineFocusMoved = false;

    const parentHighlight = document.createElement('span');
    parentHighlight.className = PARENT_OF_HIGHLIGHT_CLASS;
    const innerHighlight = document.createElement('span');
    innerHighlight.className = currentReadHighlightClass;
    innerHighlight.innerText = 'Like a ship blow from it\'s mooring';
    parentHighlight.appendChild(innerHighlight);
    container.appendChild(parentHighlight);
    await microtasksFinished();

    assertEquals(
        parentHighlight.getBoundingClientRect().bottom,
        lineFocusController.getTop());
    assertTrue(lineFocusMoved);
  });

  test('ignores previous highlights', async () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = createShortContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, defaultHeight);
    lineFocusMoved = false;

    const parentHighlight = document.createElement('span');
    parentHighlight.className = PARENT_OF_HIGHLIGHT_CLASS;
    const innerHighlight = document.createElement('span');
    innerHighlight.className = previousReadHighlightClass;
    innerHighlight.innerText = 'By a wind off the sea';
    parentHighlight.appendChild(innerHighlight);
    container.appendChild(parentHighlight);
    await microtasksFinished();

    assertFalse(lineFocusMoved);
  });

  test('snapToNextLine with line moves by line', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = document.createElement('p');
    container.innerText =
        'Like a siege rocked by a sky bird\nin a distant wood';
    document.body.appendChild(container);
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, defaultHeight);
    let oldTop = lineFocusController.getTop();

    // Snap to the first line.
    lineFocusController.snapToNextLine(true, container);
    let newTop = lineFocusController.getTop();
    assertLT(oldTop, newTop);

    // Snap to the last line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(true, container);
    newTop = lineFocusController.getTop();
    assertLT(oldTop, newTop);

    // The container only has two lines, so moving forward should not change
    // position.
    oldTop = newTop;
    lineFocusController.snapToNextLine(true, container);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);

    // Snap back to the first line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(false, container);
    newTop = lineFocusController.getTop();
    assertGT(oldTop, newTop);

    // Moving back again should not change position.
    oldTop = newTop;
    lineFocusController.snapToNextLine(false, container);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
  });

  test('snapToNextLine scrolls to line if out of view', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const height = 100;
    const scroller = document.createElement('div');
    scroller.style.height = `${height}px`;
    scroller.style.overflow = 'auto';
    const container = document.createElement('p');
    container.innerText =
        'Like a siege rocked by a sky bird\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n' +
        'in a distant wood\nin a distant wood\nin a distant wood\n';
    let scrolled = false;
    container.scrollTo = () => {
      scrolled = true;
    };
    scroller.appendChild(container);
    document.body.appendChild(scroller);
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, height);
    let oldTop = lineFocusController.getTop();

    // Snap to the first line.
    lineFocusController.snapToNextLine(true, container);
    let newTop = lineFocusController.getTop();
    // Continue moving to the next line until scrolling occurs.
    while (oldTop < newTop) {
      assertFalse(scrolled);
      oldTop = newTop;
      lineFocusController.snapToNextLine(true, container);
      newTop = lineFocusController.getTop();
    }
    assertTrue(scrolled);

    // Snap to the next line to scroll again.
    oldTop = newTop;
    scrolled = false;
    lineFocusController.snapToNextLine(true, container);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
    assertTrue(scrolled);

    // Snap to the previous line to scroll back up.
    oldTop = newTop;
    scrolled = false;
    lineFocusController.snapToNextLine(false, container);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
    assertTrue(scrolled);
  });

  test('snapToNextLine with window moves by line', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const container = document.createElement('p');
    container.innerText = 'Who can say if I\'ve been changed for the better\n' +
        'But Because I knew you I have been changed for good\n' +
        'And just to clear the air I ask forgiveness\n' +
        'for the things I\'ve done you blame before';
    document.body.appendChild(container);
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.WINDOW, lines: 3}, container, defaultHeight);
    let oldTop = lineFocusController.getTop();

    // Snap to the third line.
    lineFocusController.snapToNextLine(true, container);
    let newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);

    // Snap to the last line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(true, container);
    newTop = lineFocusController.getTop();
    assertLT(oldTop, newTop);

    // Moving forward should not change position.
    oldTop = newTop;
    lineFocusController.snapToNextLine(true, container);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);

    // Snap back to the third line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(false, container);
    newTop = lineFocusController.getTop();
    assertGT(oldTop, newTop);

    // Moving back again should not change position since the window is 3 lines
    // long and we are already at the third line.
    oldTop = newTop;
    lineFocusController.snapToNextLine(false, container);
    newTop = lineFocusController.getTop();
    assertEquals(oldTop, newTop);
  });

  test('snapToNextLine does nothing when speech active', () => {
    readAloudModel.setInitialized(false);
    const container = createLongContainer();
    lineFocusController.onLineFocusChange(
        {type: LineFocusType.LINE, lines: 1}, container, defaultHeight);
    chrome.readingMode.isLineFocusEnabled = true;
    speechController.onPlayPauseToggle(container);
    lineFocusMoved = false;

    lineFocusController.snapToNextLine(true, container);
    lineFocusController.snapToNextLine(false, container);

    assertFalse(lineFocusMoved);
  });
});
