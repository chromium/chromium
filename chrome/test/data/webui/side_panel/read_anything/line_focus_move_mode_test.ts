// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusCursorMoveMode, LineFocusLineStyleMode, LineFocusModel, LineFocusMovement, LineFocusNoneMoveMode, LineFocusStaticMoveMode, LineFocusStyle} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {MoveModeDelegate} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertLT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('LineFocusMoveMode', () => {
  let model: LineFocusModel;
  let styleMode: LineFocusLineStyleMode;
  let delegate: MoveModeDelegate;
  let sessionEnded: boolean;

  const defaultHeight = 1000;

  function createShortContainer(): HTMLElement {
    const container = document.createElement('p');
    container.innerText =
        'I\'ve heard it said\nThat people come into our lives\nfor a reason.';
    container.style.whiteSpace = 'pre-line';
    document.body.appendChild(container);
    return container;
  }

  setup(() => {
    model = new LineFocusModel();
    styleMode = new LineFocusLineStyleMode(LineFocusStyle.UNDERLINE, model);
    sessionEnded = false;
    delegate = {
      onSessionEnd() {
        sessionEnded = true;
      },
      notifyMove() {},
      notifyScroll() {},
    };
  });

  suite('static mode', () => {
    let mode: LineFocusStaticMoveMode;

    setup(() => {
      // Clearing the DOM should always be done first.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      const readingMode = new FakeReadingMode();
      chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
      mode = new LineFocusStaticMoveMode(model, styleMode, delegate);
    });

    test('getMovement returns STATIC', () => {
      assertEquals(LineFocusMovement.STATIC, mode.getMovement());
    });

    test('onActivated starts session', () => {
      model.setSessionActive(false);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertTrue(started);
      assertTrue(model.isSessionActive());
      assertEquals(styleMode.getStyle(), model.getLastEnabledLineFocusStyle());
    });

    test('onActivated updates positions', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test('onActivated does not restart active session', () => {
      model.setSessionActive(true);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;
      const container = document.createElement('div');

      mode.onActivated(container, 100);

      assertFalse(started);
      assertTrue(model.isSessionActive());
    });
  });

  suite('cursor mode', () => {
    let mode: LineFocusCursorMoveMode;

    setup(() => {
      mode = new LineFocusCursorMoveMode(model, styleMode, delegate);
    });

    test('getMovement returns CURSOR', () => {
      assertEquals(LineFocusMovement.CURSOR, mode.getMovement());
    });

    test('onActivated starts session', () => {
      model.setSessionActive(false);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;
      const container = document.createElement('div');

      mode.onActivated(container, defaultHeight);

      assertTrue(started);
      assertTrue(model.isSessionActive());
      assertEquals(styleMode.getStyle(), model.getLastEnabledLineFocusStyle());
    });

    test('onActivated updates positions', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertEquals(defaultHeight, model.getMaxY());
      assertLT(model.getMinY(), defaultHeight);
      assertEquals(3, model.getTextBounds().length);
    });

    test('onActivated does not restart active session', () => {
      model.setSessionActive(true);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;
      const container = document.createElement('div');

      mode.onActivated(container, defaultHeight);

      assertFalse(started);
      assertTrue(model.isSessionActive());
    });
  });

  suite('none mode', () => {
    let mode: LineFocusNoneMoveMode;

    setup(() => {
      mode = new LineFocusNoneMoveMode(
          model, styleMode, delegate, LineFocusMovement.STATIC);
    });

    test('getMovement returns movement from constructor', () => {
      assertEquals(LineFocusMovement.STATIC, mode.getMovement());

      const cursorMode = new LineFocusNoneMoveMode(
          model, styleMode, delegate, LineFocusMovement.CURSOR);
      assertEquals(LineFocusMovement.CURSOR, cursorMode.getMovement());
    });

    test('onActivated ends active session and resets model', () => {
      model.setSessionActive(true);
      model.setTop(100);
      model.setWindowHeight(100);
      const container = document.createElement('div');

      mode.onActivated(container, 100);

      assertTrue(sessionEnded);
      assertFalse(model.isSessionActive());
      assertEquals(0, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('onActivated does not end inactive session', () => {
      model.setSessionActive(false);
      const container = document.createElement('div');

      mode.onActivated(container, 100);

      assertFalse(sessionEnded);
    });

    test('onActivated does not update positions', () => {
      const container = createShortContainer();

      mode.onActivated(container, defaultHeight);

      assertEquals(0, model.getMaxY());
      assertEquals(0, model.getMinY());
      assertEquals(0, model.getTextBounds().length);
    });
  });
});
