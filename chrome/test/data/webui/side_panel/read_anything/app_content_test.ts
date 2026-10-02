// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, ContentController, NodeStore, SpeechController, SpEmptyStateElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AppStyleUpdater, ContentType, ReadAloudNode, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished, whenCheck} from 'chrome-untrusted://webui-test/test_util.js';

import {emitEvent, setContent, setupAppTestEnvironment, setupBasicSpeech} from './common.js';
import type {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import type {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('AppContent', () => {
  let app: AppElement;
  let contentBrowserProxy: TestContentBrowserProxy;
  let contentController: ContentController;
  let emptyState: SpEmptyStateElement;
  let speechController: SpeechController;
  let nodeStore: NodeStore;
  let readAloudModel: TestReadAloudModelBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;

  setup(async () => {
    const result = await setupAppTestEnvironment();
    app = result.app;
    contentBrowserProxy = result.contentBrowserProxy;
    contentController = result.contentController;
    speechController = result.speechController;
    nodeStore = result.nodeStore;
    readAloudModel = result.readAloudModel;
    visualBrowserProxy = result.visualBrowserProxy;
    emptyState =
        app.shadowRoot.querySelector<SpEmptyStateElement>('sp-empty-state')!;
    setupBasicSpeech(result.speech);
  });

  test('connected callback shows spinner', () => {
    const spinner = 'throbber';

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

  test('showLoading event triggers showLoading', async () => {
    const spinner = 'throbber';

    contentBrowserProxy.showLoading.callListeners();
    await microtasksFinished();

    assertStringContains(emptyState.darkImagePath, spinner);
    assertStringContains(emptyState.imagePath, spinner);
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
      contentBrowserProxy.requiresDistillationVal = false;
      app.updateContent();
      await microtasksFinished();

      assertTrue(app.$.toolbar.isReadAloudPlayable);
    });

    test('not playable if still requires distillation', async () => {
      contentBrowserProxy.requiresDistillationVal = true;
      app.updateContent();
      await microtasksFinished();

      assertFalse(app.$.toolbar.isReadAloudPlayable);
    });

    test('clears content on receiving new content', async () => {
      const text = 'If there\'s a prize for rotten judgment';
      contentBrowserProxy.textContentMap = {2: text};
      contentBrowserProxy.rootId = 0;

      app.updateContent();
      await microtasksFinished();

      assertEquals('', app.$.container.innerHTML);
    });

    test('shows new content', async () => {
      const text = 'I guess I\'ve already won that';
      contentBrowserProxy.textContentMap = {2: text};

      app.updateContent();
      await microtasksFinished();

      assertTrue(contentController.hasContent());
      assertEquals(text, app.$.container.textContent);
    });

    test('updateContent event calls updateContent', async () => {
      const text = 'I guess I\'ve already won that';
      contentBrowserProxy.textContentMap = {2: text};

      contentBrowserProxy.updateContent.callListeners();
      await microtasksFinished();

      assertTrue(contentController.hasContent());
      assertEquals(text, app.$.container.textContent);
    });

    test('adds has-selection class when valid selection', async () => {
      // Create and append a nav element to test visibility
      const nav = document.createElement('nav');
      app.$.appFlexParent.appendChild(nav);

      contentBrowserProxy.hasValidSelectionVal = true;
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

      contentBrowserProxy.hasValidSelectionVal = false;
      app.updateContent();
      await microtasksFinished();

      assertFalse(app.$.appFlexParent.classList.contains('has-selection'));
      // Verify that the nav element is hidden by the CSS rule
      assertEquals('none', window.getComputedStyle(nav).display);
    });

    test('sets empty if no new content', async () => {
      const empty = 'empty';
      contentBrowserProxy.textContentMap = {2: ''};

      app.updateContent();
      await microtasksFinished();

      assertTrue(contentController.isEmpty());
      assertStringContains(emptyState.darkImagePath, empty);
      assertStringContains(emptyState.imagePath, empty);
    });

    test('sends distilled word count', async () => {
      const text = 'Honey we can see right through ya';
      contentBrowserProxy.textContentMap = {2: text};
      const expectedWordCount = 7;

      app.updateContent();

      const sentWordCount = await contentBrowserProxy.whenCalled('onDistilled');
      assertEquals(expectedWordCount, sentWordCount);
    });

    test('sends 0 if no new content', async () => {
      contentBrowserProxy.textContentMap = {2: ''};

      app.updateContent();

      const sentWordCount = await contentBrowserProxy.whenCalled('onDistilled');
      assertEquals(0, sentWordCount);
    });

    test(
        'sends rendered blocks after layout for readability selection',
        async () => {
          contentBrowserProxy.isReadabilitySelectTextEnabledFlag = true;
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

    test(
        'rapid updateContent calls only send rendered blocks once',
        async () => {
          contentBrowserProxy.isReadabilitySelectTextEnabledFlag = true;
          let callCount = 0;
          contentController.onRenderedTextBlocksAvailable = () => {
            callCount++;
          };

          app.updateContent();
          app.updateContent();
          app.updateContent();

          assertFalse(callCount > 0, 'Should wait for requestAnimationFrame');

          // Wait for the animation frame.
          await new Promise(resolve => requestAnimationFrame(resolve));

          assertEquals(1, callCount, 'Should only be called once');
        });

    test('cancels scheduled requestAnimationFrame on disconnect', async () => {
      contentBrowserProxy.isReadabilitySelectTextEnabledFlag = true;
      let callCount = 0;
      contentController.onRenderedTextBlocksAvailable = () => {
        callCount++;
      };

      app.updateContent();
      assertFalse(callCount > 0, 'Should wait for requestAnimationFrame');

      app.disconnectedCallback();

      await new Promise(resolve => requestAnimationFrame(resolve));

      assertEquals(0, callCount, 'Should not be called after disconnect');
    });

    test('cancels scheduled requestAnimationFrame on showLoading', async () => {
      contentBrowserProxy.isReadabilitySelectTextEnabledFlag = true;
      let callCount = 0;
      contentController.onRenderedTextBlocksAvailable = () => {
        callCount++;
      };

      app.updateContent();
      assertFalse(callCount > 0, 'Should wait for requestAnimationFrame');

      app.showLoading();

      await new Promise(resolve => requestAnimationFrame(resolve));

      assertEquals(0, callCount, 'Should not be called after showLoading');
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
      contentBrowserProxy.activeDistillationMethod =
          contentBrowserProxy.distillationTypeReadability;
      contentBrowserProxy.htmlContent = `<a href="${url}">${text}</a>`;
      app.updateContent();
      await microtasksFinished();

      // By default, links are enabled.
      visualBrowserProxy.linksEnabled = true;

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

  suite('Immersive Mode app content styling', () => {
    let appStyleUpdater: AppStyleUpdater;

    setup(() => {
      appStyleUpdater = new AppStyleUpdater(app);
    });

    test(
        'onContainerScroll adds fade class to scroller when IM is enabled',
        async () => {
          const fontSize = 16;
          const text = 'This is a sample text.';

          app.$.container.style.fontSize = `${fontSize}px`;
          app.$.container.style.height = '2000px';
          appStyleUpdater.setFontSize();
          contentBrowserProxy.textContentMap = {2: text};
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
        visualBrowserProxy.onPresentationStateReceived.callListeners(
            visualBrowserProxy.inImmersiveOverlayPresentationState);
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
        visualBrowserProxy.onPresentationStateReceived.callListeners(
            visualBrowserProxy.inSidePanelPresentationState);
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
    test('onMainFrameSameDocumentNavigation scrolls to target', async () => {
      const targetId = 12;
      const textId = 13;
      const documentUrl = 'https://www.example.com/page.html';
      const targetUrl = 'https://www.example.com/page.html#footnote-1';

      contentBrowserProxy.rootId = 1;
      contentBrowserProxy.childrenMap = {1: [targetId], [targetId]: [textId]};
      contentBrowserProxy.htmlTagMap = {1: 'div', [targetId]: 'p'};
      contentBrowserProxy
          .textContentMap = {[textId]: 'Footnote Target Content'};
      contentBrowserProxy.htmlIdMap = {[targetId]: 'footnote-1'};
      contentBrowserProxy.documentUrl = documentUrl;

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

      // Trigger same document navigation
      contentBrowserProxy.onMainFrameSameDocumentNavigation.callListeners(
          targetUrl);

      assertTrue(scrollIntoViewCalled);
      assertTrue(!!scrollOptions);
      assertEquals('smooth', scrollOptions.behavior);
    });
  });
});
