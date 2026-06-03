// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, LanguageToastElement, SpEmptyStateElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AppStyleUpdater, BrowserProxy, ContentController, ContentType, LineFocusController, LineFocusMovement, LineFocusStyle, NodeStore, ReadAloudNode, setInstance, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent, VoiceClientSideStatusCode, VoiceLanguageController, VoiceNotificationManager} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertLT, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome-untrusted://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished, whenCheck} from 'chrome-untrusted://webui-test/test_util.js';

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
  let lineFocusController: LineFocusController;

  function getLineFocusPadding(): number {
    const val = app.style.getPropertyValue('--line-focus-padding');
    return val ? parseInt(val) : 0;
  }

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
    lineFocusController = new LineFocusController();
    LineFocusController.setInstance(lineFocusController);

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

  test(
      'connected callback adds line focus mouse listener in toolbar',
      async () => {
        chrome.readingMode.isLineFocusEnabled = true;
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.CURSOR}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        await microtasksFinished();
        let mouseMoveInToolbar = false;
        let mouseMove = false;
        LineFocusController.getInstance().onMouseMove = () => {
          mouseMove = true;
        };
        LineFocusController.getInstance().onMouseMoveInToolbar = () => {
          mouseMoveInToolbar = true;
        };

        app.connectedCallback();
        await microtasksFinished();
        app.$.toolbar.dispatchEvent(new MouseEvent('mousemove', {clientY: 10}));

        assertTrue(mouseMoveInToolbar);
        assertFalse(mouseMove);
      });

  test('connected callback adds line focus mouse listener', async () => {
    chrome.readingMode.isLineFocusEnabled = true;
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
        {detail: {data: LineFocusMovement.CURSOR}});
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.UNDERLINE}});
    await microtasksFinished();
    let mouseMoveInToolbar = false;
    let mouseMove = false;
    LineFocusController.getInstance().onMouseMove = () => {
      mouseMove = true;
    };
    LineFocusController.getInstance().onMouseMoveInToolbar = () => {
      mouseMoveInToolbar = true;
    };

    app.connectedCallback();
    await microtasksFinished();
    app.$.containerParent.dispatchEvent(
        new MouseEvent('mousemove', {clientY: 10}));

    assertTrue(mouseMove);
    assertFalse(mouseMoveInToolbar);
  });

  test('new content updates padding for line focus', async () => {
    chrome.readingMode.isLineFocusEnabled = true;
    app.connectedCallback();
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
        {detail: {data: LineFocusMovement.STATIC}});
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.UNDERLINE}});
    await microtasksFinished();
    assertEquals(0, getLineFocusPadding());

    app.updateContent();
    await whenCheck(app, () => getLineFocusPadding() !== 0);

    assertLT(0, getLineFocusPadding());
  });

  test(
      'new content does not update padding for line focus with flag disabled',
      async () => {
        chrome.readingMode.isLineFocusEnabled = false;
        app.connectedCallback();
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.CURSOR}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        await microtasksFinished();
        assertEquals(0, getLineFocusPadding());

        app.updateContent();
        await microtasksFinished();

        assertEquals(0, getLineFocusPadding());
      });

  test(
      'new content does not update padding for line focus with line focus off',
      async () => {
        chrome.readingMode.isLineFocusEnabled = true;
        app.connectedCallback();
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.STATIC}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.OFF}});
        await microtasksFinished();
        assertEquals(0, getLineFocusPadding());

        app.updateContent();
        await microtasksFinished();

        assertEquals(0, getLineFocusPadding());
      });

  test('line focus shortcut toggles line focus', async () => {
    chrome.readingMode.isLineFocusEnabled = true;
    assertFalse(lineFocusController.isEnabled());

    keyDownOn(app, 0, undefined, 'l');
    await microtasksFinished();
    assertTrue(lineFocusController.isEnabled());

    keyDownOn(app, 0, undefined, 'l');
    await microtasksFinished();
    assertFalse(lineFocusController.isEnabled());
  });

  test('line focus shortcut updates padding', async () => {
    chrome.readingMode.isLineFocusEnabled = true;
    // Ensure app is registered as a line focus listener.
    app.connectedCallback();
    await microtasksFinished();
    // Start with static line focus on.
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
        {detail: {data: LineFocusMovement.STATIC}});
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.UNDERLINE}});
    await microtasksFinished();
    assertEquals(0, getLineFocusPadding());
    // Ensure there's content so that padding can be added.
    app.updateContent();
    await whenCheck(app, () => getLineFocusPadding() !== 0);
    assertLT(0, getLineFocusPadding());

    // Toggling off should remove padding.
    keyDownOn(app, 0, undefined, 'l');
    await microtasksFinished();
    assertEquals(0, getLineFocusPadding());

    // Toggling on should remove padding.
    keyDownOn(app, 0, undefined, 'l');
    await microtasksFinished();
    assertLT(0, getLineFocusPadding());
  });

  test('line focus only shows on content', async () => {
    chrome.readingMode.isLineFocusEnabled = true;

    contentController.setState(ContentType.NO_CONTENT);
    await microtasksFinished();
    assertTrue(app.$.lineFocus.hasAttribute('hidden'));

    contentController.setState(ContentType.LOADING);
    await microtasksFinished();
    assertTrue(app.$.lineFocus.hasAttribute('hidden'));

    contentController.setState(ContentType.HAS_CONTENT);
    await microtasksFinished();
    assertFalse(app.$.lineFocus.hasAttribute('hidden'));
  });

  test(
      'onContentStateChange updates line focus style when enabled and ' +
          'has content',
      async () => {
        chrome.readingMode.isLineFocusEnabled = true;
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        await microtasksFinished();

        contentController.setState(ContentType.HAS_CONTENT);
        await microtasksFinished();

        assertEquals(
            'block', app.style.getPropertyValue('--line-focus-display'));
      });

  test(
      'onContentStateChange disables line focus style when no content',
      async () => {
        chrome.readingMode.isLineFocusEnabled = true;
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        await microtasksFinished();

        contentController.setState(ContentType.NO_CONTENT);
        await microtasksFinished();

        assertEquals(
            'none', app.style.getPropertyValue('--line-focus-display'));
      });

  test('showLoading shows spinner', async () => {
    const spinner = 'throbber';

    app.showLoading();
    await microtasksFinished();

    assertStringContains(emptyState.darkImagePath, spinner);
    assertStringContains(emptyState.imagePath, spinner);
  });

  test(
      'read aloud state resets on new content (Readability enabled)',
      async () => {
        chrome.readingMode.activeDistillationMethod =
            chrome.readingMode.distillationTypeReadability;

        let resetCallCount = 0;
        speechController.resetForNewContent = () => {
          resetCallCount++;
        };

        readingMode.htmlContent = '<div> My name is Regina George.</div>';

        app.updateContent();
        await microtasksFinished();

        assertEquals(
            1, resetCallCount,
            'resetForNewContent() should have been called once');
      });

  test('showLoading clears read aloud state', () => {
    const node = setContent('My name is Regina George', readAloudModel);
    app.$.container.appendChild(node);
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

    test('adds has-selection class when valid selection', async () => {
      // Create and append a nav element to test visibility
      const nav = document.createElement('nav');
      app.$.appFlexParent.appendChild(nav);

      readingMode.hasValidSelection = true;
      app.updateContent();
      await microtasksFinished();

      assertTrue(app.$.appFlexParent.classList.contains('has-selection'));
      // Verify that the nav element is visible (display is not 'none')
      assertTrue(window.getComputedStyle(nav).display !== 'none');
    });

    test('removes has-selection class when no valid selection', async () => {
      // Create and append a nav element to test visibility
      const nav = document.createElement('nav');
      app.$.appFlexParent.appendChild(nav);

      readingMode.hasValidSelection = false;
      app.updateContent();
      await microtasksFinished();

      assertFalse(app.$.appFlexParent.classList.contains('has-selection'));
      // Verify that the nav element is hidden by the CSS rule
      assertEquals('none', window.getComputedStyle(nav).display);
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

    test(
        'calls updateContentForScreen2x if readability enabled and has failed',
        async () => {
          chrome.readingMode.activeDistillationMethod =
              chrome.readingMode.distillationTypeScreen2x;

          let callCount = 0;
          contentController.updateContentForScreen2x =
              (_shadowRoot?: ShadowRoot) => {
                callCount++;
                return null;
              };

          app.updateContent();
          await microtasksFinished();

          assertEquals(
              1, callCount,
              'updateContentForScreen2x() should have been called');
        });

    test(
        'calls updateContentForReadability if readability enabled and success',
        async () => {
          chrome.readingMode.activeDistillationMethod =
              chrome.readingMode.distillationTypeReadability;

          let callCount = 0;
          contentController.updateContentForReadability =
              (_shadowRoot?: ShadowRoot) => {
                callCount++;
                return null;
              };

          app.updateContent();
          await microtasksFinished();

          assertEquals(
              1, callCount,
              'updateContentForReadability() should have been called');
        });

    test(
        'sends rendered blocks after layout for readability selection',
        async () => {
          chrome.readingMode.isReadabilitySelectTextEnabled = true;
          let blocksCalled = false;
          contentController.onRenderedTextBlocksAvailable = (container) => {
            assertEquals(app.$.container, container);
            blocksCalled = true;
          };

          app.updateContent();

          // Should NOT be called immediately because it's wrapped in
          // requestAnimationFrame
          assertFalse(blocksCalled, 'Should wait for requestAnimationFrame');

          // Wait for the animation frame to ensure layout is done.
          await new Promise(resolve => requestAnimationFrame(resolve));

          assertTrue(blocksCalled, 'Should be called after layout');
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

      setup(async () => {
        let calls = 0;
        readAloudModel.setInitialized(true);
        readAloudModel.setCurrentTextContent(linkText);
        readAloudModel.getCurrentTextSegments = () => {
          calls++;
          if (calls === 1) {
            return [{
              node: ReadAloudNode.create(nodeStore.getDomNode(textId)!)!,
              start: 0,
              length: 3,
            }];
          } else {
            return [];
          }
        };

        app.updateContent();
        await microtasksFinished();
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
    const textNodeContent = 'Some text';

    setup(() => {
      readingMode.rootId = 1;
      readingMode.getHtmlTag = (id) => {
        if (id === 1) {
          return 'div';
        }
        if (id === 2) {
          return 'img';
        }
        return '';
      };
      readingMode.getAltText = () => altText;
      readingMode.getChildren = (id) => {
        if (id === 1) {
          return [2, 3];
        }
        return [];
      };
      readingMode.getTextContent = (id) => id === 3 ? textNodeContent : '';
    });

    test('shows images when enabled', async () => {
      readingMode.imagesFeatureEnabled = true;
      app.updateContent();
      await microtasksFinished();
      assertTrue(contentController.hasContent());

      readingMode.imagesEnabled = true;
      const expectedHtmlWithImage =
          '<div dir="ltr" lang="en-us"><canvas dir="ltr" alt="' + altText +
          '" class="downloaded-image" lang="en-us" style=""></canvas>' +
          textNodeContent + '</div>';
      emitEvent(app, ToolbarEvent.IMAGES);
      await microtasksFinished();

      assertEquals(expectedHtmlWithImage, app.$.container.innerHTML);
    });

    test('hides images when disabled', async () => {
      readingMode.imagesFeatureEnabled = true;
      const expectedHtml =
          '<div dir="ltr" lang="en-us"><canvas dir="ltr" alt="' + altText +
          '" class="downloaded-image" lang="en-us" style="display: none;"></canvas>' +
          textNodeContent + '</div>';
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
      const expectedHtml =
          '<div dir="ltr" lang="en-us"><canvas dir="ltr" alt="' + altText +
          '" class="downloaded-image" lang="en-us" style="display: none;"></canvas>' +
          textNodeContent + '</div>';
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

        const expectedHtml =
            '<figure dir="ltr" lang="en-us" style=""><canvas dir=' +
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
        readingMode.imagesFeatureEnabled = false;

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

  suite('on image toggle with readability', () => {
    setup(() => {
      contentController.configureTrustedTypes();
      chrome.readingMode.activeDistillationMethod =
          chrome.readingMode.distillationTypeReadability;
    });

    test('shows and hides images when toggled', async () => {
      readingMode.imagesFeatureEnabled = true;

      readingMode.htmlContent = '<img src="foo.png">;';

      app.updateContent();
      await microtasksFinished();
      assertTrue(contentController.hasContent());

      const img = app.$.container.querySelector('img')!;

      readingMode.imagesEnabled = true;
      emitEvent(app, ToolbarEvent.IMAGES);
      await microtasksFinished();

      assertTrue(!!img);
      assertEquals('', img.style.display);  // Visible

      // Verify toggle off.
      readingMode.imagesEnabled = false;
      emitEvent(app, ToolbarEvent.IMAGES);
      await microtasksFinished();
      assertEquals('none', img.style.display);
    });

    test(
        'does not show images when images feature flag is disabled',
        async () => {
          readingMode.imagesFeatureEnabled = false;
          readingMode.htmlContent = '<img src="foo.png">;';
          app.updateContent();
          await microtasksFinished();

          const img = app.$.container.querySelector('img')!;

          readingMode.imagesEnabled = true;
          emitEvent(app, ToolbarEvent.IMAGES);
          await microtasksFinished();

          assertTrue(!!img);
          assertEquals('none', img.style.display);
        });

    suite('figure with caption', () => {
      const caption = 'That\'s ancient history';

      test('shows figures and captions when enabled', async () => {
        readingMode.imagesFeatureEnabled = true;

        readingMode.htmlContent = '<figure><img src="foo.png"><figcaption>' +
            caption + '</figcaption></figure>';

        app.updateContent();
        await microtasksFinished();
        assertTrue(contentController.hasContent());

        const figure = app.$.container.querySelector('figure')!;
        const figcaption = app.$.container.querySelector('figcaption')!;

        readingMode.imagesEnabled = true;
        emitEvent(app, ToolbarEvent.IMAGES);
        await microtasksFinished();

        assertEquals('', figure.style.display);  // Figure should be visible
        assertEquals(
            caption, figcaption.textContent);  // Caption text should be there

        // Verify toggle off.
        readingMode.imagesEnabled = false;
        emitEvent(app, ToolbarEvent.IMAGES);
        await microtasksFinished();
        assertEquals(
            'none', figure.style.display);  // figcaption will also be hidden if
                                            // it's parent is hidden.
      });

      test('does not show figures or captions when flag disabled', async () => {
        readingMode.imagesFeatureEnabled = false;

        readingMode.htmlContent = '<figure><img src="foo.png"><figcaption>' +
            caption + '</figcaption></figure>';

        app.updateContent();
        await microtasksFinished();
        assertTrue(contentController.hasContent());

        const figure = app.$.container.querySelector('figure')!;

        readingMode.imagesEnabled = true;
        emitEvent(app, ToolbarEvent.IMAGES);
        await microtasksFinished();
        assertEquals(
            'none', figure.style.display);  // figcaption will also be hidden if
                                            // it's parent is hidden.
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

    test('toggles links with Readability', async () => {
      const url = 'https://www.google.com/';
      const text = 'the best link ever';
      chrome.readingMode.activeDistillationMethod =
          chrome.readingMode.distillationTypeReadability;
      contentController.configureTrustedTypes();
      readingMode.htmlContent = `<a href="${url}">${text}</a>`;
      app.updateContent();
      await microtasksFinished();

      // By default, links are enabled.
      chrome.readingMode.linksEnabled = true;

      let link = app.$.container.querySelector('a');
      assertTrue(!!link, '<a> should be present before speech');

      readAloudModel.setInitialized(true);
      readAloudModel.setCurrentTextContent(text);
      readAloudModel.setCurrentTextSegments([{
        node: ReadAloudNode.create(link.firstChild!)!,
        start: 0,
        length: text.length,
      }]);

      // When speech becomes active, the link should be converted to a `<span>`.
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      link = app.$.container.querySelector('a');
      assertFalse(!!link, '<a> should be gone after speech starts');
      let span = app.$.container.querySelector<HTMLElement>('span[data-link]');
      assertTrue(!!span, '<span> should be present after speech starts');
      assertEquals(url, span.dataset['link']);

      // Stop speech, which should show the link again.
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      link = app.$.container.querySelector('a');
      assertTrue(!!link, '<a> should be back after speech stops');
      span = app.$.container.querySelector('span[data-link]');
      assertFalse(!!span, '<span> should be gone after speech stops');
      assertEquals(url, link.href);
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

  test('onNeedScrollForLineFocus scrolls', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const startingScrollTop = app.$.containerScroller.scrollTop;
    let scrollTo = 0;
    app.$.containerScroller.scrollTo = (options) => {
      scrollTo = (options as ScrollToOptions).top ?? 0;
    };

    const scrollDiff = 30;
    app.onNeedScrollForLineFocus(scrollDiff);

    assertEquals(startingScrollTop + scrollDiff, scrollTo);
  });

  suite('Immersive Mode app content styling', () => {
    let appStyleUpdater: AppStyleUpdater;

    setup(async () => {
      app.remove();
      chrome.readingMode.isImmersiveEnabled = true;
      app = await createApp();
      appStyleUpdater = new AppStyleUpdater(app);
    });

    test(
        'onContainerScroll adds fade class to scroller when IM is enabled',
        async () => {
          const fontSize = 16;
          const text = 'This is a sample text.\n'.repeat(10000);

          app.$.container.style.fontSize = `${fontSize}px`;
          appStyleUpdater.setFontSize();
          readingMode.getTextContent = () => text;
          app.updateContent();
          await microtasksFinished();

          assertFalse(app.$.containerScroller.classList.contains('fade'));

          app.$.containerScroller.scrollTop = fontSize + 1;
          app.$.containerScroller.dispatchEvent(new Event('scroll'));
          await whenCheck(
              app.$.containerScroller,
              () => app.$.containerScroller.classList.contains('fade'));
          assertTrue(app.$.containerScroller.classList.contains('fade'));

          app.$.containerScroller.scrollTop = fontSize - 1;
          app.$.containerScroller.dispatchEvent(new Event('scroll'));
          await whenCheck(
              app.$.containerScroller,
              () => !app.$.containerScroller.classList.contains('fade'));
          assertFalse(app.$.containerScroller.classList.contains('fade'));
        });

    test('applies immersive classes correctly to appFlexParent', async () => {
      const flexParent = app.shadowRoot.querySelector('#appFlexParent');
      assertTrue(!!flexParent);

      assertTrue(flexParent.classList.contains('immersive'));
      assertFalse(flexParent.classList.contains('full-page'));

      app.isImmersiveMode = () => true;
      app.requestUpdate();
      await microtasksFinished();

      assertTrue(flexParent.classList.contains('immersive'));
      assertTrue(flexParent.classList.contains('full-page'));
    });

    suite('Immersive Scrollbar Hover', () => {
      let scroller: HTMLElement;
      setup(() => {
        scroller = app.$.containerScroller;
        assertTrue(!!scroller);
        chrome.readingMode.onPresentationStateReceived(
            chrome.readingMode.inImmersiveOverlayPresentationState);
      });

      test('mousemove toggles hover class', () => {
        assertTrue(!!scroller);
        scroller.getBoundingClientRect = () => {
          return {
            left: 0,
            right: 100,
            top: 0,
            bottom: 100,
            width: 100,
            height: 100,
            x: 0,
            y: 0,
            toJSON: () => {},
          };
        };
        scroller.style.setProperty('--immersive-scrollbar-width', '14px');

        // Mouse over center (x=50), shouldn't trigger hover (needs to be >= 86)
        scroller.dispatchEvent(new MouseEvent('mousemove', {clientX: 50}));
        assertFalse(scroller.classList.contains('scrollbar-hovered'));

        // Mouse over right edge (x=90), should trigger hover
        scroller.dispatchEvent(new MouseEvent('mousemove', {clientX: 90}));
        assertTrue(scroller.classList.contains('scrollbar-hovered'));

        // Mouse moves back to center, should remove hover
        scroller.dispatchEvent(new MouseEvent('mousemove', {clientX: 80}));
        assertFalse(scroller.classList.contains('scrollbar-hovered'));
      });

      test('mouseleave removes hover class', () => {
        scroller.classList.add('scrollbar-hovered');
        scroller.dispatchEvent(new MouseEvent('mouseleave'));

        assertFalse(scroller.classList.contains('scrollbar-hovered'));
      });

      test('mousemove does nothing if not in full page immersive mode', () => {
        chrome.readingMode.onPresentationStateReceived(
            chrome.readingMode.inSidePanelPresentationState);
        scroller.getBoundingClientRect = () => {
          return {
            left: 0,
            right: 100,
            top: 0,
            bottom: 100,
            width: 100,
            height: 100,
            x: 0,
            y: 0,
            toJSON: () => {},
          };
        };
        scroller.style.setProperty('--immersive-scrollbar-width', '14px');

        // Even if we hover the right edge, the class shouldn't be added
        scroller.dispatchEvent(new MouseEvent('mousemove', {clientX: 90}));
        assertFalse(scroller.classList.contains('scrollbar-hovered'));
      });
    });
  });

  suite('footnote navigation', () => {
    test(
        'buildSubtree_ sets element.id when getHtmlId is available',
        async () => {
          const divId = 10;
          const textId = 11;
          readingMode.rootId = divId;
          readingMode.getHtmlTag = (id) => (id === divId) ? 'div' : '';
          readingMode.getChildren = (id) => (id === divId) ? [textId] : [];
          readingMode.getTextContent = (id) =>
              (id === textId) ? 'Some text content' : '';
          readingMode.htmlIds.set(divId, 'footnote-target');

          app.updateContent();
          await microtasksFinished();

          const renderedDiv = app.$.container.querySelector('#footnote-target');
          assertTrue(!!renderedDiv);
          assertEquals('footnote-target', renderedDiv.id);
        });

    test(
        'click handler intercepts same-page hash links and scrolls',
        async () => {
          const linkId = 10;
          const textId = 11;
          const targetId = 12;
          const documentUrl = 'https://www.example.com/page.html';
          const targetUrl = 'https://www.example.com/page.html#footnote-1';

          readingMode.rootId = 1;
          readingMode.getChildren = (id) => {
            if (id === 1) {
              return [linkId, targetId];
            }
            if (id === linkId) {
              return [textId];
            }
            return [];
          };
          readingMode.getHtmlTag = (id) => {
            if (id === 1) {
              return 'div';
            }
            if (id === linkId) {
              return 'a';
            }
            if (id === targetId) {
              return 'p';
            }
            return '';
          };
          readingMode.getTextContent = (id) => {
            if (id === textId) {
              return 'Footnote Link';
            }
            if (id === targetId) {
              return 'Footnote Target Content';
            }
            return '';
          };
          readingMode.getUrl = (id) => (id === linkId) ? targetUrl : '';
          readingMode.htmlIds.set(targetId, 'footnote-1');
          readingMode.documentUrl = documentUrl;

          // Spies
          let linkClickedId = -1;
          readingMode.onLinkClicked = (id) => {
            linkClickedId = id;
          };

          app.updateContent();
          await microtasksFinished();

          // Find the target element and mock its scrollIntoView
          const targetElement =
              app.$.container.querySelector<HTMLElement>('#footnote-1');
          assertTrue(!!targetElement);
          let scrollIntoViewCalled = false;
          let scrollOptions: ScrollIntoViewOptions|undefined;
          targetElement.scrollIntoView = (options) => {
            scrollIntoViewCalled = true;
            scrollOptions = options as ScrollIntoViewOptions;
          };

          // Find the link and click it
          const linkElement =
              app.$.container.querySelector<HTMLAnchorElement>('a');
          assertTrue(!!linkElement);
          linkElement.click();

          assertTrue(scrollIntoViewCalled);
          assertTrue(!!scrollOptions);
          assertEquals('smooth', scrollOptions.behavior);
          assertEquals(linkId, linkClickedId);
        });

    test('click handler falls back to default for external links', async () => {
      const linkId = 10;
      const textId = 11;
      const documentUrl = 'https://www.example.com/page.html';
      const targetUrl = 'https://www.different-domain.com/page.html#footnote-1';

      readingMode.rootId = 1;
      readingMode.getChildren = (id) => (id === 1) ? [linkId] :
          (id === linkId)                          ? [textId] :
                                                     [];
      readingMode.getHtmlTag = (id) => (id === 1) ? 'div' :
          (id === linkId)                         ? 'a' :
                                                    '';
      readingMode.getTextContent = (id) =>
          (id === textId) ? 'External Link' : '';
      readingMode.getUrl = (id) => (id === linkId) ? targetUrl : '';
      readingMode.documentUrl = documentUrl;

      let linkClickedId = -1;
      readingMode.onLinkClicked = (id) => {
        linkClickedId = id;
      };

      app.updateContent();
      await microtasksFinished();

      // Setup a mock target in DOM that would scroll if it were same-document
      const target = document.createElement('div');
      target.id = 'footnote-1';
      app.$.container.appendChild(target);
      let scrollIntoViewCalled = false;
      target.scrollIntoView = () => {
        scrollIntoViewCalled = true;
      };

      // Find the link and click it
      const linkElement = app.$.container.querySelector<HTMLAnchorElement>('a');
      assertTrue(!!linkElement);
      linkElement.click();

      assertEquals(linkId, linkClickedId);
      assertFalse(scrollIntoViewCalled);

      // Clean up
      app.$.container.removeChild(target);
    });

    test(
        'click handler triggers real navigation for mailto links', async () => {
          // <div>
          //   <a href="mailto:test@example.com">Email Link</a>
          // </div>
          const linkId = 10;
          const textId = 11;
          const documentUrl = 'https://www.example.com/page.html';
          const targetUrl = 'mailto:test@example.com';

          readingMode.rootId = 1;
          readingMode.getChildren = (id) => (id === 1) ? [linkId] :
              (id === linkId)                          ? [textId] :
                                                         [];
          readingMode.getHtmlTag = (id) => (id === 1) ? 'div' :
              (id === linkId)                         ? 'a' :
                                                        '';
          readingMode.getTextContent = (id) =>
              (id === textId) ? 'Email Link' : '';
          readingMode.getUrl = (id) => (id === linkId) ? targetUrl : '';
          readingMode.documentUrl = documentUrl;

          let linkClickedId = -1;
          readingMode.onLinkClicked = (id) => {
            linkClickedId = id;
          };

          // Mock containerScroller.scrollTo to verify we do not scroll
          let scrollToCalled = false;
          app.$.containerScroller.scrollTo = () => {
            scrollToCalled = true;
          };

          app.updateContent();
          await microtasksFinished();

          // Find the link and click it
          const linkElement =
              app.$.container.querySelector<HTMLAnchorElement>('a');
          assertTrue(!!linkElement);
          linkElement.click();

          // Confirm that onLinkClicked is called on the mailto link.
          assertEquals(linkId, linkClickedId);
          assertFalse(scrollToCalled, 'Should not scroll');
        });

    test(
        'click handler falls back to default if target is missing',
        async () => {
          const linkId = 10;
          const textId = 11;
          const documentUrl = 'https://www.example.com/page.html';
          const targetUrl =
              'https://www.example.com/page.html#footnote-missing';

          readingMode.rootId = 1;
          readingMode.getChildren = (id) => (id === 1) ? [linkId] :
              (id === linkId)                          ? [textId] :
                                                         [];
          readingMode.getHtmlTag = (id) => (id === 1) ? 'div' :
              (id === linkId)                         ? 'a' :
                                                        '';
          readingMode.getTextContent = (id) =>
              (id === textId) ? 'Missing Target Link' : '';
          readingMode.getUrl = (id) => (id === linkId) ? targetUrl : '';
          readingMode.documentUrl = documentUrl;

          let linkClickedId = -1;
          readingMode.onLinkClicked = (id) => {
            linkClickedId = id;
          };

          // Mock containerScroller.scrollTo to verify we do not scroll to top
          let scrollToCalled = false;
          app.$.containerScroller.scrollTo = () => {
            scrollToCalled = true;
          };

          app.updateContent();
          await microtasksFinished();

          // Find the link and click it
          const linkElement =
              app.$.container.querySelector<HTMLAnchorElement>('a');
          assertTrue(!!linkElement);
          linkElement.click();

          assertEquals(linkId, linkClickedId);
          assertFalse(scrollToCalled);
        });

    suite('scrollToAnchor', () => {
      let root: ShadowRoot;

      setup(() => {
        root = app.shadowRoot;
      });

      test('scrolls to target', () => {
        const targetId = 'footnote-1';
        const target = document.createElement('div');
        target.id = targetId;
        app.$.container.appendChild(target);

        let scrollIntoViewCalled = false;
        let scrollOptions: ScrollIntoViewOptions|undefined;
        target.scrollIntoView = (options) => {
          scrollIntoViewCalled = true;
          scrollOptions = options as ScrollIntoViewOptions;
        };

        chrome.readingMode.documentUrl = 'https://example.com/page.html';
        const result = contentController.scrollToAnchor(
            'https://example.com/page.html#footnote-1', root);

        assertTrue(result);
        assertTrue(scrollIntoViewCalled);
        assertEquals('smooth', scrollOptions?.behavior);
      });

      test('scrolls to top on empty hash', () => {
        let scrollToCalled = false;
        let scrollToOptions: ScrollToOptions|undefined;
        app.$.containerScroller.scrollTo = (options) => {
          scrollToCalled = true;
          scrollToOptions = options as ScrollToOptions;
        };

        chrome.readingMode.documentUrl = 'https://example.com/page.html';
        const result = contentController.scrollToAnchor(
            'https://example.com/page.html', root);

        assertTrue(result);
        assertTrue(scrollToCalled);
        assertEquals(0, scrollToOptions?.top);
        assertEquals('smooth', scrollToOptions?.behavior);
      });

      test('resolves relative links', () => {
        const targetId = 'footnote-1';
        const target = document.createElement('div');
        target.id = targetId;
        app.$.container.appendChild(target);

        let scrollIntoViewCalled = false;
        target.scrollIntoView = () => {
          scrollIntoViewCalled = true;
        };

        chrome.readingMode.documentUrl = 'https://example.com/page.html';

        // Test hash only
        let result = contentController.scrollToAnchor('#footnote-1', root);
        assertTrue(result);
        assertTrue(scrollIntoViewCalled);

        // Reset and test relative path
        scrollIntoViewCalled = false;
        result =
            contentController.scrollToAnchor('./page.html#footnote-1', root);
        assertTrue(result);
        assertTrue(scrollIntoViewCalled);
      });

      test('ignores different page URLs', () => {
        chrome.readingMode.documentUrl = 'https://example.com/page.html';
        const result = contentController.scrollToAnchor(
            'https://different.com/page.html#footnote-1', root);
        assertFalse(result);
      });

      test('ignores different pathnames', () => {
        chrome.readingMode.documentUrl = 'https://example.com/page.html';
        const result = contentController.scrollToAnchor(
            'https://example.com/other.html#footnote-1', root);
        assertFalse(result);
      });

      test('ignores different search parameters', () => {
        chrome.readingMode.documentUrl =
            'https://example.com/page.html?query=1';
        const result = contentController.scrollToAnchor(
            'https://example.com/page.html?query=2#footnote-1', root);
        assertFalse(result);
      });

      test('scrolls with identical search parameters', () => {
        const targetId = 'footnote-1';
        const target = document.createElement('div');
        target.id = targetId;
        app.$.container.appendChild(target);

        let scrollIntoViewCalled = false;
        target.scrollIntoView = () => {
          scrollIntoViewCalled = true;
        };

        chrome.readingMode.documentUrl =
            'https://example.com/page.html?query=1';
        const result = contentController.scrollToAnchor(
            'https://example.com/page.html?query=1#footnote-1', root);

        assertTrue(result);
        assertTrue(scrollIntoViewCalled);

        // Clean up
        app.$.container.removeChild(target);
      });

      test('handles invalid URLs gracefully', () => {
        chrome.readingMode.documentUrl = 'https://example.com/page.html';
        const result = contentController.scrollToAnchor('invalid://url', root);
        assertFalse(result);
      });

      test('handles malformed URI percent-encoding gracefully', () => {
        chrome.readingMode.documentUrl = 'https://example.com/page.html';

        // Test hash only with malformed percent-encoding
        let result = contentController.scrollToAnchor('#foo%2', root);
        assertFalse(result);

        // Test relative path with malformed percent-encoding
        result = contentController.scrollToAnchor('./page.html#foo%2', root);
        assertFalse(result);
      });

      test('falls back to top on #top hash if element is missing', () => {
        let scrollToCalled = false;
        let scrollToOptions: ScrollToOptions|undefined;
        app.$.containerScroller.scrollTo = (options) => {
          scrollToCalled = true;
          scrollToOptions = options as ScrollToOptions;
        };

        chrome.readingMode.documentUrl = 'https://example.com/page.html';

        // Test hash only
        let result = contentController.scrollToAnchor('#top', root);
        assertTrue(result);
        assertTrue(scrollToCalled);
        assertEquals(0, scrollToOptions?.top);
        assertEquals('smooth', scrollToOptions?.behavior);

        // Reset and test absolute URL
        scrollToCalled = false;
        result = contentController.scrollToAnchor(
            'https://example.com/page.html#top', root);
        assertTrue(result);
        assertTrue(scrollToCalled);
        assertEquals(0, scrollToOptions?.top);
        assertEquals('smooth', scrollToOptions?.behavior);
      });

      test('scrolls to element on #top hash if element is present', () => {
        const targetId = 'top';
        const target = document.createElement('div');
        target.id = targetId;
        app.$.container.appendChild(target);

        let scrollIntoViewCalled = false;
        target.scrollIntoView = () => {
          scrollIntoViewCalled = true;
        };

        chrome.readingMode.documentUrl = 'https://example.com/page.html';
        const result = contentController.scrollToAnchor('#top', root);
        assertTrue(result);
        assertTrue(scrollIntoViewCalled);

        // Clean up
        app.$.container.removeChild(target);
      });
    });
  });
});
