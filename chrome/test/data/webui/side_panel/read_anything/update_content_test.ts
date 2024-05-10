// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeTreeBuilder} from './fake_tree_builder.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('UpdateContent', () => {
  let app: ReadAnythingElement;

  const textNodeIds = [3, 5, 7, 9];
  const texts = [
    'If there\'s a prize for rotten judgment',
    'I guess I\'ve already won that',
    'No one is worth the aggravation',
    'That\'s ancient history, been there, done that!',
  ];

  setup(() => {
    suppressInnocuousErrors();
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    new FakeTreeBuilder()
        .root(1)
        .addTag(2, /* parentId= */ 1, 'p')
        .addText(textNodeIds[0]!!, /* parentId= */ 2, texts[0]!)
        .addTag(4, /* parentId= */ 1, 'p')
        .addText(textNodeIds[1]!, /* parentId= */ 4, texts[1]!)
        .addTag(6, /* parentId= */ 1, 'p')
        .addText(textNodeIds[2]!, /* parentId= */ 6, texts[2]!)
        .addTag(8, /* parentId= */ 1, 'p')
        .addText(textNodeIds[3]!, /* parentId= */ 8, texts[3]!)
        .build(readingMode);

    // @ts-ignore
    app.enabledLanguagesInPref = ['en-US'];
    // @ts-ignore
    app.selectedVoice = {lang: 'en', name: 'Kristi'} as SpeechSynthesisVoice;
    app.getSpeechSynthesisVoice();
  });

  suite('after update content, read aloud is', () => {
    test('playable if done with distillation', () => {
      chrome.readingMode.requiresDistillation = false;
      app.updateContent();
      assertTrue(app.isReadAloudPlayable());
    });

    test('not playable if still requires distillation', () => {
      chrome.readingMode.requiresDistillation = true;
      app.updateContent();
      assertFalse(app.isReadAloudPlayable());
    });
  });
});
