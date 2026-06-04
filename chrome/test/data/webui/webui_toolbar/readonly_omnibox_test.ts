// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertGE, assertLE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, INVALID_FOCUS_REQUEST_HANDLE, OmniboxTextColor} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {OmniboxAction, ReadonlyOmniboxElement} from 'chrome://webui-toolbar.top-chrome/app.js';

class MockToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super(['onOmniboxAction']);
  }

  onOmniboxAction(action: OmniboxAction) {
    this.methodCalled('onOmniboxAction', action);
  }
}

class MockBrowserProxy extends TestBrowserProxy {
  toolbarUIHandler: MockToolbarUiHandler = new MockToolbarUiHandler();

  addFocusRequestListener() {
    return INVALID_FOCUS_REQUEST_HANDLE;
  }

  removeFocusRequestListener() {}
}

suite('ReadonlyOmnibox', function() {
  let omnibox: ReadonlyOmniboxElement;
  let uiHandler: MockToolbarUiHandler;

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

  function getTextInput(): HTMLInputElement {
    return omnibox.$.textInput;
  }

  function getStringSelection(): string {
    const inp = getTextInput();
    return inp.value.substring(inp.selectionStart || 0, inp.selectionEnd || 0);
  }

  function fakeKeyDown(key: string) {
    const ev = new KeyboardEvent('keydown', {key});
    getTextInput().dispatchEvent(ev);
  }

  // Tests that the bounding boxes of `first` and `second` have the same
  // vertical bounds, and `first` is directly to the left of `second`.
  function assertLinedUp(first: HTMLElement, second: HTMLElement): void {
    const firstBounds = first.getBoundingClientRect();
    const secondBounds = second.getBoundingClientRect();
    assertEquals(firstBounds.top, secondBounds.top);
    assertEquals(firstBounds.bottom, secondBounds.bottom);
    assertEquals(firstBounds.right, secondBounds.left);
  }

  setup(() => {
    const browserProxy = new MockBrowserProxy();
    uiHandler = browserProxy.toolbarUIHandler;
    BrowserProxyImpl.setInstance(browserProxy as any);

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
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 0,
      textPieces: [
        {
          text: 'Hello',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: '',
      additionalText: '',
      selection: null,
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('Hello', omnibox.$.textInput.value);

    // Now set to blank
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 0,
      textPieces: [],
      inlineAutocompletion: '',
      additionalText: '',
      selection: null,
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('', omnibox.$.textContainer.textContent);
    assertEquals('', omnibox.$.textInput.value);
  });

  test('Setting text with multiple pieces', async () => {
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 0,
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
      inlineAutocompletion: '',
      additionalText: '',
      selection: null,
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('Hello', omnibox.$.textInput.value);
  });

  test('Text formatting', async () => {
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 0,
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
      inlineAutocompletion: '',
      additionalText: '',
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
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 0,
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
      inlineAutocompletion: '',
      additionalText: '',
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

  test('Inline completion', async () => {
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 0,
      textPieces: [
        {
          text: 'example.com',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: '/artic',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxTextDimmed,
        },
      ],
      inlineAutocompletion: 'les/1/',
      additionalText: '',
      selection: {start: 1, end: 2},
      textIsUrl: true,
    };
    await microtasksFinished();

    // The inline autocompletion gets rendered as selected text in the input,
    // overriding the selection field.
    assertEquals('example.com/artic', omnibox.$.textContainer.textContent);
    assertEquals('example.com/articles/1/', omnibox.$.textInput.value);
    assertEquals('les/1/', getStringSelection());

    // Typing 'l' should accept one character from inline completion, and
    // send a textInput event over mojo.
    fakeKeyDown('l');
    await microtasksFinished();
    assertEquals(1, uiHandler.getCallCount('onOmniboxAction'));
    let args = uiHandler.getArgs('onOmniboxAction');
    assertTrue(!!args[0].textInput);
    const kExpectedInput1 = 'example.com/articl';
    assertEquals(kExpectedInput1, args[0].textInput.text);
    assertEquals(kExpectedInput1.length, args[0].textInput.selection.start);
    assertEquals(kExpectedInput1.length, args[0].textInput.selection.end);
    assertEquals('es/1/', args[0].textInput.inlineAutocompletion);

    // The <input> got its selection shifted, and the readonly view got updated
    // with new character.
    assertEquals('example.com/articl', omnibox.$.textContainer.textContent);
    assertEquals('example.com/articles/1/', omnibox.$.textInput.value);
    assertEquals('es/1/', getStringSelection());

    // Now a mismatching one should be handled as any key press.
    // (And as it's not something the browser end cares about, it doesn't
    // get sent over mojo).
    fakeKeyDown('o');
    await microtasksFinished();
    assertEquals(1, uiHandler.getCallCount('onOmniboxAction'));

    // Since it's a fake key, the <input> doesn't actually update, so we
    // have to simulate it.
    const input = getTextInput();
    input.value = kExpectedInput1 + 'o';
    input.selectionStart = kExpectedInput1.length + 1;
    input.selectionEnd = kExpectedInput1.length + 1;
    input.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();
    assertEquals(2, uiHandler.getCallCount('onOmniboxAction'));
    args = uiHandler.getArgs('onOmniboxAction');
    assertTrue(!!args[1].textInput);
    assertEquals(kExpectedInput1 + 'o', args[1].textInput.text);
    assertEquals(kExpectedInput1.length + 1, args[1].textInput.selection.start);
    assertEquals(kExpectedInput1.length + 1, args[1].textInput.selection.end);
    assertEquals('', args[1].textInput.inlineAutocompletion);
    assertEquals('example.com/articlo', omnibox.$.textContainer.textContent);
    assertEquals('example.com/articlo', omnibox.$.textInput.value);
    assertEquals('', getStringSelection());
  });

  test('Additional text', async () => {
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 0,
      textPieces: [
        {
          text: 'popula',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: 'r page',
      additionalText: ' - uk.wikipedia.org',
      selection: {start: 0, end: 0},
      textIsUrl: true,
    };
    await microtasksFinished();

    // Inline autocompletion rendered as selection.
    assertEquals('popula', omnibox.$.textContainer.textContent);
    assertEquals('popular page', omnibox.$.textInput.value);
    assertEquals('r page', getStringSelection());
    // And there is also a hidden box for inline completion (with the visible
    // portion of the completion coming from the <input>), and a visible
    // one for additional text.
    assertEquals('r page', omnibox.$.inlineAutocomplete.textContent);
    assertEquals(' - uk.wikipedia.org', omnibox.$.additionalText.textContent);

    const inlineAutocompleteStyle =
        omnibox.$.inlineAutocomplete.computedStyleMap();
    assertEquals(
        'hidden', inlineAutocompleteStyle.get('visibility')?.toString());

    // Check that our 3 boxes are all lined up. This really wants to check
    // against what's inside the <input>, but that doesn't seem possible.
    assertLinedUp(omnibox.$.textContainer, omnibox.$.inlineAutocomplete);
    assertLinedUp(omnibox.$.inlineAutocomplete, omnibox.$.additionalText);

    const right1 = omnibox.$.additionalText.getBoundingClientRect().right;

    // Advance completion and make sure stuff is still reasonable.
    fakeKeyDown('r');
    await microtasksFinished();
    assertEquals('popular', omnibox.$.textContainer.textContent);
    assertEquals('popular page', omnibox.$.textInput.value);
    assertEquals(' page', getStringSelection());
    assertEquals(' page', omnibox.$.inlineAutocomplete.textContent);
    assertEquals(' - uk.wikipedia.org', omnibox.$.additionalText.textContent);
    assertLinedUp(omnibox.$.textContainer, omnibox.$.inlineAutocomplete);
    assertLinedUp(omnibox.$.inlineAutocomplete, omnibox.$.additionalText);
    const right2 = omnibox.$.additionalText.getBoundingClientRect().right;

    // And the space.
    fakeKeyDown(' ');
    await microtasksFinished();
    assertEquals('popular ', omnibox.$.textContainer.textContent);
    assertEquals('popular page', omnibox.$.textInput.value);
    assertEquals('page', getStringSelection());
    assertEquals('page', omnibox.$.inlineAutocomplete.textContent);
    assertEquals(' - uk.wikipedia.org', omnibox.$.additionalText.textContent);
    assertLinedUp(omnibox.$.textContainer, omnibox.$.inlineAutocomplete);
    assertLinedUp(omnibox.$.inlineAutocomplete, omnibox.$.additionalText);
    const right3 = omnibox.$.additionalText.getBoundingClientRect().right;

    // If we didn't screw up the whitespace, the right edge of the box
    // should be basically the same. (It seemed exactly the same when
    // writing this test).
    assertLE(Math.abs(right1 - right2), 0.1);
    assertLE(Math.abs(right2 - right3), 0.1);
  });

  test('Inline completion race vs. browser handling', async () => {
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 0,
      textPieces: [
        {
          text: 'example.com',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: '/artic',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxTextDimmed,
        },
      ],
      inlineAutocompletion: 'les/1/',
      additionalText: '',
      selection: {start: 1, end: 2},
      textIsUrl: true,
    };
    await microtasksFinished();

    // The inline autocompletion gets rendered as selected text in the input,
    // overriding the selection field.
    assertEquals('example.com/artic', omnibox.$.textContainer.textContent);
    assertEquals('example.com/articles/1/', omnibox.$.textInput.value);
    assertEquals('les/1/', getStringSelection());

    // Typing 'l' should accept one character from inline completion, and
    // send a textInput event over mojo.
    fakeKeyDown('l');
    await microtasksFinished();
    assertEquals(1, uiHandler.getCallCount('onOmniboxAction'));
    {
      const lastArgs = uiHandler.getArgs('onOmniboxAction').at(-1);
      assertTrue(!!lastArgs.textInput);
      const kExpectedInput1 = 'example.com/articl';
      assertEquals(kExpectedInput1, lastArgs.textInput.text);
      assertEquals(kExpectedInput1.length, lastArgs.textInput.selection.start);
      assertEquals(kExpectedInput1.length, lastArgs.textInput.selection.end);
      assertEquals(0, lastArgs.textInput.browserVersion);
      assertEquals(1, lastArgs.textInput.uiVersion);
      assertEquals('es/1/', lastArgs.textInput.inlineAutocompletion);

      // And input is updated appropriately.
      assertEquals('example.com/articl', omnibox.$.textContainer.textContent);
      assertEquals('example.com/articles/1/', omnibox.$.textInput.value);
      assertEquals('es/1/', getStringSelection());
    }

    // Similarly simulate 'e'.
    fakeKeyDown('e');
    await microtasksFinished();
    assertEquals(2, uiHandler.getCallCount('onOmniboxAction'));
    {
      const lastArgs = uiHandler.getArgs('onOmniboxAction').at(-1);
      assertTrue(!!lastArgs.textInput);
      const kExpectedInput2 = 'example.com/article';
      assertEquals(kExpectedInput2, lastArgs.textInput.text);
      assertEquals(kExpectedInput2.length, lastArgs.textInput.selection.start);
      assertEquals(kExpectedInput2.length, lastArgs.textInput.selection.end);
      assertEquals(0, lastArgs.textInput.browserVersion);
      assertEquals(2, lastArgs.textInput.uiVersion);
      assertEquals('s/1/', lastArgs.textInput.inlineAutocompletion);
      assertEquals('example.com/article', omnibox.$.textContainer.textContent);
      assertEquals('example.com/articles/1/', omnibox.$.textInput.value);
      assertEquals('s/1/', getStringSelection());
    }

    // Now an update comes from the browser that's after the 'l'. It should
    // get ignored.
    omnibox.browserOmniboxState = {
      browserVersion: 0,
      uiVersion: 1,
      textPieces: [
        {
          text: 'example.com',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: '/articl',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxTextDimmed,
        },
      ],
      inlineAutocompletion: 'es/1/',
      additionalText: '',
      selection: {start: 1, end: 2},
      textIsUrl: true,
    };
    await microtasksFinished();

    assertEquals('example.com/article', omnibox.$.textContainer.textContent);
    assertEquals('example.com/articles/1/', omnibox.$.textInput.value);
    assertEquals('s/1/', getStringSelection());

    // If something totally different gets loaded, however, it should get
    // honored due to bumped browserVersion.
    omnibox.browserOmniboxState = {
      browserVersion: 1,
      uiVersion: 0,
      textPieces: [
        {
          text: 'example.org',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: '/ess',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxTextDimmed,
        },
      ],
      inlineAutocompletion: 'ay',
      additionalText: '',
      selection: {start: 1, end: 2},
      textIsUrl: true,
    };
    await microtasksFinished();

    assertEquals('example.org/ess', omnibox.$.textContainer.textContent);
    assertEquals('example.org/essay', omnibox.$.textInput.value);
    assertEquals('ay', getStringSelection());

    // Similate a. This should advance completion and send updates with
    // new browserVersion numbers.
    fakeKeyDown('a');
    await microtasksFinished();
    assertEquals(3, uiHandler.getCallCount('onOmniboxAction'));
    {
      const lastArgs = uiHandler.getArgs('onOmniboxAction').at(-1);
      assertTrue(!!lastArgs.textInput);
      const kExpectedInput3 = 'example.org/essa';
      assertEquals(kExpectedInput3, lastArgs.textInput.text);
      assertEquals(kExpectedInput3.length, lastArgs.textInput.selection.start);
      assertEquals(kExpectedInput3.length, lastArgs.textInput.selection.end);
      assertEquals(1, lastArgs.textInput.browserVersion);
      assertEquals(1, lastArgs.textInput.uiVersion);
      assertEquals('y', lastArgs.textInput.inlineAutocompletion);
      assertEquals('example.org/essa', omnibox.$.textContainer.textContent);
      assertEquals('example.org/essay', omnibox.$.textInput.value);
      assertEquals('y', getStringSelection());
    }
  });
});
