// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ReadAnythingElement, WordBoundaryState} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {WordBoundaryMode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('WordBoundariesUsedForSpeech', () => {
  let app: ReadAnythingElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let playPauseButton: CrIconButtonElement;

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
    playPauseButton =
        app.$.toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
            '#play-pause')!;
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
  });

  suite('by default', () => {
    test('wordBoundaryState in default state', () => {
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(state.mode, WordBoundaryMode.NO_BOUNDARIES);
      assertEquals(state.previouslySpokenIndex, 0);
      assertEquals(state.speechUtteranceStartIndex, 0);
    });
  });

  suite('during speech with no word boundaries ', () => {
    setup(() => {
      playPauseButton.click();
    });

    test('wordBoundaryState in default state during speech', () => {
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(state.mode, WordBoundaryMode.NO_BOUNDARIES);
      assertEquals(state.previouslySpokenIndex, 0);
      assertEquals(state.speechUtteranceStartIndex, 0);
    });
  });

  suite('by default', () => {
    test('wordBoundaryState in default state', () => {
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(state.mode, WordBoundaryMode.NO_BOUNDARIES);
      assertEquals(state.previouslySpokenIndex, 0);
      assertEquals(state.speechUtteranceStartIndex, 0);
    });
  });

  suite('during speech with one initial word boundary ', () => {
    setup(() => {
      playPauseButton.click();
      app.updateBoundary(10);
    });

    test('wordBoundaryState uses most recent boundary', () => {
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(state.mode, WordBoundaryMode.BOUNDARY_DETECTED);
      assertEquals(state.previouslySpokenIndex, 10);
      assertEquals(state.speechUtteranceStartIndex, 0);
    });

    test(
        'pause / play toggle updates speechResumedOnPreviousWordBoundary',
        () => {
          playPauseButton.click();
          playPauseButton.click();
          const state: WordBoundaryState = app.wordBoundaryState;
          assertEquals(state.mode, WordBoundaryMode.BOUNDARY_DETECTED);
          assertEquals(state.previouslySpokenIndex, 0);
          assertEquals(state.speechUtteranceStartIndex, 10);
        });

    test('word boundaries update after play / pause toggle', () => {
      playPauseButton.click();
      playPauseButton.click();
      app.updateBoundary(3);
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(state.mode, WordBoundaryMode.BOUNDARY_DETECTED);
      assertEquals(state.previouslySpokenIndex, 3);
      assertEquals(state.speechUtteranceStartIndex, 10);
    });

    test('word boundaries correct after multiple play / pause toggles', () => {
      playPauseButton.click();
      playPauseButton.click();
      app.updateBoundary(3);
      playPauseButton.click();
      playPauseButton.click();
      app.updateBoundary(7);
      playPauseButton.click();
      playPauseButton.click();
      app.updateBoundary(1);
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(state.mode, WordBoundaryMode.BOUNDARY_DETECTED);
      assertEquals(state.previouslySpokenIndex, 1);
      assertEquals(state.speechUtteranceStartIndex, 20);
    });
  });

  suite('during speech with multiple word boundaries ', () => {
    setup(() => {
      playPauseButton.click();
      app.updateBoundary(10);
      app.updateBoundary(15);
      app.updateBoundary(25);
      app.updateBoundary(40);
    });

    test('wordBoundaryState in default state during speech', () => {
      const state: WordBoundaryState = app.wordBoundaryState;
      assertEquals(state.mode, WordBoundaryMode.BOUNDARY_DETECTED);
      assertEquals(state.previouslySpokenIndex, 40);
      assertEquals(state.speechUtteranceStartIndex, 0);
    });
  });
});
