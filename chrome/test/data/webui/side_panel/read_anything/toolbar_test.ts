// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {mockMetrics, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('Toolbar', () => {
  let toolbar: ReadAnythingToolbarElement;
  let shadowRoot: ShadowRoot;
  let metrics: TestMetricsBrowserProxy;

  async function createToolbar(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isImmersiveEnabled = true;
    metrics = mockMetrics();
    return createToolbar();
  });

  test('has audio controls', () => {
    const audioControls = shadowRoot.querySelector('#audio-controls');
    assertTrue(!!audioControls);
  });

  test('does not have highlight menu', () => {
    stubAnimationFrame();
    const highlightButton = getButton('highlight');
    assertFalse(!!highlightButton);
  });

  test('does not have voice menu', () => {
    stubAnimationFrame();
    const voiceButton = getButton('voice-selection');
    assertFalse(!!voiceButton);
  });

  test('does have settings menu', () => {
    stubAnimationFrame();
    const moreButton = getButton('more');
    assertTrue(!!moreButton);
  });

  suite('tab index', () => {
    setup(() => {
      assertEquals(toolbar.$.toolbarContainer.tabIndex, 0);
    });

    test('is -1 after Tab keydown', () => {
      stubAnimationFrame();
      toolbar.$.toolbarContainer.dispatchEvent(new FocusEvent('blur'));
      assertEquals(toolbar.$.toolbarContainer.tabIndex, -1);
    });

    test('is reset after closing reading mode', async () => {
      stubAnimationFrame();
      toolbar.$.toolbarContainer.dispatchEvent(new FocusEvent('blur'));
      assertEquals(toolbar.$.toolbarContainer.tabIndex, -1);
      toolbar.presentationState = 1;
      await microtasksFinished();
      assertEquals(toolbar.$.toolbarContainer.tabIndex, 0);
    });

    test('is reset after opening reading mode in side panel', async () => {
      stubAnimationFrame();
      toolbar.$.toolbarContainer.dispatchEvent(new FocusEvent('blur'));
      assertEquals(toolbar.$.toolbarContainer.tabIndex, -1);
      toolbar.presentationState = 2;
      await microtasksFinished();
      assertEquals(toolbar.$.toolbarContainer.tabIndex, 0);
    });
  });

  suite('rate button', () => {
    let rateButton: CrButtonElement;

    async function changeRate(rate: number) {
      toolbar.$.rateMenu.dispatchEvent(
          new CustomEvent(ToolbarEvent.RATE, {detail: {data: rate}}));
      await microtasksFinished();
    }

    setup(() => {
      const rate = shadowRoot.querySelector<CrButtonElement>('#rate');
      assertTrue(!!rate);
      rateButton = rate;
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

  suite('audio buttons', () => {
    let playPauseButton: CrIconButtonElement;
    let nextButton: CrIconButtonElement;
    let previousButton: CrIconButtonElement;

    setup(async () => {
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
      await microtasksFinished();
    });

    test('all buttons disabled when not playable', async () => {
      toolbar.isReadAloudPlayable = false;
      await microtasksFinished();

      assertTrue(playPauseButton.disabled);
      assertTrue(nextButton.disabled);
      assertTrue(previousButton.disabled);
    });

    test(
        'next and previous buttons are disabled if speech is not active',
        async () => {
          toolbar.isSpeechActive = false;
          await microtasksFinished();

          assertTrue(nextButton.disabled);
          assertTrue(previousButton.disabled);
        });

    test(
        'granularity buttons are visible when speech is inactive', async () => {
          toolbar.isSpeechActive = false;
          await microtasksFinished();

          assertTrue(isVisible(previousButton));
          assertTrue(isVisible(nextButton));
        });

    test('next button emits next event', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();

      let nextEmitted = false;
      document.addEventListener(
          ToolbarEvent.NEXT_GRANULARITY, () => nextEmitted = true);

      nextButton.click();
      await microtasksFinished();

      assertTrue(nextEmitted);
    });

    test('next button logs next event', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();

      nextButton.click();
      await microtasksFinished();

      assertEquals(
          'Accessibility.ReadAnything.ReadAloudNextButtonSessionCount',
          await metrics.whenCalled('incrementMetricCount'));
    });

    test('previous button emits previous event', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();

      let previousEmitted = false;
      document.addEventListener(
          ToolbarEvent.PREVIOUS_GRANULARITY, () => previousEmitted = true);

      previousButton.click();
      await microtasksFinished();

      assertTrue(previousEmitted);
    });

    test('previous button logs previous event', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();

      previousButton.click();
      await microtasksFinished();

      assertEquals(
          'Accessibility.ReadAnything.ReadAloudPreviousButtonSessionCount',
          await metrics.whenCalled('incrementMetricCount'));
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

      assertEquals(
          loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
              'read-anything-20:play-circle-filled' :
              'read-anything-20:play-old',
          playPauseButton.ironIcon);
      assertStringContains('play (k)', playPauseButton.title.toLowerCase());
      assertStringContains(
          'play / pause, keyboard shortcut k',
          playPauseButton.ariaLabel!.toLowerCase());
    });

    test('button while speech active indicates click to pause', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();

      assertEquals(
          loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
              'read-anything-20:pause-circle-filled' :
              'read-anything-20:pause-old',
          playPauseButton.ironIcon);
      assertStringContains('pause (k)', playPauseButton.title.toLowerCase());
      assertStringContains(
          'play / pause, keyboard shortcut k',
          playPauseButton.ariaLabel!.toLowerCase());
    });
  });

  suite('line focus button', () => {
    setup(async () => {
      chrome.readingMode.isLineFocusEnabled = true;
      await createToolbar();
    });

    test('shows with line focus on', async () => {
      toolbar.isLineFocusShowing = true;
      await microtasksFinished();

      assertTrue(!!getButton('line-focus-off'));
    });

    test('turns line focus off', async () => {
      toolbar.isLineFocusShowing = true;
      await microtasksFinished();
      const button = getButton('line-focus-off');
      const whenFired = eventToPromise<CustomEvent<{data: boolean}>>(
          ToolbarEvent.LINE_FOCUS_TOGGLE, toolbar);

      assertTrue(!!button);
      button.click();
      const event = await whenFired;

      assertFalse(event.detail.data);
    });

    test('does not show with line focus off', async () => {
      toolbar.isLineFocusShowing = false;
      await microtasksFinished();

      assertFalse(!!getButton('line-focus-off'));
    });
  });

  suite('ai playback button', () => {
    test('does not show with flag disabled', async () => {
      chrome.readingMode.isReadAnythingReadAloudExperimentalPlaybackUiEnabled =
          false;
      await createToolbar();
      assertFalse(!!getButton('ai-playback-toggle'));
    });

    suite('with flag enabled', () => {
      let aiPlaybackButton: CrIconButtonElement;

      setup(async () => {
        chrome.readingMode
            .isReadAnythingReadAloudExperimentalPlaybackUiEnabled = true;
        await createToolbar();

        const button = getButton('ai-playback-toggle');
        assertTrue(!!button);
        aiPlaybackButton = button;
      });

      test('click toggles active state and class', async () => {
        assertFalse(aiPlaybackButton.classList.contains('active'));

        aiPlaybackButton.click();
        await microtasksFinished();

        assertTrue(aiPlaybackButton.classList.contains('active'));

        aiPlaybackButton.click();
        await microtasksFinished();

        assertFalse(aiPlaybackButton.classList.contains('active'));
      });
    });
  });
});
