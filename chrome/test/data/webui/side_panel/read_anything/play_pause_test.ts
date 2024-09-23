// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MetricsBrowserProxyImpl, ReadAnythingLogger, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';


suite('PlayPause', () => {
  let toolbar: ReadAnythingToolbarElement;
  let metrics: TestMetricsBrowserProxy;
  let playPauseButton: CrIconButtonElement;
  let granularityContainer: HTMLElement;
  let clickEmitted: boolean;

  setup(() => {
    suppressInnocuousErrors();
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metrics);
    ReadAnythingLogger.setInstance(new ReadAnythingLogger());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;

    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    flush();

    playPauseButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#play-pause')!;
    granularityContainer = toolbar.shadowRoot!.querySelector<HTMLElement>(
        '#granularity-container')!;
    clickEmitted = false;
    document.addEventListener(
        ToolbarEvent.PLAY_PAUSE, () => clickEmitted = true);
  });

  test('on click emits click event', () => {
    playPauseButton.click();
    assertTrue(clickEmitted);

    clickEmitted = false;
    playPauseButton.click();
    assertTrue(clickEmitted);
  });

  test('on click logs click event', async () => {
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

  test('when playing', () => {
    toolbar.isSpeechActive = true;

    // Test that button indicates speech is playing
    assertEquals('read-anything-20:pause', playPauseButton.ironIcon);
    assertStringContains('pause (k)', playPauseButton.title.toLowerCase());
    assertStringContains(
        'play / pause, keyboard shortcut k',
        playPauseButton.ariaLabel!.toLowerCase());

    // Test that granularity menu buttons show
    assertTrue(isVisible(granularityContainer));
  });

  test('when paused', () => {
    toolbar.isSpeechActive = false;

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
