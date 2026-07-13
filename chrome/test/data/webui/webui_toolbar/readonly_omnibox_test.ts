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
    super(['onOmniboxAction', 'adjustOmniboxTextForCopy']);
  }

  onOmniboxAction(action: OmniboxAction) {
    this.methodCalled('onOmniboxAction', action);
  }

  adjustOmniboxTextForCopy(text: string, _selectionStart: number) {
    this.methodCalled('adjustOmniboxTextForCopy', text);
    return Promise.resolve({
      adjustedText: text,
      adjustedUrl: null,
    });
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

  const initialState = {
    browserVersion: 0,
    uiVersion: 0,
    textPieces: [],
    inlineAutocompletion: '',
    additionalText: '',
    formattedFullUrl: '',
    selection: null,
    textIsUrl: false,
    userInputInProgress: false,
  };

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
    // Verify that both copies of selected text are in sync; modulo the
    // special case for inline autocompletion.
    const inputSelection =
        inp.value.substring(inp.selectionStart || 0, inp.selectionEnd || 0);
    let start = omnibox.omniboxViewState.selection?.start || 0;
    let end = omnibox.omniboxViewState.selection?.end || 0;
    if (start > end) {
      [start, end] = [end, start];
    }
    if (omnibox.omniboxViewState.inlineAutocompletion.length === 0) {
      const stateSelection = inp.value.substring(start, end);
      assertEquals(stateSelection, inputSelection);
    } else {
      // If we're displaying inline autocompletion as selection, state's
      // selection will be of caret at end of the user input portion, and
      // match the <input> selection's start.
      assertEquals(start, inp.selectionStart);
      assertEquals(end, inp.selectionStart);
    }
    return inputSelection;
  }

  function fakeKeyDown(key: string, shiftDown: boolean = false) {
    const ev = new KeyboardEvent('keydown', {key, shiftKey: shiftDown});
    getTextInput().dispatchEvent(ev);
  }

  // Tests that the bounding boxes of `first` and `second` have the same
  // vertical bounds, and `first` is directly to the left of `second`.
  function assertLinedUp(first: HTMLElement, second: HTMLElement): void {
    const firstBounds = first.getBoundingClientRect();
    const secondBounds = second.getBoundingClientRect();
    assertLE(Math.abs(firstBounds.top - secondBounds.top), 0.1);
    assertLE(Math.abs(firstBounds.bottom - secondBounds.bottom), 0.1);
    assertLE(Math.abs(firstBounds.right - secondBounds.left), 0.1);
  }

  // buttons = 1 by default here, and 0 on up, since that's the current
  // state of the button during the respective event.
  function fakeLeftMouseDown(buttons = 1) {
    const evDown = new MouseEvent('mousedown', {detail: 1, button: 0, buttons});
    getTextInput().dispatchEvent(evDown);
  }

  function fakeLeftMouseUp(buttons = 0) {
    const evUp = new MouseEvent('mouseup', {detail: 1, button: 0, buttons});
    getTextInput().dispatchEvent(evUp);
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
      ...initialState,
      textPieces: [
        {
          text: 'Hello',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      selection: null,
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('Hello', omnibox.$.textInput.value);

    // Now set to blank
    omnibox.browserOmniboxState = Object.assign(initialState);
    await microtasksFinished();
    assertEquals('', omnibox.$.textContainer.textContent);
    assertEquals('', omnibox.$.textInput.value);
  });

  test('Setting text with multiple pieces', async () => {
    omnibox.browserOmniboxState = {
      ...initialState,
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
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('Hello', omnibox.$.textInput.value);
  });

  test('Text formatting', async () => {
    omnibox.browserOmniboxState = {
      ...initialState,
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
      ...initialState,
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

  test('Inline completion', async () => {
    omnibox.browserOmniboxState = {
      ...initialState,
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
    // Version updated with new input.
    assertEquals(1, omnibox.omniboxViewState.uiVersion);

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
    // Likewise version updated on the non-inline character path.
    assertEquals(2, omnibox.omniboxViewState.uiVersion);
  });

  test('Additional text', async () => {
    omnibox.browserOmniboxState = {
      ...initialState,
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
      ...initialState,
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
      ...initialState,
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
      ...initialState,
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

  test(
      'Disregarding browser selection when ui-controlled properly',
      async () => {
        omnibox.browserOmniboxState = {
          ...initialState,
          textPieces: [
            {
              text: 'www.example.',
              strikethrough: false,
              color: OmniboxTextColor.kOmniboxText,
            },
          ],
          selection: {start: 0, end: 3},
          textIsUrl: true,
        };
        await microtasksFinished();

        // Pretend we typed in ftp
        const input = getTextInput();
        input.value = 'ftp.example.';
        input.dispatchEvent(new InputEvent('input'));
        await microtasksFinished();
        assertEquals(1, omnibox.omniboxViewState.uiVersion);

        // and that we changed the caret on mouse down or arrow, etc., which
        // didn't get synced yet.
        input.setSelectionRange(4, 4);

        // Browser sends out-of-dated selection for some reason.
        omnibox.browserOmniboxState = {
          ...initialState,
          textPieces: [
            {
              text: 'ftp.example.',
              strikethrough: false,
              color: OmniboxTextColor.kOmniboxText,
            },
          ],
          selection: {start: 3, end: 3},
          textIsUrl: true,
          uiVersion: 1,
        };
        await microtasksFinished();

        // input's selection isn't restored to wrong value.
        assertEquals(4, input.selectionStart);
      });

  test('Unelision on home', async () => {
    omnibox.browserOmniboxState = {
      ...initialState,
      textPieces: [
        {
          text: 'example.com',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
        {
          text: '/article',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxTextDimmed,
        },
      ],
      inlineAutocompletion: '',
      formattedFullUrl: 'https://example.com/article',
      selection: {start: 1, end: 1},
      textIsUrl: true,
    };
    await microtasksFinished();

    const input = getTextInput();

    assertEquals('example.com/article', input.value);
    // The state set the caret to 1 so we can see home moving the caret.
    assertEquals(1, input.selectionStart!);
    assertEquals(1, input.selectionEnd!);

    // Pressing home moves caret and unelides (both versions).
    fakeKeyDown('Home');
    await microtasksFinished();
    assertEquals('https://example.com/article', input.value);
    assertEquals(
        'https://example.com/article', omnibox.$.textContainer.textContent);
    assertEquals(0, input.selectionStart!);
    assertEquals(0, input.selectionEnd!);
    // It also updated the ui-side version #, since text changed.
    assertEquals(1, omnibox.omniboxViewState.uiVersion);

    // ... Home unelides even if all is selected.
    const testShortUrl = 'example.com';
    omnibox.browserOmniboxState = {
      ...initialState,
      textPieces: [
        {
          text: testShortUrl,
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: '',
      formattedFullUrl: 'https://example.com',
      selection: {start: 0, end: testShortUrl.length},
      textIsUrl: true,
      browserVersion: 1,
    };
    await microtasksFinished();
    assertEquals(testShortUrl, input.value);
    assertEquals(0, input.selectionStart!);
    assertEquals(testShortUrl.length, input.selectionEnd!);
    fakeKeyDown('Home');
    await microtasksFinished();
    assertEquals('https://example.com', input.value);
    assertEquals('https://example.com', omnibox.$.textContainer.textContent);
    assertEquals(0, input.selectionStart!);
    assertEquals(0, input.selectionEnd!);
    assertEquals(1, omnibox.omniboxViewState.uiVersion);

    // Now test shift-home (forward selection dir).
    omnibox.browserOmniboxState = {
      ...initialState,
      textPieces: [
        {
          text: testShortUrl,
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: '',
      formattedFullUrl: 'https://example.com',
      selection: {start: 1, end: 3},
      textIsUrl: true,
      browserVersion: 2,
    };
    await microtasksFinished();
    assertEquals(1, input.selectionStart);
    assertEquals(3, input.selectionEnd);
    assertEquals('forward', input.selectionDirection);

    fakeKeyDown('Home', /*shiftDown=*/ true);
    await microtasksFinished();
    assertEquals('https://e', getStringSelection());
    assertEquals(1, omnibox.omniboxViewState.uiVersion);

    // Now test shift-home (backward selection dir).
    omnibox.browserOmniboxState = {
      ...initialState,
      textPieces: [
        {
          text: testShortUrl,
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: '',
      formattedFullUrl: 'https://example.com',
      selection: {start: 3, end: 1},
      textIsUrl: true,
      browserVersion: 3,
    };
    await microtasksFinished();
    assertEquals(1, input.selectionStart);
    assertEquals(3, input.selectionEnd);
    assertEquals('backward', input.selectionDirection);

    fakeKeyDown('Home', /*shiftDown=*/ true);
    await microtasksFinished();
    assertEquals('https://exa', getStringSelection());
    assertEquals(1, omnibox.omniboxViewState.uiVersion);
  });

  test('Unelision on mouse release', async () => {
    omnibox.browserOmniboxState = {
      ...initialState,
      textPieces: [
        {
          text: 'example.com',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: '',
      formattedFullUrl: 'https://example.com',
      selection: {start: 0, end: 0},
      textIsUrl: true,
    };
    await microtasksFinished();
    const input = getTextInput();
    assertEquals('example.com', input.value);
    assertEquals('example.com', omnibox.$.textContainer.textContent);
    assertEquals('', getStringSelection());

    // Just clicking should not unelide, but should select all.
    fakeLeftMouseDown();
    fakeLeftMouseUp();
    await microtasksFinished();
    assertEquals('example.com', input.value);
    assertEquals('example.com', omnibox.$.textContainer.textContent);
    assertEquals('example.com', getStringSelection());
    assertEquals(1, omnibox.omniboxViewState.uiVersion);

    // Reset the omnibox in between subtests.
    omnibox.browserOmniboxState = {
      ...omnibox.browserOmniboxState,
      browserVersion: 1,
    };
    await microtasksFinished();
    assertEquals('example.com', input.value);
    assertEquals('example.com', omnibox.$.textContainer.textContent);
    assertEquals('', getStringSelection());

    // Simulate drag-select. (We currently don't handle it properly with real
    // events, but this at least exercises the relevant code for updating
    // selection). To get this to happen and not hit the select-all path
    // we can act as if the middle button is down continuously.
    // Here, 5 = middle + left, 4 = middle.
    fakeLeftMouseDown(/*buttons=*/ 5);
    input.setSelectionRange(1, 5, 'forward');
    fakeLeftMouseUp(/*buttons=*/ 4);
    await microtasksFinished();
    assertEquals('https://example.com', input.value);
    assertEquals('https://example.com', omnibox.$.textContainer.textContent);
    // 2 due to user selection + unelide both changing it.
    assertEquals(2, omnibox.omniboxViewState.uiVersion);
    // Selection value is preserved if it's in the middle.
    assertEquals('xamp', getStringSelection());
    assertEquals('forward', input.selectionDirection);

    // Reset the omnibox in between subtests.
    omnibox.browserOmniboxState = {
      ...omnibox.browserOmniboxState,
      browserVersion: 2,
    };
    await microtasksFinished();

    // Drag-select to beginning. Should also include https:// in the selection
    // after unelision, and preserve the direction.
    fakeLeftMouseDown(/*buttons=*/ 5);
    input.setSelectionRange(0, 5, 'backward');
    fakeLeftMouseUp(/*buttons=*/ 4);
    await microtasksFinished();
    assertEquals('https://example.com', input.value);
    assertEquals('https://example.com', omnibox.$.textContainer.textContent);
    // Selection value is preserved if it's in the middle.
    assertEquals('https://examp', getStringSelection());
    assertEquals('backward', input.selectionDirection);
    // 2 due to user selection + unelide both changing it.
    assertEquals(2, omnibox.omniboxViewState.uiVersion);
  });

  test('Unelision of intranet domains', async () => {
    omnibox.browserOmniboxState = {
      ...initialState,
      textPieces: [
        {
          text: 'example/',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: '',
      formattedFullUrl: 'https://example',
      selection: {start: 0, end: 0},
      textIsUrl: true,
    };
    await microtasksFinished();
    const input = getTextInput();
    assertEquals('example/', input.value);
    assertEquals('example/', omnibox.$.textContainer.textContent);

    // Simulate drag-select.
    fakeLeftMouseDown(/*buttons=*/ 5);
    input.setSelectionRange(1, 5);
    fakeLeftMouseUp(/*buttons=*/ 4);
    await microtasksFinished();
    assertEquals('https://example', input.value);
    assertEquals('https://example', omnibox.$.textContainer.textContent);
    // Selection value is preserved if it's in the middle.
    assertEquals('xamp', getStringSelection());
  });

  test('Unelision of https://https', async () => {
    omnibox.browserOmniboxState = {
      ...initialState,
      textPieces: [
        {
          text: 'https',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: '',
      formattedFullUrl: 'https://https',
      selection: {start: 0, end: 0},
      textIsUrl: true,
    };
    await microtasksFinished();
    const input = getTextInput();
    assertEquals('https', input.value);
    assertEquals('https', omnibox.$.textContainer.textContent);

    // Simulate drag-select.
    fakeLeftMouseDown(/*buttons=*/ 5);
    input.setSelectionRange(1, 5);
    fakeLeftMouseUp(/*buttons=*/ 4);
    await microtasksFinished();
    assertEquals('https://https', input.value);
    assertEquals('https://https', omnibox.$.textContainer.textContent);
    // Selection value is preserved if it's in the middle.
    assertEquals('ttps', getStringSelection());
    // And it's selecting the second copy, not the first one.
    assertEquals(9, input.selectionStart);
    assertEquals(13, input.selectionEnd);

    // Reset, and repeat with trailing /
    omnibox.browserOmniboxState = {
      ...initialState,
      textPieces: [
        {
          text: 'https/',
          strikethrough: false,
          color: OmniboxTextColor.kOmniboxText,
        },
      ],
      inlineAutocompletion: '',
      formattedFullUrl: 'https://https',
      selection: {start: 0, end: 0},
      textIsUrl: true,
      browserVersion: 1,
    };
    await microtasksFinished();
    assertEquals('https/', input.value);
    assertEquals('https/', omnibox.$.textContainer.textContent);

    // Simulate drag-select, including the trailing /
    fakeLeftMouseDown(/*buttons=*/ 5);
    input.setSelectionRange(1, 6);
    fakeLeftMouseUp(/*buttons=*/ 4);
    await microtasksFinished();
    assertEquals('https://https', input.value);
    assertEquals('https://https', omnibox.$.textContainer.textContent);
    // Selection value is preserved (sans the trailing /).
    assertEquals('ttps', getStringSelection());
    // And it's selecting the second copy, not the first one.
    assertEquals(9, input.selectionStart);
    assertEquals(13, input.selectionEnd);
  });

});
