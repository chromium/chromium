// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {MENU_SHOW_DELAY_MS, SettingsOption, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement, SettingsMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome-untrusted://webui-test/keyboard_mock_interactions.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('Toolbar', () => {
  let toolbar: ReadAnythingToolbarElement;
  let shadowRoot: ShadowRoot;
  let metrics: TestMetricsBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;
  let audioBrowserProxy: TestAudioBrowserProxy;

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
    const result = setupTestEnvironment();
    visualBrowserProxy = result.visualBrowserProxy;
    audioBrowserProxy = result.audioBrowserProxy;
    metrics = result.metrics;
    stubAnimationFrame();
    return createToolbar();
  });

  test('has audio controls', () => {
    const audioControls = shadowRoot.querySelector('#audio-controls');
    assertTrue(!!audioControls);
  });

  suite('tab index', () => {
    setup(() => {
      assertEquals(toolbar.$.toolbarContainer.tabIndex, 0);
    });

    test('is -1 after Tab keydown', () => {
      toolbar.$.toolbarContainer.dispatchEvent(new FocusEvent('blur'));
      assertEquals(toolbar.$.toolbarContainer.tabIndex, -1);
    });

    test('is reset after closing reading mode', async () => {
      toolbar.$.toolbarContainer.dispatchEvent(new FocusEvent('blur'));
      assertEquals(toolbar.$.toolbarContainer.tabIndex, -1);
      toolbar.presentationState = visualBrowserProxy.inHiddenPresentationState;
      await microtasksFinished();
      assertEquals(toolbar.$.toolbarContainer.tabIndex, 0);
    });

    test('is reset after opening reading mode in side panel', async () => {
      toolbar.$.toolbarContainer.dispatchEvent(new FocusEvent('blur'));
      assertEquals(toolbar.$.toolbarContainer.tabIndex, -1);
      toolbar.presentationState =
          visualBrowserProxy.inSidePanelPresentationState;
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

    test('shows rate menu on click', async () => {
      rateButton.click();
      await microtasksFinished();
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
      toolbar.isSpeechActive = true;
      return microtasksFinished();
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
      await microtasksFinished();

      const whenFired = eventToPromise(ToolbarEvent.NEXT_GRANULARITY, toolbar);

      nextButton.click();
      await whenFired;
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

      const whenFired =
          eventToPromise(ToolbarEvent.PREVIOUS_GRANULARITY, toolbar);

      previousButton.click();
      await whenFired;
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
      const whenFired1 = eventToPromise(ToolbarEvent.PLAY_PAUSE, toolbar);

      playPauseButton.click();
      await whenFired1;

      const whenFired2 = eventToPromise(ToolbarEvent.PLAY_PAUSE, toolbar);

      playPauseButton.click();
      await whenFired2;
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
          audioBrowserProxy.pauseButtonStopSource,
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
      visualBrowserProxy.experimentalPlaybackUiEnabled = false;
      await createToolbar();
      assertFalse(!!getButton('ai-playback-toggle'));
    });

    suite('with flag enabled', () => {
      let aiPlaybackButton: CrIconButtonElement;

      setup(async () => {
        visualBrowserProxy.experimentalPlaybackUiEnabled = true;
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

  test('restoreSettingsFromPrefs event updates toolbar', async () => {
    audioBrowserProxy.speechRate = 1.5;

    visualBrowserProxy.restoreSettingsFromPrefs.callListeners();
    await microtasksFinished();

    const rateButton = shadowRoot.querySelector('#rate');
    assertTrue(!!rateButton);
    assertEquals('1.5', rateButton.textContent.trim());
  });

  suite('settings menu', () => {
    let moreButton: CrIconButtonElement;
    let settingsMenu: SettingsMenuElement;

    function getMenuItem(id: SettingsOption): HTMLElement|null {
      const actionMenu = settingsMenu.$.lazyMenu.get();
      const menuItems =
          Array.from(actionMenu.querySelectorAll<HTMLElement>('.menu-row'));
      return menuItems.find(item => item.id === id) || null;
    }

    setup(async () => {
      const more = getButton('more');
      assertTrue(!!more);
      moreButton = more;

      settingsMenu = toolbar.$.settingsMenu;

      moreButton.click();
      settingsMenu.$.lazyMenu.get();
      await microtasksFinished();
    });

    teardown(async () => {
      if (toolbar) {
        for (const menu of Object.values(toolbar.settingsMenu_)) {
          menu?.close();
        }
      }
      if (settingsMenu) {
        settingsMenu.close();
      }
      return microtasksFinished();
    });

    test('shows dropdown menu on click', () => {
      assertTrue(settingsMenu.$.lazyMenu.get().open);
    });

    test('isSpeechActive is passed to settings menu', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();
      assertTrue(settingsMenu.isSpeechActive);

      toolbar.isSpeechActive = false;
      await microtasksFinished();
      assertFalse(settingsMenu.isSpeechActive);
    });

    test('opens submenus on click', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      targetItem.click();
      assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    });

    test('opens appearance submenu on click when flag enabled', async () => {
      settingsMenu.settingsPrefs = {...settingsMenu.settingsPrefs};
      visualBrowserProxy.readAnythingImprovedUiEnabled = true;
      await microtasksFinished();
      const targetItem = getMenuItem(SettingsOption.APPEARANCE);
      assertTrue(!!targetItem);
      targetItem.click();
      assertTrue(toolbar.$.appearanceMenu.$.menu.$.lazyMenu.get().open);
    });

    test('opens submenus on hover', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      const timer = new MockTimer();
      timer.install();

      targetItem.dispatchEvent(new PointerEvent(
          'pointerenter', {bubbles: true, cancelable: true, view: window}));
      timer.tick(MENU_SHOW_DELAY_MS + 10);
      timer.uninstall();
      assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    });

    test('does not open submenu before delay', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      const timer = new MockTimer();
      timer.install();

      targetItem.dispatchEvent(new PointerEvent(
          'pointerenter', {bubbles: true, cancelable: true, view: window}));
      timer.tick(MENU_SHOW_DELAY_MS - 10);

      assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
      timer.uninstall();
    });

    test('closes when clicked outside', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      const timer = new MockTimer();
      timer.install();

      targetItem.dispatchEvent(new PointerEvent(
          'pointerenter', {bubbles: true, cancelable: true, view: window}));
      timer.tick(MENU_SHOW_DELAY_MS + 10);
      timer.uninstall();

      assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);

      moreButton.click();
      const actionMenu = settingsMenu.$.lazyMenu.get();
      assertFalse(actionMenu.open);
      assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    });

    test('closes opened menu if mouse is moved out', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      const timer = new MockTimer();
      timer.install();

      targetItem.dispatchEvent(new PointerEvent(
          'pointerenter', {bubbles: true, cancelable: true, view: window}));
      timer.tick(MENU_SHOW_DELAY_MS + 10);

      assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);

      targetItem.dispatchEvent(new PointerEvent('pointerleave', {
        bubbles: true,
        cancelable: true,
        view: window,
        relatedTarget: moreButton,
      }));
      timer.tick(MENU_SHOW_DELAY_MS + 10);
      timer.uninstall();

      const actionMenu = settingsMenu.$.lazyMenu.get();
      assertTrue(actionMenu.open);
      assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    });

    test('clicking language menu does not close settings menu', async () => {
      // Open the voice selection menu
      const targetItem = getMenuItem(SettingsOption.VOICE_SELECTION);
      assertTrue(!!targetItem);
      targetItem.click();
      assertTrue(toolbar.$.voiceSelectionMenu.$.voiceSelectionMenu.get().open);

      // Open the language menu
      const languageMenuButton =
          toolbar.$.voiceSelectionMenu.shadowRoot.querySelector<HTMLElement>(
              '.language-menu-button');
      assertTrue(!!languageMenuButton);
      languageMenuButton.click();
      await microtasksFinished();

      const languageMenu =
          toolbar.$.voiceSelectionMenu.shadowRoot.querySelector(
              'language-menu');
      assertTrue(!!languageMenu);
      const dialog = languageMenu.$.languageMenu;

      dialog.click();

      assertTrue(settingsMenu.$.lazyMenu.get().open);
    });

    test('cancels open timer on key event', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      const timer = new MockTimer();
      timer.install();

      targetItem.dispatchEvent(new PointerEvent(
          'pointerenter', {bubbles: true, cancelable: true, view: window}));
      const elapsedTime = MENU_SHOW_DELAY_MS - 10;
      timer.tick(elapsedTime);

      keyDownOn(settingsMenu, 0, undefined, 'ArrowDown');
      timer.tick(elapsedTime + 20);
      assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
      timer.uninstall();
    });

    test('opens submenu of focused element on horizontal forward arrow', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      targetItem.focus();

      keyDownOn(settingsMenu, 0, undefined, 'ArrowRight');
      assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
    });

    test('focuses a different element on vertical forward arrow', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      targetItem.focus();

      keyDownOn(settingsMenu, 0, undefined, 'ArrowUp');
      assertNotEquals(document.activeElement, targetItem);
    });

    test('does nothing on toggle items on arrow event', () => {
      const targetItem = getMenuItem(SettingsOption.LINKS);
      assertTrue(!!targetItem);
      targetItem.focus();

      keyDownOn(settingsMenu, 0, undefined, 'ArrowRight');
      assertEquals(0, visualBrowserProxy.getCallCount('onLinksEnabledToggled'));
    });

    test('closes submenu on horizontal backward arrow or escape key', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      const targetMenu = toolbar.$.fontMenu.$.menu.$.lazyMenu.get();
      assertTrue(!!targetItem);

      targetItem.click();
      assertTrue(targetMenu.open);
      keyDownOn(settingsMenu, 0, undefined, 'ArrowLeft');
      assertFalse(targetMenu.open);

      targetItem.click();
      assertTrue(targetMenu.open);
      keyDownOn(settingsMenu, 0, undefined, 'Escape');
      assertFalse(targetMenu.open);
    });

    test('does not open submenus after settings menu is closed', () => {
      const targetItem = getMenuItem(SettingsOption.FONT);
      assertTrue(!!targetItem);
      const timer = new MockTimer();
      timer.install();

      targetItem.dispatchEvent(new PointerEvent(
          'pointerenter', {bubbles: true, cancelable: true, view: window}));
      timer.tick(MENU_SHOW_DELAY_MS - 1);
      settingsMenu.close();
      timer.tick(1);

      assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
      timer.uninstall();
    });

    test('shows close button in immersive mode', async () => {
      toolbar.isImmersiveMode = true;
      await microtasksFinished();
      const closeButton = getButton('close');
      assertTrue(!!closeButton);
    });

    test('does not show close button in side panel mode', () => {
      const closeButton = getButton('close');
      assertFalse(!!closeButton);
    });

    test(
        'does not close opened submenu if mouse moves directly into it', () => {
          const targetItem = getMenuItem(SettingsOption.FONT);
          assertTrue(!!targetItem);
          const timer = new MockTimer();
          timer.install();

          targetItem.dispatchEvent(new PointerEvent(
              'pointerenter', {bubbles: true, cancelable: true, view: window}));
          timer.tick(MENU_SHOW_DELAY_MS + 10);

          const fontSubmenu = toolbar.$.fontMenu;
          assertTrue(fontSubmenu.$.menu.$.lazyMenu.get().open);

          // Simulate that mouse moved out of item, but we specify that the new
          // element is directly under the cursor.
          targetItem.dispatchEvent(new PointerEvent('pointerleave', {
            bubbles: true,
            cancelable: true,
            view: window,
            relatedTarget: fontSubmenu,
          }));

          timer.tick(MENU_SHOW_DELAY_MS + 10);
          timer.uninstall();

          const actionMenu = settingsMenu.$.lazyMenu.get();
          assertTrue(actionMenu.open);
          assertTrue(fontSubmenu.$.menu.$.lazyMenu.get().open);
        });

    test('invokes visualBrowserProxy on translation requested event', () => {
      visualBrowserProxy.translateEntryPointEnabled = true;
      settingsMenu.dispatchEvent(
          new CustomEvent(ToolbarEvent.TRANSLATION_REQUESTED));
      assertEquals(
          1, visualBrowserProxy.getCallCount('onTranslationRequested'));
    });
  });
});
