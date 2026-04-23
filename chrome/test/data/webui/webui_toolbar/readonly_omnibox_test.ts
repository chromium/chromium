// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertGE, assertLE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {OmniboxTextColor} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ReadonlyOmniboxElement} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('ReadonlyOmnibox', function() {
  let omnibox: ReadonlyOmniboxElement;

  function getTextPieces(): NodeListOf<HTMLElement> {
    return omnibox.shadowRoot.querySelectorAll<HTMLElement>(
        '#textContainer span');
  }

  function checkPiece(
      piece: HTMLElement|undefined, expectText: string, expectStrike: boolean,
      expectColor: string): void {
    assertTrue(piece !== undefined);
    assertEquals(expectText, piece.textContent);
    const style = piece.computedStyleMap();
    assertEquals(
        expectStrike ? 'line-through' : 'none',
        style.get('text-decoration')?.toString());
    assertEquals(expectColor, style.get('color')?.toString());
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    omnibox = document.createElement('readonly-omnibox');

    omnibox.style.setProperty('--color-omnibox-text', 'rgb(0, 255, 255)');
    omnibox.style.setProperty('--color-omnibox-text-dimmed', 'rgb(0, 255, 0)');
    omnibox.style.setProperty(
        '--color-omnibox-foreground-disabled', 'rgb(0, 0, 255)');
    omnibox.style.setProperty(
        '--color-omnibox-security-chip-dangerous', 'rgb(255, 0, 0)');
    document.body.appendChild(omnibox);
  });

  test('Setting text without selection', async () => {
    omnibox.omniboxViewState = {
      textPieces: [
        {
          text: 'Hello',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      selection: null,
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('Hello', omnibox.$.textInput.value);

    // Now set to blank
    omnibox.omniboxViewState = {
      textPieces: [],
      selection: null,
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('', omnibox.$.textContainer.textContent);
    assertEquals('', omnibox.$.textInput.value);
  });

  test('Setting text with multiple pieces', async () => {
    omnibox.omniboxViewState = {
      textPieces: [
        {
          text: 'He',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: 'llo',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      selection: null,
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('Hello', omnibox.$.textInput.value);
  });

  test('Text formatting', async () => {
    omnibox.omniboxViewState = {
      textPieces: [
        {
          text: 'A0',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: 'A1',
          strikethrough: true,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: 'B0',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxTextDimmed,
        },
        {
          text: 'B1',
          strikethrough: true,
          color: OmniboxTextColor.kOmniboxTextDimmed,
        },
        {
          text: 'C0',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxForegroundDisabled,
        },
        {
          text: 'C1',
          strikethrough: true,
          color: OmniboxTextColor.kOmniboxForegroundDisabled,
        },
        {
          text: 'D0',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxSecurityChipDangerous,
        },
        {
          text: 'D1',
          strikethrough: true,
          color: OmniboxTextColor.kOmniboxSecurityChipDangerous,
        },
      ],
      selection: null,
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('A0A1B0B1C0C1D0D1', omnibox.$.textContainer.textContent);
    assertEquals('A0A1B0B1C0C1D0D1', omnibox.$.textInput.value);
    const pieces = getTextPieces();
    assertEquals(8, pieces.length);
    checkPiece(pieces[0], 'A0', false, 'rgb(0, 255, 255)');
    checkPiece(pieces[1], 'A1', true, 'rgb(0, 255, 255)');
    checkPiece(pieces[2], 'B0', false, 'rgb(0, 255, 0)');
    checkPiece(pieces[3], 'B1', true, 'rgb(0, 255, 0)');
    checkPiece(pieces[4], 'C0', false, 'rgb(0, 0, 255)');
    checkPiece(pieces[5], 'C1', true, 'rgb(0, 0, 255)');
    checkPiece(pieces[6], 'D0', false, 'rgb(255, 0, 0)');
    checkPiece(pieces[7], 'D1', true, 'rgb(255, 0, 0)');
  });

  test('RTL mode handling', async () => {
    omnibox.style.setProperty('direction', 'rtl');
    omnibox.omniboxViewState = {
      textPieces: [
        {
          text: 'example.com',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: '/articles/1/',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxTextDimmed,
        },
      ],
      selection: null,
      textIsUrl: true,
    };
    await microtasksFinished();
    assertEquals(
        'example.com/articles/1/', omnibox.$.textContainer.textContent);
    assertEquals('example.com/articles/1/', omnibox.$.textInput.value);

    const omniboxBounds = omnibox.getBoundingClientRect();
    const pieces = getTextPieces();
    assertEquals(pieces.length, 2);
    assertTrue(!!pieces[0]);
    const spanBound1 = pieces[0].getBoundingClientRect();
    assertTrue(!!pieces[1]);
    const spanBound2 = pieces[1].getBoundingClientRect();

    // If the URL is rendered as LTR, the two pieces should be right next to
    // each other. (If not, they're nested and one of the slashes ends up in
    // the wrong place).
    assertLE(Math.abs(spanBound1.right - spanBound2.x), 2);

    // And since the box should be RTL, the URL should be vaguely right aligned.
    assertGE(spanBound1.x, omniboxBounds.x + omniboxBounds.width * 0.6);
    assertGE(spanBound2.x, omniboxBounds.x + omniboxBounds.width * 0.6);
  });
});
