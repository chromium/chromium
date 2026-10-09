// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {HighlightMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, setupTestEnvironment, stubAnimationFrame} from './common.js';
import type {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';

suite('HighlightMenuElement', () => {
  let highlightMenu: HighlightMenuElement;
  let audioBrowserProxy: TestAudioBrowserProxy;

  function createHighlightMenu() {
    highlightMenu = document.createElement('highlight-menu');
    document.body.appendChild(highlightMenu);
  }

  setup(() => {
    const result = setupTestEnvironment();
    audioBrowserProxy = result.audioBrowserProxy;
  });

  test('has checkmarks', () => {
    createHighlightMenu();
    assertCheckMarksForDropdown(highlightMenu);
  });

  test('highlight change closes all menus', async () => {
    createHighlightMenu();
    const closePromise = eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    highlightMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.HIGHLIGHT_CHANGE,
        {detail: {data: audioBrowserProxy.getNoHighlighting()}}));
    await closePromise;
  });

  test('has phrase highlighting option if flag enabled', () => {
    audioBrowserProxy.isPhraseHighlightingEnabledFlag = true;

    createHighlightMenu();

    const menu = highlightMenu.$.menu.$.lazyMenu.get();
    const options =
        Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
    const titles = options.map(button => button.textContent?.trim());
    assertEquals(5, titles.length);
    assertTrue(titles.includes('Phrase'));
  });

  test('does not have phrase highlighting option if flag disabled', () => {
    audioBrowserProxy.isPhraseHighlightingEnabledFlag = false;

    createHighlightMenu();

    const menu = highlightMenu.$.menu.$.lazyMenu.get();
    const options =
        Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
    const titles = options.map(button => button.textContent?.trim());
    assertEquals(4, titles.length);
    assertFalse(titles.includes('Phrase'));
  });

  test('restores saved highlight option', async () => {
    createHighlightMenu();
    const granularity = audioBrowserProxy.getWordHighlighting();
    const startingIndex = highlightMenu.$.menu.currentSelectedIndex;
    assertNotEquals(granularity, startingIndex);

    highlightMenu.highlightGranularity = granularity;
    await microtasksFinished();

    assertNotEquals(startingIndex, highlightMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved highlight is the same', async () => {
    createHighlightMenu();
    const startingIndex = highlightMenu.$.menu.currentSelectedIndex;

    highlightMenu.highlightGranularity = 0;
    await microtasksFinished();

    assertEquals(startingIndex, highlightMenu.$.menu.currentSelectedIndex);
  });

  test('can be closed programatically', () => {
    createHighlightMenu();
    stubAnimationFrame();
    highlightMenu.open(document.body);
    const innerMenu = highlightMenu.$.menu.$.lazyMenu.get();
    assertTrue(innerMenu.open);
    highlightMenu.close();
    assertFalse(innerMenu.open);
  });
});
