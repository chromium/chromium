// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement, ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {stubAnimationFrame, suppressInnocuousErrors} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('PhraseHighlighting', () => {
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

    // Use a tree with just one sentence. For the actual implementation of
    // phrase segmentation, a more realistic example would be to use
    // setSimpleAxTreeWithText instead.
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
  });

  function computeStyle(style: string) {
    return window.getComputedStyle(app.$.container).getPropertyValue(style);
  }

  suite('changing the highlight from the menu', () => {
    let toolbar: ReadAnythingToolbarElement;
    let highlightButton: CrIconButtonElement;
    let options: HTMLButtonElement[];

    setup(() => {
      toolbar = app.$.toolbar;
      highlightButton =
          toolbar.$.toolbarContainer.querySelector<CrIconButtonElement>(
              '#highlight')!;
      stubAnimationFrame();
      highlightButton.click();
      flush();

      const menu = toolbar.$.highlightMenu.$.menu.$.lazyMenu.get();
      assertTrue(menu.open);
      options = Array.from(
          menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
    });

    test('with word highlighting on, word is highlighted', () => {
      options[1]!.click();
      flush();
      assertEquals(
          chrome.readingMode.highlightGranularity,
          chrome.readingMode.wordHighlighting);

      app.updateBoundary(0);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'This ');
    });

    test('with phrase highlighting on, phrase is highlighted', () => {
      options[2]!.click();
      flush();
      assertEquals(
          chrome.readingMode.highlightGranularity,
          chrome.readingMode.phraseHighlighting);

      app.updateBoundary(0);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'This is a ');
    });

    test('with sentence highlighting on, sentence is highlighted', () => {
      options[3]!.click();
      flush();
      assertEquals(
          chrome.readingMode.highlightGranularity,
          chrome.readingMode.sentenceHighlighting);

      app.updateBoundary(0);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'This is a link.');
    });

    test('with highlighting off, highlight is invisible', () => {
      options[4]!.click();
      flush();
      assertEquals(
          chrome.readingMode.highlightGranularity,
          chrome.readingMode.noHighlighting);

      app.updateBoundary(0);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals('transparent', computeStyle('--current-highlight-bg-color'));
    });
  });

  suite('after a word boundary', () => {
    setup(() => {
      app.updateBoundary(0);
    });

    test('initially, phrase is highlighted', () => {
      chrome.readingMode.onHighlightGranularityChanged(
          chrome.readingMode.phraseHighlighting);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'This is a ');
    });

    test('phrase highlight same after second word boundary', () => {
      chrome.readingMode.onHighlightGranularityChanged(
          chrome.readingMode.phraseHighlighting);
      app.updateBoundary(5);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'This is a ');
    });

    test('phrase highlighting highlights second phrase', () => {
      chrome.readingMode.onHighlightGranularityChanged(
          chrome.readingMode.phraseHighlighting);
      app.updateBoundary(10);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'link.');
    });

    // Tests for checking correct handling of auto granularity.
    test(
        'with auto highlighting and rate of 2, sentence highlight used', () => {
          chrome.readingMode.onHighlightGranularityChanged(
              chrome.readingMode.sentenceHighlighting);
          app.playSpeech();
          const currentHighlight =
              app.$.container.querySelector('.current-read-highlight');
          assertTrue(currentHighlight !== undefined);
          assertEquals('This is a link.', currentHighlight!.textContent);
        });

    test('with auto highlighting and rate of 1, phrase highlight used', () => {
      chrome.readingMode.onHighlightGranularityChanged(
          chrome.readingMode.autoHighlighting);
      chrome.readingMode.onSpeechRateChange(1);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals('This is a ', currentHighlight!.textContent);
    });

    test('with auto highlighting and rate of 0.5, word highlight used', () => {
      chrome.readingMode.onHighlightGranularityChanged(
          chrome.readingMode.autoHighlighting);
      chrome.readingMode.onSpeechRateChange(0.5);
      app.playSpeech();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals('This ', currentHighlight!.textContent);
    });

    // TODO(b/364327601): Add tests for unsupported language handling.
  });
});
