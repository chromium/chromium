// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {moreOptionsClass} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

// TODO: b/40275871 - Add more tests.
suite('ToolbarOverflow', () => {
  let toolbar: ReadAnythingToolbarElement;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    return microtasksFinished();
  });

  suite('on reset toolbar', () => {
    setup(() => {
      // Open the menu first so we can be sure the event is what closes it.
      toolbar.$.moreOptionsMenu.get().showAt(toolbar);
      assertTrue(toolbar.$.moreOptionsMenu.get().open);

      toolbar.$.toolbarContainer.dispatchEvent(
          new CustomEvent('reset-toolbar'));
      return microtasksFinished();
    });

    test('more options closed', () => {
      assertFalse(toolbar.$.moreOptionsMenu.get().open);
    });

    test('more options empty', () => {
      const moreOptionsButtons =
          toolbar.$.moreOptionsMenu.get().querySelectorAll<HTMLElement>(
              moreOptionsClass);
      assertEquals(0, moreOptionsButtons.length);
    });
  });

  suite('on toolbar overflow', () => {
    async function overflow(numOverflowButtons: number): Promise<void> {
      toolbar.$.toolbarContainer.dispatchEvent(
          new CustomEvent('toolbar-overflow', {
            bubbles: true,
            composed: true,
            detail: {numOverflowButtons},
          }));
      await microtasksFinished();
      toolbar.$.moreOptionsMenu.get();
    }

    test('more options contains overflow', async () => {
      let numOverflow = 3;
      await overflow(numOverflow);
      assertEquals(
          numOverflow,
          toolbar.$.moreOptionsMenu.get()
              .querySelectorAll<HTMLElement>(moreOptionsClass)
              .length);

      numOverflow = 5;
      await overflow(numOverflow);
      assertEquals(
          numOverflow,
          toolbar.$.moreOptionsMenu.get()
              .querySelectorAll<HTMLElement>(moreOptionsClass)
              .length);
    });

    test('click from overflow opens correct menu', async () => {
      stubAnimationFrame();
      const numOverflow = 3;
      await overflow(numOverflow);

      const letterSpacingButton =
          toolbar.$.moreOptionsMenu.get().querySelector<CrIconButtonElement>(
              '#letter-spacing');
      assertTrue(!!letterSpacingButton);
      letterSpacingButton.click();

      assertTrue(toolbar.$.letterSpacingMenu.$.menu.$.lazyMenu.get().open);
    });
  });
});
