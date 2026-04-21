// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LineFocusCursorMoveMode, LineFocusLineStyleMode, LineFocusModel, LineFocusMovement, LineFocusNoneMoveMode, LineFocusStaticMoveMode, LineFocusStyle} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {MoveModeDelegate} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('LineFocusMoveMode', () => {
  let model: LineFocusModel;
  let styleMode: LineFocusLineStyleMode;
  let delegate: MoveModeDelegate;
  let sessionEnded: boolean;

  setup(() => {
    model = new LineFocusModel();
    styleMode = new LineFocusLineStyleMode(LineFocusStyle.UNDERLINE, model);
    sessionEnded = false;
    delegate = {
      onSessionEnd() {
        sessionEnded = true;
      },
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

      mode.onActivated();

      assertTrue(started);
      assertTrue(model.isSessionActive());
      assertEquals(styleMode.getStyle(), model.getLastEnabledLineFocusStyle());
    });

    test('onActivated does not restart active session', () => {
      model.setSessionActive(true);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;

      mode.onActivated();

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

      mode.onActivated();

      assertTrue(started);
      assertTrue(model.isSessionActive());
      assertEquals(styleMode.getStyle(), model.getLastEnabledLineFocusStyle());
    });

    test('onActivated does not restart active session', () => {
      model.setSessionActive(true);
      let started = false;
      chrome.readingMode.startLineFocusSession = () => started = true;

      mode.onActivated();

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

      mode.onActivated();

      assertTrue(sessionEnded);
      assertFalse(model.isSessionActive());
      assertEquals(0, model.getTop());
      assertEquals(0, model.getWindowHeight());
    });

    test('onActivated does not end inactive session', () => {
      model.setSessionActive(false);
      mode.onActivated();
      assertFalse(sessionEnded);
    });
  });
});
