// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import type {MediaMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('MediaMenuElement', () => {
  let mediaMenu: MediaMenuElement;
  let metrics: TestMetricsBrowserProxy;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    mediaMenu = document.createElement('media-menu');
    document.body.appendChild(mediaMenu);
  });

  function getImagesButton(): HTMLButtonElement {
    const actionMenu = mediaMenu.$.lazyMenu.get();
    return actionMenu.querySelector<HTMLButtonElement>('#images-toggle-button')!
        ;
  }

  function getLinksButton(): HTMLButtonElement {
    const actionMenu = mediaMenu.$.lazyMenu.get();
    return actionMenu.querySelector<HTMLButtonElement>('#links-toggle-button')!;
  }

  test('has no internal separator between toggles', () => {
    const actionMenu = mediaMenu.$.lazyMenu.get();
    const separator = actionMenu.querySelector<HTMLElement>('.separator');
    assertFalse(!!separator);
  });

  test('images prop update changes toggle checked state', async () => {
    mediaMenu.settingsPrefs = {
      ...mediaMenu.settingsPrefs,
      imagesEnabled: true,
    };
    await microtasksFinished();

    const imagesToggle =
        getImagesButton().querySelector<CrToggleElement>('cr-toggle')!;
    assertTrue(imagesToggle.checked);

    mediaMenu.settingsPrefs = {
      ...mediaMenu.settingsPrefs,
      imagesEnabled: false,
    };
    await microtasksFinished();

    assertFalse(imagesToggle.checked);
  });

  test('links prop update changes toggle checked state', async () => {
    mediaMenu.settingsPrefs = {
      ...mediaMenu.settingsPrefs,
      linksEnabled: true,
    };
    await microtasksFinished();

    const linksToggle =
        getLinksButton().querySelector<CrToggleElement>('cr-toggle')!;
    assertTrue(linksToggle.checked);

    mediaMenu.settingsPrefs = {
      ...mediaMenu.settingsPrefs,
      linksEnabled: false,
    };
    await microtasksFinished();

    assertFalse(linksToggle.checked);
  });

  test('on images row click toggles images exactly once', async () => {
    let imagesToggledCount = 0;
    chrome.readingMode.onImagesEnabledToggled = () => {
      imagesToggledCount++;
      chrome.readingMode.imagesEnabled = !chrome.readingMode.imagesEnabled;
    };
    chrome.readingMode.imagesEnabled = false;
    mediaMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      imagesEnabled: false,
    };
    await microtasksFinished();

    getImagesButton().click();
    await microtasksFinished();

    assertEquals(1, imagesToggledCount);
    assertTrue(chrome.readingMode.imagesEnabled);
    assertTrue(
        getImagesButton().querySelector<CrToggleElement>('cr-toggle')!.checked);
    assertEquals(
        ReadAnythingSettingsChange.IMAGES_ENABLED_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));

    // Clicking directly on cr-toggle should also toggle once without double
    // click.
    const imagesToggle =
        getImagesButton().querySelector<CrToggleElement>('cr-toggle')!;
    imagesToggle.click();
    await microtasksFinished();

    assertEquals(2, imagesToggledCount);
    assertFalse(chrome.readingMode.imagesEnabled);
    assertFalse(imagesToggle.checked);
  });

  test('on links row click toggles links exactly once', async () => {
    let linksToggledCount = 0;
    chrome.readingMode.onLinksEnabledToggled = () => {
      linksToggledCount++;
      chrome.readingMode.linksEnabled = !chrome.readingMode.linksEnabled;
    };
    chrome.readingMode.linksEnabled = false;
    mediaMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      linksEnabled: false,
    };
    await microtasksFinished();

    getLinksButton().click();
    await microtasksFinished();

    assertEquals(1, linksToggledCount);
    assertTrue(chrome.readingMode.linksEnabled);
    assertTrue(
        getLinksButton().querySelector<CrToggleElement>('cr-toggle')!.checked);
    assertEquals(
        ReadAnythingSettingsChange.LINKS_ENABLED_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));

    const linksToggle =
        getLinksButton().querySelector<CrToggleElement>('cr-toggle')!;
    linksToggle.click();
    await microtasksFinished();

    assertEquals(2, linksToggledCount);
    assertFalse(chrome.readingMode.linksEnabled);
    assertFalse(linksToggle.checked);
  });

  test('toggles are disabled when speech is active', async () => {
    mediaMenu.isSpeechActive = true;
    await microtasksFinished();

    const imagesButton = getImagesButton();
    const linksButton = getLinksButton();
    assertTrue(imagesButton.disabled);
    assertTrue(linksButton.disabled);

    const imagesToggle =
        imagesButton.querySelector<CrToggleElement>('cr-toggle')!;
    const linksToggle =
        linksButton.querySelector<CrToggleElement>('cr-toggle')!;
    assertTrue(imagesToggle.disabled);
    assertTrue(linksToggle.disabled);

    let imagesEventWasFired = false;
    mediaMenu.addEventListener(
        ToolbarEvent.IMAGES, () => imagesEventWasFired = true);
    imagesButton.click();
    assertFalse(imagesEventWasFired);

    let linksEventWasFired = false;
    mediaMenu.addEventListener(
        ToolbarEvent.LINKS, () => linksEventWasFired = true);
    linksButton.click();
    assertFalse(linksEventWasFired);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    mediaMenu.open(document.body);
    assertTrue(mediaMenu.$.lazyMenu.get().open);
    mediaMenu.close();
    assertFalse(mediaMenu.$.lazyMenu.get().open);
  });
});
