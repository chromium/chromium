// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AppElement, WordBoundaryState} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {PauseActionSource, ToolbarEvent, WordBoundaryMode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice, emitEvent, suppressInnocuousErrors} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('WordBoundariesUsedForSpeech', () => {
  let app: AppElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;

  // root htmlTag='#document' id=1
  // ++link htmlTag='a' url='http://www.google.com' id=2
  // ++++staticText name='This is a link.' id=3
  // ++link htmlTag='a' url='http://www.youtube.com' id=4
  // ++++staticText name='This is another link.' id=5
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4],
      },
      {
        id: 2,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.google.com',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is a link.',
      },
      {
        id: 4,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.youtube.com',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This is another link.',
      },
    ],
  };

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    app.enabledLangs = ['en-US'];
    const selectedVoice =
        createSpeechSynthesisVoice({lang: 'en', name: 'Kristi'});
    emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice}});
    flush();
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
  });

  test('by default wordBoundaryState in default state', () => {
    const state: WordBoundaryState = app.wordBoundaryState;
    assertEquals(WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED, state.mode);
    assertEquals(0, state.previouslySpokenIndex);
    assertEquals(0, state.speechUtteranceStartIndex);
  });

  test(
      'during speech with no boundaries wordBoundaryState in default state',
      () => {
        app.playSpeech();
        const state: WordBoundaryState = app.wordBoundaryState;
        assertEquals(WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED, state.mode);
        assertEquals(0, state.previouslySpokenIndex);
        assertEquals(0, state.speechUtteranceStartIndex);
      });

  test('by default, wordBoundaryState in default state', () => {
    const state: WordBoundaryState = app.wordBoundaryState;
    assertEquals(WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED, state.mode);
    assertEquals(0, state.previouslySpokenIndex);
    assertEquals(0, state.speechUtteranceStartIndex);
  });

  suite('during speech with one initial word boundary ', () => {
    setup(() => {
      app.playSpeech();
      app.updateBoundary(10);
    });

    test('wordBoundaryState uses most recent boundary', () => {
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(WordBoundaryMode.BOUNDARY_DETECTED, state.mode);
      assertEquals(10, state.previouslySpokenIndex);
      assertEquals(0, state.speechUtteranceStartIndex);
    });

    test(
        'pause / play toggle updates speechResumedOnPreviousWordBoundary',
        () => {
          app.stopSpeech(PauseActionSource.BUTTON_CLICK);
          app.playSpeech();
          const state: WordBoundaryState = app.wordBoundaryState;
          assertEquals(WordBoundaryMode.BOUNDARY_DETECTED, state.mode);
          assertTrue(!!app.getSpeechSynthesisVoice());
          assertEquals(0, state.previouslySpokenIndex);
          assertEquals(10, state.speechUtteranceStartIndex);
        });

    test('word boundaries update after pause / play toggle', () => {
      app.stopSpeech(PauseActionSource.BUTTON_CLICK);
      app.playSpeech();
      app.updateBoundary(3);
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(WordBoundaryMode.BOUNDARY_DETECTED, state.mode);
      assertEquals(3, state.previouslySpokenIndex);
      assertEquals(10, state.speechUtteranceStartIndex);
    });

    test('word boundaries correct after multiple pause / play toggles', () => {
      app.stopSpeech(PauseActionSource.BUTTON_CLICK);
      app.playSpeech();
      app.updateBoundary(3);
      app.stopSpeech(PauseActionSource.BUTTON_CLICK);
      app.playSpeech();
      app.updateBoundary(7);
      app.stopSpeech(PauseActionSource.BUTTON_CLICK);
      app.playSpeech();
      app.updateBoundary(1);
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(WordBoundaryMode.BOUNDARY_DETECTED, state.mode);
      assertEquals(1, state.previouslySpokenIndex);
      assertEquals(20, state.speechUtteranceStartIndex);
    });
  });

  test(
      'with multiple word boundaries wordBoundaryState in default state during speech',
      () => {
        app.playSpeech();
        app.updateBoundary(10);
        app.updateBoundary(15);
        app.updateBoundary(25);
        app.updateBoundary(40);

        const state: WordBoundaryState = app.wordBoundaryState;
        assertEquals(WordBoundaryMode.BOUNDARY_DETECTED, state.mode);
        assertEquals(40, state.previouslySpokenIndex);
        assertEquals(0, state.speechUtteranceStartIndex);
      });

  test('after voice change resets to unsupported boundary mode', () => {
    app.playSpeech();
    app.updateBoundary(10);
    assertEquals(
        WordBoundaryMode.BOUNDARY_DETECTED, app.wordBoundaryState.mode);

    const selectedVoice =
        createSpeechSynthesisVoice({lang: 'es', name: 'Lauren'});
    emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice}});
    flush();

    // After a voice change, the word boundary state has been reset.
    const state: WordBoundaryState = app.wordBoundaryState;
    assertEquals(WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED, state.mode);
    assertEquals(0, state.previouslySpokenIndex);
    assertEquals(0, state.speechUtteranceStartIndex);

    // After another boundary event, the boundary mode is set to
    // BOUNDARY_DETECTED again.
    app.updateBoundary(15);
    assertEquals(
        WordBoundaryMode.BOUNDARY_DETECTED, app.wordBoundaryState.mode);
  });

  test(
      'after voice change to same language does not reset word boundary mode',
      () => {
        app.playSpeech();
        app.updateBoundary(10);
        assertEquals(
            WordBoundaryMode.BOUNDARY_DETECTED, app.wordBoundaryState.mode);

        const selectedVoice =
            createSpeechSynthesisVoice({lang: 'en', name: 'Lauren'});
        emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice}});
        flush();

        // After a voice change to the same language, the word boundary state
        // has stayed the same.
        const state: WordBoundaryState = app.wordBoundaryState;
        assertEquals(WordBoundaryMode.BOUNDARY_DETECTED, state.mode);
        assertEquals(0, state.previouslySpokenIndex);
        assertEquals(10, state.speechUtteranceStartIndex);
      });
});
