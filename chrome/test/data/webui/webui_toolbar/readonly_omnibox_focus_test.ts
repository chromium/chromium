// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, EventDispositionFlag, INVALID_FOCUS_REQUEST_HANDLE, OmniboxTextColor} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {OmniboxAction} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ReadonlyOmniboxElement} from 'chrome://webui-toolbar.top-chrome/readonly_omnibox.js';

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

// These tests care about focus and selection so can't be parallelized.
// TODO(crbug.com/500653057): Since the <input> now keeps track of selection,
// some of these tests should actually move to the regular test.
suite('ReadOnlyOmniboxFocus', function() {
  let omnibox: ReadonlyOmniboxElement;
  let other: HTMLInputElement;  // A focusable sibling element.
  let uiHandler: MockToolbarUiHandler;

  function getStringSelection(): string {
    const inp = omnibox.$.textInput;
    return inp.value.substring(inp.selectionStart || 0, inp.selectionEnd || 0);
  }

  setup(async () => {
    const browserProxy = new MockBrowserProxy();
    uiHandler = browserProxy.toolbarUIHandler;
    BrowserProxyImpl.setInstance(browserProxy as any);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    omnibox = document.createElement('readonly-omnibox');
    // `other` can be any focusable element; it's here for tests that move
    // the focus around.
    other = document.createElement('input');
    document.body.appendChild(omnibox);
    document.body.appendChild(other);
    omnibox.$.textInput.focus();
    await microtasksFinished();
  });

  test('Setting text with selection', async () => {
    omnibox.omniboxViewState = {
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
      selection: {start: 1, end: 5},
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('Hello', omnibox.$.textInput.value);
    assertEquals('ello', getStringSelection());
    const style = omnibox.$.textContainer.computedStyleMap();
    assertEquals('ellipsis', style.get('text-overflow')?.toString());
  });

  test('Setting multi-piece text with selection', async () => {
    omnibox.omniboxViewState = {
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
      selection: {start: 1, end: 5},
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('Hello', omnibox.$.textInput.value);
    assertEquals('ello', getStringSelection());
  });

  // Focus out and in should not affect selection now (since it's kept track of
  // by the <input> even when not painting it. We also always set long text
  // to ellipsis if too long, since that's just our rich text view and not the
  // <input>.
  test('Selection on focus out and back in', async () => {
    omnibox.omniboxViewState = {
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
      selection: {start: 1, end: 5},
      textIsUrl: false,
    };
    await microtasksFinished();
    assertEquals('ello', getStringSelection());

    other.focus();
    await microtasksFinished();
    let style = omnibox.$.textContainer.computedStyleMap();
    assertEquals('ellipsis', style.get('text-overflow')?.toString());
    assertEquals('ello', getStringSelection());

    omnibox.$.textInput.focus();
    await microtasksFinished();
    style = omnibox.$.textContainer.computedStyleMap();
    assertEquals('ellipsis', style.get('text-overflow')?.toString());
    assertEquals('ello', getStringSelection());
  });

  test('Event forwarding via mojo', async () => {
    omnibox.omniboxViewState = {
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
      selection: {start: 1, end: 5},
      textIsUrl: false,
    };
    await microtasksFinished();

    other.focus();
    uiHandler.reset();

    // Focus us.
    omnibox.$.textInput.focus();
    await microtasksFinished();
    assertEquals(1, uiHandler.getCallCount('onOmniboxAction'));
    let args = uiHandler.getArgs('onOmniboxAction');

    assertTrue(!!args[0].focusChange);
    assertTrue(args[0].focusChange.hasFocus);
    assertEquals(1, args[0].focusChange.selection.start);
    assertEquals(5, args[0].focusChange.selection.end);

    // Synthesize some events. Note that these do not actually affect the
    // value of the input, but they do trigger forwarding.
    const escDown = new KeyboardEvent('keydown', {
      key: 'Escape',
      bubbles: true,
    });
    omnibox.$.textInput.dispatchEvent(escDown);
    await microtasksFinished();
    assertEquals(2, uiHandler.getCallCount('onOmniboxAction'));
    args = uiHandler.getArgs('onOmniboxAction');
    assertTrue(!!args[1].key);
    assertEquals(1, args[1].key.selection.start);
    assertEquals(5, args[1].key.selection.end);
    assertEquals(true, args[1].key.isKeyDown);
    assertEquals('Escape', args[1].key.key);
    assertArrayEquals([], args[1].key.modifiers);

    // Synthetic input will report the current actual state.
    omnibox.$.textInput.value = 'abcdefgh';
    omnibox.$.textInput.setSelectionRange(2, 3, 'backward');
    const inputEvent = new InputEvent('input', {
      data: 'Does not look at it',
      bubbles: true,
    });
    omnibox.$.textInput.dispatchEvent(inputEvent);
    await microtasksFinished();
    assertEquals(3, uiHandler.getCallCount('onOmniboxAction'));
    args = uiHandler.getArgs('onOmniboxAction');
    assertTrue(!!args[2].textInput);
    assertEquals(3, args[2].textInput.selection.start);
    assertEquals(2, args[2].textInput.selection.end);
    assertEquals('abcdefgh', args[2].textInput.text);
    assertEquals('', args[2].textInput.inlineAutocompletion);

    const controlUp = new KeyboardEvent('keyup', {
      key: 'Control',
      bubbles: true,
      shiftKey: true,
    });
    omnibox.$.textInput.dispatchEvent(controlUp);
    await microtasksFinished();
    assertEquals(4, uiHandler.getCallCount('onOmniboxAction'));
    args = uiHandler.getArgs('onOmniboxAction');
    assertTrue(!!args[3].key);
    assertEquals(3, args[3].key.selection.start);
    assertEquals(2, args[3].key.selection.end);
    assertEquals(false, args[3].key.isKeyDown);
    assertEquals('Control', args[3].key.key);
    assertArrayEquals(
        [EventDispositionFlag.kShiftKeyDown], args[3].key.modifiers);

    // Now blur.
    other.focus();
    assertEquals(5, uiHandler.getCallCount('onOmniboxAction'));
    args = uiHandler.getArgs('onOmniboxAction');
    assertTrue(!!args[4].focusChange);
    assertFalse(args[4].focusChange.hasFocus);
    assertEquals(3, args[4].focusChange.selection.start);
    assertEquals(2, args[4].focusChange.selection.end);
  });
});
