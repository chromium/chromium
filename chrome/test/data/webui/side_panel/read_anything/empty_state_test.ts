// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, ContentController, ContentType} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement, SpEmptyStateElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('EmptyState', () => {
  let app: AppElement;
  let contentController: ContentController;

  function getEmptyState() {
    const empty =
        app.shadowRoot.querySelector<SpEmptyStateElement>('sp-empty-state');
    assertTrue(!!empty);
    return empty;
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.rootId = 1;
    contentController = new ContentController();
    ContentController.setInstance(contentController);

    app = await createApp();
  });

  test('no content shows empty state', async () => {
    const emptyPath = 'empty_state.svg';

    contentController.setState(ContentType.NO_CONTENT);
    await microtasksFinished();

    const empty = getEmptyState();
    assertStringContains(empty.darkImagePath, emptyPath);
    assertStringContains(empty.imagePath, emptyPath);
  });

  test('updateContent after loading with no content shows empty', async () => {
    const emptyPath = 'empty_state.svg';
    chrome.readingMode.getTextContent = () => '';
    app.showLoading();
    await microtasksFinished();

    app.updateContent();
    await microtasksFinished();

    const empty = getEmptyState();
    assertStringContains(empty.darkImagePath, emptyPath);
    assertStringContains(empty.imagePath, emptyPath);
  });
});
