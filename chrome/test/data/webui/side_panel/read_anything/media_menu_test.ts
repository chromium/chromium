// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import type {MediaMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {setupTestEnvironment, stubAnimationFrame} from './common.js';

suite('MediaMenuElement', () => {
  let mediaMenu: MediaMenuElement;

  setup(() => {
    setupTestEnvironment();

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
    mediaMenu.imagesEnabled = true;
    await microtasksFinished();

    const imagesToggle =
        getImagesButton().querySelector<CrToggleElement>('cr-toggle')!;
    assertTrue(imagesToggle.checked);

    mediaMenu.imagesEnabled = false;
    await microtasksFinished();

    assertFalse(imagesToggle.checked);
  });

  test('links prop update changes toggle checked state', async () => {
    mediaMenu.linksEnabled = true;
    await microtasksFinished();

    const linksToggle =
        getLinksButton().querySelector<CrToggleElement>('cr-toggle')!;
    assertTrue(linksToggle.checked);

    mediaMenu.linksEnabled = false;
    await microtasksFinished();

    assertFalse(linksToggle.checked);
  });

  test('on images row click fires images toggle exactly once', async () => {
    let imagesEventCount = 0;
    document.addEventListener(ToolbarEvent.IMAGES, () => imagesEventCount++);

    getImagesButton().click();
    await microtasksFinished();
    assertEquals(1, imagesEventCount);

    // Clicking directly on cr-toggle should also toggle once without double
    // click.
    getImagesButton().querySelector<CrToggleElement>('cr-toggle')!.click();
    await microtasksFinished();
    assertEquals(2, imagesEventCount);
  });

  test('on links row click fires links toggle exactly once', async () => {
    let linksEventCount = 0;
    document.addEventListener(ToolbarEvent.LINKS, () => linksEventCount++);

    getLinksButton().click();
    await microtasksFinished();
    assertEquals(1, linksEventCount);

    getLinksButton().querySelector<CrToggleElement>('cr-toggle')!.click();
    await microtasksFinished();
    assertEquals(2, linksEventCount);
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
