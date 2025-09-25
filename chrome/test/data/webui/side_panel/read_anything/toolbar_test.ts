// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {IMAGES_DISABLED_ICON, IMAGES_ENABLED_ICON, IMAGES_TOGGLE_BUTTON_ID, LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('Toolbar', () => {
  let toolbar: ReadAnythingToolbarElement;
  let shadowRoot: ShadowRoot;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  async function createToolbar(): Promise<void> {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();
    assertTrue(!!toolbar.shadowRoot);
    shadowRoot = toolbar.shadowRoot;
  }

  suite('with read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = true;
      return createToolbar();
    });

    test('has text settings menus', () => {
      stubAnimationFrame();

      const colorButton =
          shadowRoot.querySelector<CrIconButtonElement>('#color');
      assertTrue(!!colorButton);
      colorButton.click();
      assertTrue(toolbar.$.colorMenu.$.menu.$.lazyMenu.get().open);

      const lineSpacingButton =
          shadowRoot.querySelector<CrIconButtonElement>('#line-spacing');
      assertTrue(!!lineSpacingButton);
      lineSpacingButton.click();
      assertTrue(toolbar.$.lineSpacingMenu.$.menu.$.lazyMenu.get().open);

      const letterSpacingButton =
          shadowRoot.querySelector<CrIconButtonElement>('#letter-spacing');
      assertTrue(!!letterSpacingButton);
      letterSpacingButton.click();
      assertTrue(toolbar.$.letterSpacingMenu.$.menu.$.lazyMenu.get().open);
    });

    test('has rate menu', () => {
      stubAnimationFrame();
      const rateButton = shadowRoot.querySelector<CrIconButtonElement>('#rate');

      assertTrue(!!rateButton);
      rateButton.click();

      assertTrue(toolbar.$.rateMenu.$.menu.$.lazyMenu.get().open);
    });

    test('has audio controls', () => {
      const audioControls = toolbar.shadowRoot.querySelector('#audio-controls');
      assertTrue(!!audioControls);
    });
  });

  suite('without read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = false;
      return createToolbar();
    });

    test('has text settings menus', () => {
      stubAnimationFrame();

      const colorButton =
          shadowRoot.querySelector<CrIconButtonElement>('#color');
      assertTrue(!!colorButton);
      colorButton.click();
      assertTrue(toolbar.$.colorMenu.$.menu.$.lazyMenu.get().open);

      const lineSpacingButton =
          shadowRoot.querySelector<CrIconButtonElement>('#line-spacing');
      assertTrue(!!lineSpacingButton);
      lineSpacingButton.click();
      assertTrue(toolbar.$.lineSpacingMenu.$.menu.$.lazyMenu.get().open);

      const letterSpacingButton =
          shadowRoot.querySelector<CrIconButtonElement>('#letter-spacing');
      assertTrue(!!letterSpacingButton);
      letterSpacingButton.click();
      assertTrue(toolbar.$.letterSpacingMenu.$.menu.$.lazyMenu.get().open);
    });

    test('does not have rate menu', () => {
      stubAnimationFrame();
      const rateButton = shadowRoot.querySelector<CrIconButtonElement>('#rate');
      assertFalse(!!rateButton);
    });

    test('does not have audio controls', () => {
      const audioControls = shadowRoot.querySelector('#audio-controls');
      assertFalse(!!audioControls);
    });
  });

  suite('link button', () => {
    let menuButton: CrIconButtonElement;

    setup(async () => {
      await createToolbar();
      const linkButton = shadowRoot.querySelector<CrIconButtonElement>(
          '#' + LINK_TOGGLE_BUTTON_ID);
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
      const linkButton = shadowRoot.querySelector<CrIconButtonElement>(
          '#' + IMAGES_TOGGLE_BUTTON_ID);
      assertTrue(!!linkButton);
      menuButton = linkButton;
    }

    test('does not show with flag disabled', async () => {
      chrome.readingMode.imagesFeatureEnabled = false;
      await createToolbar();

      const linkButton = shadowRoot.querySelector<CrIconButtonElement>(
          '#' + IMAGES_TOGGLE_BUTTON_ID);

      assertFalse(!!linkButton);
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
