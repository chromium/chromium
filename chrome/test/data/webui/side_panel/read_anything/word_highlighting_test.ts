// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, PauseActionSource, WordBoundaryMode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice, emitEvent, setSimpleAxTreeWithText, suppressInnocuousErrors, waitForPlayFromSelection} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('WordHighlighting', () => {
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
    flush();
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
  });

  // TODO(b/301131238): Before enabling the feature flag, ensure we've
  // added more robust tests.
  suite('with word boundary flag enabled after a word boundary', () => {
    setup(() => {
      app.updateBoundary(10);
    });

    test('word highlight used', () => {
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);

      // Sometimes the word returned can be "link", "link.", or "link. " which
      // can create flaky tests. Therefore, just check that the highlighted
      // text starts with "link" and isn't longer than the string would be if it
      // were "link. "
      // TODO(b/301131238): Investigate why there's a discrepancy here.
      assertTrue(currentHighlight!.textContent!.startsWith('link'));
      assertTrue(currentHighlight!.textContent!.length < 6);
    });

    test('with rate over 1 sentence highlight used', () => {
      chrome.readingMode.onSpeechRateChange(2);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals('This is a link.', currentHighlight!.textContent);
    });
  });

  test('with no word boundary sentence highlight used', () => {
    app.playSpeech();
    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    assertEquals('This is a link.', currentHighlight.textContent);
  });

  test(
      'word highlighting with multiple punctuation marks skips highlight',
      () => {
        setSimpleAxTreeWithText('.?!\'\",(){}[]');
        app.updateBoundary(10);
        app.playSpeech();
        const currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        assertFalse(!!currentHighlight);
      });

  test(
      'word highlighting with single alphabet character does not skip highlight',
      () => {
        setSimpleAxTreeWithText('a');
        app.updateBoundary(0);
        app.playSpeech();
        const currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        assertTrue(!!currentHighlight);
        assertEquals('a', currentHighlight.textContent);
      });

  test('word highlighting skipping', () => {
    const toTest =
        ['[', ']', '(', ')', '.', ',', '?', '!', '{', '}', '\'', '\"'];

    for (const char of toTest) {
      setSimpleAxTreeWithText(char);
      app.updateBoundary(0);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertFalse(!!currentHighlight);
    }
  });

  test('on speaking from selection, word boundary state reset', async () => {
    const anchorIndex = 1;
    const focusIndex = 2;
    const anchorOffset = 0;
    const focusOffset = 1;
    app.playSpeech();
    app.updateBoundary(2);
    app.stopSpeech(PauseActionSource.BUTTON_CLICK);

    // Update the selection directly on the document.
    const spans = app.$.container.querySelectorAll('span');
    assertEquals(spans.length, 3);
    const anchor = spans[anchorIndex]!;
    const focus = spans[focusIndex]!;
    const range = document.createRange();
    range.setStart(anchor, anchorOffset);
    range.setEnd(focus, focusOffset);
    const selection = document.getSelection();
    assertTrue(!!selection);
    selection.addRange(range);

    // Play from selection.
    app.playSpeech();
    await waitForPlayFromSelection();

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');

    // Verify that we're highlighting from the selected point.
    assertTrue(!!currentHighlight);
    assertTrue(!!currentHighlight.textContent);
    assertEquals('This ', currentHighlight.textContent);

    // Verify that the word boundary state has been reset.
    assertEquals(WordBoundaryMode.NO_BOUNDARIES, app.wordBoundaryState.mode);
  });

  test('sentence highlight used with espeak voice', () => {
    const selectedVoice =
        createSpeechSynthesisVoice({lang: 'en', name: 'Kristi eSpeak'});
    emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice}});
    flush();

    const sentence = 'Hello, how are you!';

    setSimpleAxTreeWithText(sentence);
    app.updateBoundary(0);
    app.playSpeech();
    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(currentHighlight !== undefined);
    assertEquals(sentence, currentHighlight!.textContent);
  });
});
