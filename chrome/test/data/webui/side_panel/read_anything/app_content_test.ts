// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, SpEmptyStateElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {BrowserProxy, ContentController, NodeStore, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent, VoiceLanguageController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, emitEvent, setSimpleTreeWithText, setupBasicSpeech} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('AppContent', () => {
  let app: AppElement;
  let readingMode: FakeReadingMode;
  let contentController: ContentController;
  let emptyState: SpEmptyStateElement;
  let speechController: SpeechController;
  let nodeStore: NodeStore;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    const speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    VoiceLanguageController.setInstance(new VoiceLanguageController());
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    contentController = new ContentController();
    ContentController.setInstance(contentController);

    app = await createApp();
    emptyState =
        app.shadowRoot.querySelector<SpEmptyStateElement>('sp-empty-state')!;
    setupBasicSpeech(speech);
  });

  test('connected callback shows spinner', async () => {
    const spinner = 'throbber';

    app.connectedCallback();
    await microtasksFinished();

    assertStringContains(emptyState.darkImagePath, spinner);
    assertStringContains(emptyState.imagePath, spinner);
  });

  test('showLoading shows spinner', async () => {
    const spinner = 'throbber';

    app.showLoading();
    await microtasksFinished();

    assertStringContains(emptyState.darkImagePath, spinner);
    assertStringContains(emptyState.imagePath, spinner);
  });

  test('showLoading clears read aloud state', () => {
    setSimpleTreeWithText('My name is Regina George');
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    assertTrue(speechController.isSpeechActive());

    app.showLoading();

    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
  });

  test('selection on loading screen does nothing', async () => {
    const range = new Range();
    range.setStartBefore(emptyState);
    range.setEndAfter(emptyState);
    const selection = document.getSelection();
    assertTrue(!!selection);
    selection.removeAllRanges();

    app.showLoading();
    await microtasksFinished();
    selection.addRange(range);
    await microtasksFinished();

    assertEquals('', document.getSelection()?.toString());
  });

  suite('updateContent', () => {
    test('playable if done with distillation', async () => {
      readingMode.requiresDistillation = false;
      app.updateContent();
      await microtasksFinished();

      assertTrue(app.$.toolbar.isReadAloudPlayable);
    });

    test('not playable if still requires distillation', async () => {
      readingMode.requiresDistillation = true;
      app.updateContent();
      await microtasksFinished();

      assertFalse(app.$.toolbar.isReadAloudPlayable);
    });

    test('clears content on receiving new content', async () => {
      const text = 'If there\'s a prize for rotten judgment';
      readingMode.getTextContent = () => text;
      readingMode.rootId = 0;

      app.updateContent();
      await microtasksFinished();

      assertEquals('', app.$.container.innerHTML);
    });

    test('shows new content', async () => {
      const text = 'I guess I\'ve already won that';
      readingMode.getTextContent = () => text;

      app.updateContent();
      await microtasksFinished();

      assertTrue(contentController.hasContent());
      assertEquals(text, app.$.container.textContent);
    });

    test('sets empty if no new content', async () => {
      const empty = 'empty';
      readingMode.getTextContent = () => '';

      app.updateContent();
      await microtasksFinished();

      assertTrue(contentController.isEmpty());
      assertStringContains(emptyState.darkImagePath, empty);
      assertStringContains(emptyState.imagePath, empty);
    });
  });

  suite('on image toggle', () => {
    const altText = 'No man is worth the aggravation';

    setup(() => {
      readingMode.getHtmlTag = () => 'img';
      readingMode.getAltText = () => altText;
      readingMode.getChildren = () => [];
    });

    test('shows images when enabled', async () => {
      readingMode.imagesFeatureEnabled = true;
      const expectedHtml = '<canvas dir="ltr" alt="' + altText +
          '" class="downloaded-image" lang="en-us" style=""></canvas>';
      app.updateContent();
      await microtasksFinished();
      assertTrue(contentController.hasContent());

      readingMode.imagesEnabled = true;
      emitEvent(app, ToolbarEvent.IMAGES);
      await microtasksFinished();

      assertEquals(expectedHtml, app.$.container.innerHTML);
    });

    test('hides images when disabled', async () => {
      readingMode.imagesFeatureEnabled = true;
      const expectedHtml = '<canvas dir="ltr" alt="' + altText +
          '" class="downloaded-image" lang="en-us" style="display: none;">' +
          '</canvas>';
      app.updateContent();
      await microtasksFinished();
      assertTrue(contentController.hasContent());

      readingMode.imagesEnabled = false;
      emitEvent(app, ToolbarEvent.IMAGES);
      await microtasksFinished();

      assertEquals(expectedHtml, app.$.container.innerHTML);
    });

    test('does not show images when feature flag disabled', async () => {
      readingMode.imagesFeatureEnabled = false;
      const expectedHtml = '<canvas dir="ltr" alt="' + altText +
          '" class="downloaded-image" lang="en-us" style="display: none;">' +
          '</canvas>';
      app.updateContent();
      await microtasksFinished();

      readingMode.imagesEnabled = true;
      emitEvent(app, ToolbarEvent.IMAGES);
      await microtasksFinished();

      assertEquals(expectedHtml, app.$.container.innerHTML);
    });

    suite('figure with caption', () => {
      const figureId = 2;
      const imageId = 3;
      const captionId = 4;
      const textId = 5;
      const caption = 'That\'s ancient history';

      setup(() => {
        readingMode.rootId = figureId;
        readingMode.getAltText = () => '';
        readingMode.getHtmlTag = (id) => {
          if (id === figureId) {
            return 'figure';
          } else if (id === imageId) {
            return 'img';
          } else if (id === captionId) {
            return 'figcaption';
          } else {
            return '';
          }
        };
        readingMode.getChildren = (id) => {
          if (id === figureId) {
            return [imageId, captionId];
          } else if (id === captionId) {
            return [textId];
          } else {
            return [];
          }
        };
        readingMode.getTextContent = () => caption;
      });

      test('shows figures and captions when enabled', async () => {
        readingMode.imagesFeatureEnabled = true;

        const expectedHtml = '<figure dir="ltr" lang="en-us"><canvas dir=' +
            '"ltr" alt="" class="downloaded-image" lang="en-us" style="">' +
            '</canvas><figcaption dir="ltr" lang="en-us">' + caption +
            '</figcaption></figure>';
        app.updateContent();
        await microtasksFinished();
        assertTrue(contentController.hasContent());

        readingMode.imagesEnabled = true;
        emitEvent(app, ToolbarEvent.IMAGES);
        await microtasksFinished();

        assertEquals(expectedHtml, app.$.container.innerHTML);
      });

      test('hides figures and captions when disabled', async () => {
        readingMode.imagesFeatureEnabled = true;

        const expectedHtml = '<figure dir="ltr" lang="en-us" style="display:' +
            ' none;"><canvas dir="ltr" alt="" class="downloaded-image" lang=' +
            '"en-us" style="display: none;"></canvas><figcaption dir="ltr"' +
            ' lang="en-us">' + caption + '</figcaption></figure>';
        app.updateContent();
        await microtasksFinished();
        assertTrue(contentController.hasContent());

        readingMode.imagesEnabled = false;
        emitEvent(app, ToolbarEvent.IMAGES);
        await microtasksFinished();

        assertEquals(expectedHtml, app.$.container.innerHTML);
      });

      test('does not show figures or captions when flag disabled', async () => {
        readingMode.imagesFeatureEnabled = true;

        const expectedHtml = '<figure dir="ltr" lang="en-us" style="display:' +
            ' none;"><canvas dir="ltr" alt="" class="downloaded-image" lang=' +
            '"en-us" style="display: none;"></canvas><figcaption dir="ltr"' +
            ' lang="en-us">' + caption + '</figcaption></figure>';
        app.updateContent();
        await microtasksFinished();
        assertTrue(contentController.hasContent());

        readingMode.imagesEnabled = false;
        emitEvent(app, ToolbarEvent.IMAGES);
        await microtasksFinished();

        assertEquals(expectedHtml, app.$.container.innerHTML);
      });
    });
  });
});
