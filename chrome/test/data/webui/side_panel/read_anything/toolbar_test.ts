// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {IMAGES_DISABLED_ICON, IMAGES_ENABLED_ICON, IMAGES_TOGGLE_BUTTON_ID, LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {mockMetrics, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('Toolbar', () => {
  let toolbar: ReadAnythingToolbarElement;
  let shadowRoot: ShadowRoot;
  let metrics: TestMetricsBrowserProxy;

  async function createToolbar(): Promise<void> {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();
    assertTrue(!!toolbar.shadowRoot);
    shadowRoot = toolbar.shadowRoot;
  }

  function getButton(id: string): CrIconButtonElement|null {
    return shadowRoot.querySelector<CrIconButtonElement>('#' + id);
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();
  });

  suite('with read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = true;
      return createToolbar();
    });

    test('has text settings menus', () => {
      stubAnimationFrame();

      const colorButton = getButton('color');
      assertTrue(!!colorButton);
      colorButton.click();
      assertTrue(toolbar.$.colorMenu.$.menu.$.lazyMenu.get().open);

      const lineSpacingButton = getButton('line-spacing');
      assertTrue(!!lineSpacingButton);
      lineSpacingButton.click();
      assertTrue(toolbar.$.lineSpacingMenu.$.menu.$.lazyMenu.get().open);

      const letterSpacingButton = getButton('letter-spacing');
      assertTrue(!!letterSpacingButton);
      letterSpacingButton.click();
      assertTrue(toolbar.$.letterSpacingMenu.$.menu.$.lazyMenu.get().open);
    });

    test('has highlight menu', () => {
      stubAnimationFrame();
      const highlightButton = getButton('highlight');

      assertTrue(!!highlightButton);
      highlightButton.click();

      assertTrue(toolbar.$.highlightMenu.$.menu.$.lazyMenu.get().open);
    });

    test('has voice menu', () => {
      stubAnimationFrame();
      const voiceButton = getButton('voice-selection');

      assertTrue(!!voiceButton);
      voiceButton.click();

      assertTrue(toolbar.$.voiceSelectionMenu.$.voiceSelectionMenu.get().open);
    });

    test('has audio controls', () => {
      const audioControls = shadowRoot.querySelector('#audio-controls');
      assertTrue(!!audioControls);
    });

    test('font is dropdown menu from button', () => {
      stubAnimationFrame();
      const fontButton = getButton('font');

      assertTrue(!!fontButton);
      fontButton.click();

      assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    });
  });

  suite('without read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = false;
      return createToolbar();
    });

    test('has text settings menus', () => {
      stubAnimationFrame();

      const colorButton = getButton('color');
      assertTrue(!!colorButton);
      colorButton.click();
      assertTrue(toolbar.$.colorMenu.$.menu.$.lazyMenu.get().open);

      const lineSpacingButton = getButton('line-spacing');
      assertTrue(!!lineSpacingButton);
      lineSpacingButton.click();
      assertTrue(toolbar.$.lineSpacingMenu.$.menu.$.lazyMenu.get().open);

      const letterSpacingButton = getButton('letter-spacing');
      assertTrue(!!letterSpacingButton);
      letterSpacingButton.click();
      assertTrue(toolbar.$.letterSpacingMenu.$.menu.$.lazyMenu.get().open);
    });

    test('does not have voice menu', () => {
      stubAnimationFrame();
      const voiceButton = getButton('voice-selection');
      assertFalse(!!voiceButton);
    });

    test('does not have highlight menu', () => {
      stubAnimationFrame();
      const highlightButton = getButton('highlight');
      assertFalse(!!highlightButton);
    });

    test('does not have audio controls', () => {
      const audioControls = shadowRoot.querySelector('#audio-controls');
      assertFalse(!!audioControls);
    });

    test('font is select element', () => {
      const fontSelect = shadowRoot.querySelector('#font-select');
      assertTrue(!!fontSelect);
    });
  });

  suite('rate button', () => {
    let rateButton: CrButtonElement;

    async function changeRate(rate: number) {
      toolbar.$.rateMenu.dispatchEvent(
          new CustomEvent(ToolbarEvent.RATE, {detail: {data: rate}}));
      return microtasksFinished();
    }

    setup(async () => {
      chrome.readingMode.isReadAloudEnabled = true;
      await createToolbar();

      const rate = shadowRoot.querySelector<CrButtonElement>('#rate');
      assertTrue(!!rate);
      rateButton = rate;
    });

    test('does not exist with read aloud disabled', async () => {
      chrome.readingMode.isReadAloudEnabled = false;
      await createToolbar();
      assertFalse(!!shadowRoot.querySelector<CrButtonElement>('#rate'));
    });

    test('shows rate menu on click', () => {
      stubAnimationFrame();
      rateButton.click();
      assertTrue(toolbar.$.rateMenu.$.menu.$.lazyMenu.get().open);
    });

    test('defaults to 1x', () => {
      assertStringContains(rateButton.ariaLabel!, '1x');
      assertStringContains(rateButton.textContent, '1x');
    });

    test('rate change updates rate button', async () => {
      await changeRate(2);
      assertStringContains(rateButton.ariaLabel!, '2x');
      assertStringContains(rateButton.textContent, '2x');

      await changeRate(0.5);
      assertStringContains(rateButton.ariaLabel!, '0.5');
      assertStringContains(rateButton.textContent, '0.5');
    });
  });

  suite('highlight button', () => {
    let highlightButton: CrIconButtonElement;

    async function changeHighlight(granularity: number) {
      toolbar.$.highlightMenu.dispatchEvent(new CustomEvent(
          ToolbarEvent.HIGHLIGHT_CHANGE, {detail: {data: granularity}}));
      return microtasksFinished();
    }

    setup(async () => {
      chrome.readingMode.isReadAloudEnabled = true;
      await createToolbar();

      const highlight = getButton('highlight');
      assertTrue(!!highlight);
      highlightButton = highlight;
    });

    test('does not exist with read aloud disabled', async () => {
      chrome.readingMode.isReadAloudEnabled = false;
      await createToolbar();
      assertFalse(!!shadowRoot.querySelector<CrButtonElement>('#highlight'));
    });

    test('shows highlight menu on click', () => {
      stubAnimationFrame();
      highlightButton.click();
      assertTrue(toolbar.$.highlightMenu.$.menu.$.lazyMenu.get().open);
    });

    test('icon defaults to on', () => {
      assertStringContains(highlightButton.ironIcon!, 'highlight-on');
    });

    test('highlight off updates button icon', async () => {
      await changeHighlight(chrome.readingMode.noHighlighting);
      assertStringContains(highlightButton.ironIcon!, 'highlight-off');
    });

    test('highlight granularities update button icon', async () => {
      await changeHighlight(chrome.readingMode.noHighlighting);
      await changeHighlight(chrome.readingMode.wordHighlighting);
      assertStringContains(highlightButton.ironIcon!, 'highlight-on');

      await changeHighlight(chrome.readingMode.noHighlighting);
      await changeHighlight(chrome.readingMode.autoHighlighting);
      assertStringContains(highlightButton.ironIcon!, 'highlight-on');
    });
  });

  suite('audio buttons', () => {
    let playPauseButton: CrIconButtonElement;
    let nextButton: CrIconButtonElement;
    let previousButton: CrIconButtonElement;

    setup(async () => {
      chrome.readingMode.isReadAloudEnabled = true;
      await createToolbar();

      const playPause = getButton('play-pause');
      assertTrue(!!playPause, 'play');
      playPauseButton = playPause;
      const next = getButton('nextGranularity');
      assertTrue(!!next, 'next');
      nextButton = next;
      const previous = getButton('previousGranularity');
      assertTrue(!!previous, 'previous');
      previousButton = previous;

      toolbar.isReadAloudPlayable = true;
      return microtasksFinished();
    });

    test('all buttons disabled when not playable', async () => {
      toolbar.isReadAloudPlayable = false;
      await microtasksFinished();

      assertTrue(playPauseButton.disabled);
      assertTrue(nextButton.disabled);
      assertTrue(previousButton.disabled);
    });

    test('next button emits next event', async () => {
      let nextEmitted = false;
      document.addEventListener(
          ToolbarEvent.NEXT_GRANULARITY, () => nextEmitted = true);

      nextButton.click();
      await microtasksFinished();

      assertTrue(nextEmitted);
    });

    test('next button logs next event', async () => {
      nextButton.click();
      await microtasksFinished();

      assertEquals(
          'Accessibility.ReadAnything.ReadAloudNextButtonSessionCount',
          await metrics.whenCalled('incrementMetricCount'));
    });

    test('next button hidden when speech is inactive', async () => {
      toolbar.isSpeechActive = false;
      await microtasksFinished();
      assertFalse(isVisible(nextButton));
    });

    test('next button showing when speech is active', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();
      assertTrue(isVisible(nextButton));
    });

    test('previous button emits previous event', async () => {
      let previousEmitted = false;
      document.addEventListener(
          ToolbarEvent.PREVIOUS_GRANULARITY, () => previousEmitted = true);

      previousButton.click();
      await microtasksFinished();

      assertTrue(previousEmitted);
    });

    test('previous button logs previous event', async () => {
      previousButton.click();
      await microtasksFinished();

      assertEquals(
          'Accessibility.ReadAnything.ReadAloudPreviousButtonSessionCount',
          await metrics.whenCalled('incrementMetricCount'));
    });

    test('previous button hidden when speech is inactive', async () => {
      toolbar.isSpeechActive = false;
      await microtasksFinished();
      assertFalse(isVisible(previousButton));
    });

    test('previous button showing when speech is active', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();
      assertTrue(isVisible(previousButton));
    });

    test('play button emits play pause event', async () => {
      let clicksEmitted = 0;
      document.addEventListener(ToolbarEvent.PLAY_PAUSE, () => clicksEmitted++);

      playPauseButton.click();
      await microtasksFinished();
      playPauseButton.click();
      await microtasksFinished();

      assertEquals(2, clicksEmitted);
    });

    test('play button logs play event when speech inactive', async () => {
      toolbar.isSpeechActive = false;

      playPauseButton.click();
      await microtasksFinished();

      assertEquals(
          'Accessibility.ReadAnything.ReadAloudPlaySessionCount',
          await metrics.whenCalled('incrementMetricCount'));
    });

    test('play button logs pause event when speech active', async () => {
      toolbar.isSpeechActive = true;

      playPauseButton.click();
      await microtasksFinished();

      assertEquals(
          'Accessibility.ReadAnything.ReadAloudPauseSessionCount',
          await metrics.whenCalled('incrementMetricCount'));
    });

    test('play button logs pause source when speech active', async () => {
      toolbar.isSpeechActive = true;

      playPauseButton.click();
      await microtasksFinished();

      assertEquals(
          chrome.readingMode.pauseButtonStopSource,
          await metrics.whenCalled('recordSpeechStopSource'));
    });

    test('button while speech inactive indicates click to play', async () => {
      toolbar.isSpeechActive = false;
      await microtasksFinished();

      assertEquals('read-anything-20:play', playPauseButton.ironIcon);
      assertStringContains('play (k)', playPauseButton.title.toLowerCase());
      assertStringContains(
          'play / pause, keyboard shortcut k',
          playPauseButton.ariaLabel!.toLowerCase());
    });

    test('button while speech active indicates click to pause', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();

      assertEquals('read-anything-20:pause', playPauseButton.ironIcon);
      assertStringContains('pause (k)', playPauseButton.title.toLowerCase());
      assertStringContains(
          'play / pause, keyboard shortcut k',
          playPauseButton.ariaLabel!.toLowerCase());
    });
  });

  suite('link button', () => {
    let menuButton: CrIconButtonElement;

    setup(async () => {
      await createToolbar();
      const linkButton = getButton(LINK_TOGGLE_BUTTON_ID);
      assertTrue(!!linkButton);
      menuButton = linkButton;
    });

    test('by default links are on and button is enabled', () => {
      assertEquals(LINKS_ENABLED_ICON, menuButton.ironIcon);
      assertTrue(chrome.readingMode.linksEnabled);
      assertStringContains('disable links', menuButton.title.toLowerCase());
      assertStringContains(
          'disable links', menuButton.ariaLabel!.toLowerCase());
      assertFalse(menuButton.disabled);
    });

    test('links turn off on click', async () => {
      menuButton.click();
      await microtasksFinished();

      assertEquals(LINKS_DISABLED_ICON, menuButton.ironIcon);
      assertFalse(chrome.readingMode.linksEnabled);
      assertStringContains('enable links', menuButton.title.toLowerCase());
      assertStringContains('enable links', menuButton.ariaLabel!.toLowerCase());
    });

    test('links turn on on second click', async () => {
      menuButton.click();
      await microtasksFinished();
      menuButton.click();
      await microtasksFinished();

      assertEquals(LINKS_ENABLED_ICON, menuButton.ironIcon);
      assertTrue(chrome.readingMode.linksEnabled);
      assertStringContains('disable links', menuButton.title.toLowerCase());
      assertStringContains(
          'disable links', menuButton.ariaLabel!.toLowerCase());
    });

    test('event is propagated on click', () => {
      let linksToggled = false;
      document.addEventListener(ToolbarEvent.LINKS, () => linksToggled = true);

      menuButton.click();

      assertTrue(linksToggled);
    });

    test('when speech active, button is disabled', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();
      assertTrue(menuButton.disabled);
    });

    test('when speech not active, button is enabled', async () => {
      toolbar.isSpeechActive = false;
      await microtasksFinished();
      assertFalse(menuButton.disabled);
    });
  });

  suite('image button', () => {
    let menuButton: CrIconButtonElement;

    async function getImageButton() {
      chrome.readingMode.imagesFeatureEnabled = true;
      await createToolbar();
      const imageButton = getButton(IMAGES_TOGGLE_BUTTON_ID);
      assertTrue(!!imageButton);
      menuButton = imageButton;
    }

    test('does not show with flag disabled', async () => {
      chrome.readingMode.imagesFeatureEnabled = false;
      await createToolbar();

      const imageButton = getButton(IMAGES_TOGGLE_BUTTON_ID);

      assertFalse(!!imageButton);
    });

    test('by default images are off and button is enabled', async () => {
      await getImageButton();

      assertEquals(IMAGES_DISABLED_ICON, menuButton.ironIcon);
      assertFalse(chrome.readingMode.imagesEnabled);
      assertStringContains('enable images', menuButton.title.toLowerCase());
      assertStringContains(
          'enable images', menuButton.ariaLabel!.toLowerCase());
      assertFalse(menuButton.disabled);
    });

    test('images turn off on click', async () => {
      await getImageButton();

      menuButton.click();
      await microtasksFinished();

      assertEquals(IMAGES_ENABLED_ICON, menuButton.ironIcon);
      assertTrue(chrome.readingMode.imagesEnabled);
      assertStringContains('disable images', menuButton.title.toLowerCase());
      assertStringContains(
          'disable images', menuButton.ariaLabel!.toLowerCase());
    });

    test('images turn on on second click', async () => {
      await getImageButton();

      menuButton.click();
      await microtasksFinished();
      menuButton.click();
      await microtasksFinished();

      assertEquals(IMAGES_DISABLED_ICON, menuButton.ironIcon);
      assertFalse(chrome.readingMode.imagesEnabled);
      assertStringContains('enable images', menuButton.title.toLowerCase());
      assertStringContains(
          'enable images', menuButton.ariaLabel!.toLowerCase());
    });

    test('event is propagated on click', async () => {
      let imagesToggled = false;
      document.addEventListener(
          ToolbarEvent.IMAGES, () => imagesToggled = true);
      await getImageButton();

      menuButton.click();

      assertTrue(imagesToggled);
    });

    test('when speech active, button is disabled', async () => {
      await getImageButton();

      toolbar.isSpeechActive = true;
      await microtasksFinished();

      assertTrue(menuButton.disabled);
    });

    test('when speech not active, button is enabled', async () => {
      await getImageButton();

      toolbar.isSpeechActive = false;
      await microtasksFinished();

      assertFalse(menuButton.disabled);
    });
  });
});
