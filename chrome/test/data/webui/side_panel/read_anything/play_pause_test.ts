// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';


suite('PlayPause', () => {
  let toolbar: ReadAnythingToolbarElement;
  let metrics: TestMetricsBrowserProxy;
  let playPauseButton: CrIconButtonElement;
  let granularityContainer: HTMLElement;
  let clickEmitted: boolean;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = mockMetrics();
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;

    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();

    playPauseButton =
        toolbar.shadowRoot.querySelector<CrIconButtonElement>('#play-pause')!;
    granularityContainer = toolbar.shadowRoot.querySelector<HTMLElement>(
        '#granularity-container')!;
    clickEmitted = false;
    document.addEventListener(
        ToolbarEvent.PLAY_PAUSE, () => clickEmitted = true);
  });

  test('on click emits click event when read aloud is playable', async () => {
    toolbar.isReadAloudPlayable = true;
    await microtasksFinished();
    playPauseButton.click();
    await microtasksFinished();

    assertTrue(clickEmitted);

    clickEmitted = false;
    playPauseButton.click();
    await microtasksFinished();

    assertTrue(clickEmitted);
  });

  test('on click does not emit event when not playable', async () => {
    toolbar.isReadAloudPlayable = false;
    await microtasksFinished();
    playPauseButton.click();
    await microtasksFinished();
    assertFalse(clickEmitted);
  });

  test('on click logs click event', async () => {
    toolbar.isReadAloudPlayable = true;
    await microtasksFinished();
    toolbar.isSpeechActive = false;
    playPauseButton.click();
    assertEquals(
        'Accessibility.ReadAnything.ReadAloudPlaySessionCount',
        await metrics.whenCalled('incrementMetricCount'));

    metrics.reset();
    toolbar.isSpeechActive = true;
    playPauseButton.click();
    assertEquals(
        'Accessibility.ReadAnything.ReadAloudPauseSessionCount',
        await metrics.whenCalled('incrementMetricCount'));
  });

  test('on click logs speech stop pause source', async () => {
    toolbar.isReadAloudPlayable = true;
    await microtasksFinished();
    toolbar.isSpeechActive = false;
    playPauseButton.click();

    toolbar.isSpeechActive = true;
    playPauseButton.click();
    assertEquals(
        chrome.readingMode.pauseButtonStopSource,
        await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('when playing', async () => {
    toolbar.isSpeechActive = true;
    await microtasksFinished();

    // Test that button indicates speech is playing
    assertEquals('read-anything-20:pause', playPauseButton.ironIcon);
    assertStringContains('pause (k)', playPauseButton.title.toLowerCase());
    assertStringContains(
        'play / pause, keyboard shortcut k',
        playPauseButton.ariaLabel!.toLowerCase());

    // Test that granularity menu buttons show
    assertTrue(isVisible(granularityContainer));
  });

  test('when paused', async () => {
    toolbar.isSpeechActive = false;
    await microtasksFinished();
    // Test that button indicates speech is paused
    assertEquals('read-anything-20:play', playPauseButton.ironIcon);
    assertStringContains('play (k)', playPauseButton.title.toLowerCase());
    assertStringContains(
        'play / pause, keyboard shortcut k',
        playPauseButton.ariaLabel!.toLowerCase());

    // Test that granularity menu buttons hidden
    assertFalse(isVisible(granularityContainer));
  });
});
