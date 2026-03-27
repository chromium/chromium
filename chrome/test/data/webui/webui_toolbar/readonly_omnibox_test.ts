// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertGE, assertLE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {OmniboxTextColor, ReadonlyOmniboxElement} from 'chrome://webui-toolbar.top-chrome/app.js';

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

    // Now set to blank
    omnibox.omniboxViewState = {
      textPieces: [],
      selection: null,
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('', omnibox.$.textContainer.textContent);
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

    // Also exercise the textChildren() helper a bit.
    const textNodes: Text[] = omnibox.textChildren();
    assertEquals(2, textNodes.length);
    assertTrue(!!textNodes[0]);
    assertEquals('He', textNodes[0].nodeValue);
    assertTrue(!!textNodes[1]);
    assertEquals('llo', textNodes[1].nodeValue);
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

  test('Range conversion helpers', () => {
    const div = document.createElement('div');
    const comments: Comment[] = [];
    const text: Text[] = [];
    for (let i = 0; i < 4; ++i) {
      const c = document.createComment('c' + i);
      comments.push(c);
      div.appendChild(c);
      const t = document.createTextNode('t' + i);
      div.appendChild(t);
      text.push(t);
    }
    const c = document.createComment('c4');
    comments.push(c);
    div.appendChild(c);

    // Nodes:  c0  t0  c1  t1  c2  t2  c3  t3  c4
    // Text:      "t0"    "t1"    "t2"    "t3"
    // Offsets:    01      23      45      67

    const fullText = 't0t1t2t3';
    assertEquals(fullText, div.textContent);

    {
      // Select everything inside the div, using it rather than child nodes
      // for specifying start and end.
      const r1 = document.createRange();
      r1.setStart(div, 0);
      r1.setEnd(div, div.childNodes.length);
      assertEquals(fullText, r1.toString());
      const b1 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetStart(text, r1);
      const e1 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetEnd(text, r1);
      assertEquals(0, b1);
      assertEquals(8, e1);
    }

    {
      // Selection starts in the middle of a comment node, and ends in the
      // middle of a text node.
      const r2 = document.createRange();
      r2.setStart(comments[0]!, 1);
      r2.setEnd(text[3]!, 1);
      assertEquals('t0t1t2t', r2.toString());
      const b2 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetStart(text, r2);
      const e2 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetEnd(text, r2);
      assertEquals(0, b2);
      assertEquals(7, e2);
    }

    {
      // Selection starts at the end of a text node, and ends at beginning of
      // a comment node.
      const r3 = document.createRange();
      r3.setStart(text[1]!, 2);
      r3.setEnd(comments[4]!, 0);
      assertEquals('t2t3', r3.toString());
      const b3 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetStart(text, r3);
      const e3 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetEnd(text, r3);
      assertEquals(4, b3);
      assertEquals(8, e3);
    }

    {
      // Selection is a portion of a text node.
      const r4 = document.createRange();
      r4.setStart(text[1]!, 1);
      r4.setEnd(text[1]!, 2);
      assertEquals('1', r4.toString());
      const b4 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetStart(text, r4);
      const e4 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetEnd(text, r4);
      assertEquals(3, b4);
      assertEquals(4, e4);
    }

    {
      // Caret at end of a text node.
      const r5 = document.createRange();
      r5.setStart(text[2]!, 2);
      r5.setEnd(text[2]!, 2);
      assertEquals('', r5.toString());
      const b5 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetStart(text, r5);
      const e5 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetEnd(text, r5);
      assertEquals(6, b5);
      assertEquals(6, e5);
    }

    {
      // Selection inside comment node; we interpret it as caret between the
      // two surrounding pieces of text.
      const r6 = document.createRange();
      r6.setStart(comments[2]!, 1);
      r6.setEnd(comments[2]!, 2);
      assertEquals('', r6.toString());
      const b6 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetStart(text, r6);
      const e6 = ReadonlyOmniboxElement.nodeRelToGlobalOffsetEnd(text, r6);
      assertEquals(4, b6);
      assertEquals(4, e6);
    }
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
