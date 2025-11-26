// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, LanguageToastElement, SpEmptyStateElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {BrowserProxy, ContentController, ContentType, NodeStore, ReadAloudNode, setInstance, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent, VoiceClientSideStatusCode, VoiceLanguageController, VoiceNotificationManager} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, emitEvent, setContent, setupBasicSpeech} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('AppContent', () => {
  let app: AppElement;
  let readingMode: FakeReadingMode;
  let contentController: ContentController;
  let emptyState: SpEmptyStateElement;
  let speechController: SpeechController;
  let voiceLanguageController: VoiceLanguageController;
  let nodeStore: NodeStore;
  let notificationManager: VoiceNotificationManager;
  let readAloudModel: TestReadAloudModelBrowserProxy;
  let speech: TestSpeechBrowserProxy;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    readAloudModel = new TestReadAloudModelBrowserProxy();
    setInstance(readAloudModel);
    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    notificationManager = new VoiceNotificationManager();
    VoiceNotificationManager.setInstance(notificationManager);
    voiceLanguageController = new VoiceLanguageController();
    VoiceLanguageController.setInstance(voiceLanguageController);
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
    setContent('My name is Regina George', readAloudModel);
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

  test('no content shows empty state', async () => {
    const emptyPath = 'empty_state.svg';

    contentController.setState(ContentType.NO_CONTENT);
    await microtasksFinished();

    assertStringContains(emptyState.darkImagePath, emptyPath);
    assertStringContains(emptyState.imagePath, emptyPath);
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

    test('sends distilled word count', async () => {
      const text = 'Honey we can see right through ya';
      readingMode.getTextContent = () => text;
      const expectedWordCount = 7;
      let sentWordCount = 0;
      readingMode.onDistilled = (wordCount) => {
        sentWordCount = wordCount;
      };

      app.updateContent();
      await microtasksFinished();

      assertEquals(expectedWordCount, sentWordCount);
    });

    test('sends 0 if no new content', async () => {
      readingMode.getTextContent = () => '';
      let sentWordCount = -1;
      readingMode.onDistilled = (wordCount) => {
        sentWordCount = wordCount;
      };

      app.updateContent();
      await microtasksFinished();

      assertEquals(0, sentWordCount);
    });
  });

  suite('on links toggle', () => {
    const linkId = 44;
    const textId = 45;
    const linkText = 'Try to keep it hidden';
    const url = 'www.mountainview.gov';

    setup(() => {
      readingMode.rootId = linkId;
      readingMode.getHtmlTag = (id) => (id === linkId) ? 'a' : '';
      readingMode.getTextContent = (id) => (id === linkId) ? '' : linkText;
      readingMode.getChildren = (id) => (id === linkId) ? [textId] : [];
      readingMode.getUrl = () => url;
    });

    test('shows links when enabled', async () => {
      const expectedHtml =
          '<a dir="ltr" href="' + url + '" lang="en-us">' + linkText + '</a>';
      app.updateContent();
      await microtasksFinished();
      assertTrue(contentController.hasContent());

      readingMode.linksEnabled = true;
      emitEvent(app, ToolbarEvent.LINKS);
      await microtasksFinished();

      assertEquals(
          expectedHtml, app.$.container.innerHTML, app.$.container.innerHTML);
    });

    test('hides links when disabled', async () => {
      const expectedHtml = '<span dir="ltr" lang="en-us" data-link="' + url +
          '">' + linkText + '</span>';
      app.updateContent();
      await microtasksFinished();
      assertTrue(contentController.hasContent());

      readingMode.linksEnabled = false;
      emitEvent(app, ToolbarEvent.LINKS);
      await microtasksFinished();

      assertEquals(
          expectedHtml, app.$.container.innerHTML, app.$.container.innerHTML);
    });

    suite('with speech', () => {
      const noLinksHtml = '<span dir="ltr" lang="en-us" data-link="' + url +
          '"><span class="parent-of-highlight"><span class="' +
          'current-read-highlight">Try</span> to keep it hidden</span></span>';
      const linksHtml = '<a dir="ltr" lang="en-us" href="' + url +
          '"><span class="parent-of-highlight"><span class="' +
          'current-read-highlight">Try</span> to keep it hidden</span></a>';

      setup(() => {
        const parent = document.createElement('a');
        const text = document.createTextNode(linkText);
        parent.href = url;
        parent.appendChild(text);
        nodeStore.setDomNode(parent, linkId);
        nodeStore.setDomNode(text, textId);
        const segments =
            [{node: ReadAloudNode.create(text)!, start: 0, length: 3}];
        let calls = 0;
        readAloudModel.setInitialized(true);
        readAloudModel.setCurrentTextContent(linkText);
        readAloudModel.getCurrentTextSegments = () => {
          calls++;
          if (calls === 1) {
            return segments;
          } else {
            return [];
          }
        };


        return app.updateContent();
      });

      test('hides links when speech active', async () => {
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        await microtasksFinished();
        assertEquals(noLinksHtml, app.$.container.innerHTML);
      });

      test('shows links when speech paused', async () => {
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        await microtasksFinished();

        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        await microtasksFinished();

        assertEquals(linksHtml, app.$.container.innerHTML);
      });

      test('shows links when speech finished', async () => {
        const expectedHTML = '<a dir="ltr" lang="en-us" href="' + url +
            '"><span class="parent-of-highlight"><span class="">' +
            'Try</span> to keep it hidden</span></a>';
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        await microtasksFinished();

        const spoken = await speech.whenCalled('speak');
        spoken.onend();

        assertEquals(expectedHTML, app.$.container.innerHTML);
      });

      test('hides links when speech active and links disabled', async () => {
        readingMode.linksEnabled = false;
        emitEvent(app, ToolbarEvent.LINKS);
        await microtasksFinished();

        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        await microtasksFinished();
        assertEquals(noLinksHtml, app.$.container.innerHTML);
      });

      test('hides links when speech paused and links disabled', async () => {
        readingMode.linksEnabled = false;
        emitEvent(app, ToolbarEvent.LINKS);
        await microtasksFinished();
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        await microtasksFinished();

        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        await microtasksFinished();

        assertEquals(noLinksHtml, app.$.container.innerHTML);
      });
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

  suite('on speech active change', () => {
    test('selection allowed by default', () => {
      assertEquals(
          'user-select-disabled-when-speech-active-false',
          app.$.container.className);
      assertEquals('auto', window.getComputedStyle(app.$.container).userSelect);
    });

    test('selection disallowed when speech active', async () => {
      setContent('Been there, done that', readAloudModel);

      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      assertEquals(
          'user-select-disabled-when-speech-active-true',
          app.$.container.className);
      assertEquals('none', window.getComputedStyle(app.$.container).userSelect);
    });

    test('selection allowed after speech stops', async () => {
      setContent('Who do you think you\'re kidding?', readAloudModel);

      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      assertEquals(
          'user-select-disabled-when-speech-active-false',
          app.$.container.className);
      assertEquals('auto', window.getComputedStyle(app.$.container).userSelect);
    });
  });

  test('playing from selection clears selection', () => {
    const p = document.createElement('p');
    p.innerText = 'He\'s the earth and heaven to ya';
    document.body.appendChild(p);
    const selection = app.getSelection();
    assertTrue(!!selection);
    const range = new Range();
    range.setStartBefore(p);
    range.setEndAfter(p);
    selection.addRange(range);
    assertEquals(p.innerText, selection.toString());

    app.onPlayingFromSelection();

    assertEquals('', selection.toString());
  });

  test('on selection resets plays from new selection', async () => {
    const text1 = 'Not like you- ';
    const text2 = ' you lost your nerve, you lost the game.';

    const p1 = document.createElement('p');
    p1.innerText = text1;
    app.$.container.appendChild(p1);
    const node1 = p1.firstChild!;
    const id1 = 2;
    nodeStore.setDomNode(node1, id1);

    const p2 = document.createElement('p');
    p2.innerText = text2;
    app.$.container.appendChild(p2);
    const node2 = p2.firstChild!;
    const id2 = 3;
    nodeStore.setDomNode(node2, id2);

    const segments =
        [{node: ReadAloudNode.create(node1)!, start: 0, length: text1.length}];
    readAloudModel.setCurrentTextSegments(segments);
    readAloudModel.setCurrentTextContent(text1);
    readAloudModel.init(ReadAloudNode.create(document.body)!);

    // Start speech to initialize read aloud state.
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    assertTrue(speechController.isSpeechTreeInitialized());
    await microtasksFinished();

    // Create a selection.
    const selection = app.getSelection();
    assertTrue(!!selection);
    const range = document.createRange();
    range.setStart(node1, 1);
    range.setEnd(node2, 3);
    selection.removeAllRanges();
    selection.addRange(range);
    document.dispatchEvent(new Event('selectionchange'));
    await microtasksFinished();

    // After a selection, the read aloud state should still be set to true.
    // This differs from the V8 selection approach.
    assertTrue(speechController.isSpeechTreeInitialized());
  });

  suite('language toast', () => {
    const lang = 'ko-km';
    let toast: LanguageToastElement;

    setup(() => {
      toast = app.$.languageToast;
    });

    test('shows error toasts', async () => {
      notificationManager.onNoEngineConnection();
      await microtasksFinished();
      assertTrue(toast.$.toast.open);
    });

    test('does not shows error toast with language menu open', async () => {
      emitEvent(app, ToolbarEvent.LANGUAGE_MENU_OPEN);

      notificationManager.onNoEngineConnection();
      await microtasksFinished();

      assertFalse(toast.$.toast.open);
    });

    test('shows error toast after language menu is closed', async () => {
      emitEvent(app, ToolbarEvent.LANGUAGE_MENU_OPEN);
      emitEvent(app, ToolbarEvent.LANGUAGE_MENU_CLOSE);

      notificationManager.onNoEngineConnection();
      await microtasksFinished();

      assertTrue(toast.$.toast.open);
    });

    // <if expr="is_chromeos">
    test('shows downloaded toast on ChromeOS', async () => {
      notificationManager.onVoiceStatusChange(
          lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);
      await microtasksFinished();
      notificationManager.onVoiceStatusChange(
          lang, VoiceClientSideStatusCode.AVAILABLE, []);
      await microtasksFinished();

      assertTrue(toast.$.toast.open);
    });
    // </if>

    // <if expr="not is_chromeos">
    test('no downloaded toast outside ChromeOS', async () => {
      notificationManager.onVoiceStatusChange(
          lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);
      await microtasksFinished();
      notificationManager.onVoiceStatusChange(
          lang, VoiceClientSideStatusCode.AVAILABLE, []);
      await microtasksFinished();

      assertFalse(toast.$.toast.open);
    });
    // </if>
  });
});
