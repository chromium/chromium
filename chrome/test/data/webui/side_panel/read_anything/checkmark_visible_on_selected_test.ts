// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('CheckmarkVisibleOnSelected', () => {
  let toolbar: ReadAnythingToolbarElement;

  setup(function() {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  function createToolbar(): void {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
  }

  function assertCheckMarkVisible(
      checkMarks: NodeListOf<HTMLElement>, expectedIndex: number): void {
    checkMarks.forEach((element, index) => {
      assertEquals(
          index === expectedIndex ? 'visible' : 'hidden',
          element.style.visibility);
    });
  }

  function assertCheckMarksForDropdown(dropdown: HTMLElement): void {
    const buttons =
        dropdown.querySelectorAll<HTMLButtonElement>('.dropdown-item');
    const checkMarks = dropdown.querySelectorAll<HTMLElement>('.check-mark');
    assertEquals(buttons.length, checkMarks.length);
    buttons.forEach((button, index) => {
      button.click();
      assertCheckMarkVisible(checkMarks, index);
    });
  }

  suite('with read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = true;
      createToolbar();
    });

    test('rate menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.rateMenu);
    });

    test('line spacing menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.lineSpacingMenu);
    });

    test('letter spacing menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.letterSpacingMenu);
    });

    test('color menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.colorMenu);
    });

    test('font menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.fontMenu);
    });
  });

  suite('without read aloud', () => {
    setup(() => {
      chrome.readingMode.isReadAloudEnabled = false;
      createToolbar();
    });

    test('font menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.fontMenu);
    });

    test('line spacing menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.lineSpacingMenu);
    });

    test('letter spacing menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.letterSpacingMenu);
    });

    test('color menu has checkmarks', () => {
      assertCheckMarksForDropdown(toolbar.$.colorMenu);
    });
  });

});
