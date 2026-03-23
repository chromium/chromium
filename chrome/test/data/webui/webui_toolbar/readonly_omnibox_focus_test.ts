// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {type ReadonlyOmniboxElement} from 'chrome://webui-toolbar.top-chrome/readonly_omnibox.js';

// These tests care about focus and selection so can't be parallelized.
suite('ReadOnlyOmniboxFocus', function() {
  let omnibox: ReadonlyOmniboxElement;
  let other: HTMLInputElement;  // A focusable sibling element.

  function getStringSelection(): string {
    const selection = document.getSelection();
    assertTrue(!!selection);
    return selection.toString();
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    omnibox = document.createElement('readonly-omnibox');
    // `other` can be any focusable element; it's here for tests that move
    // the focus around.
    other = document.createElement('input');
    document.body.appendChild(omnibox);
    document.body.appendChild(other);
    omnibox.$.textContainer.focus();
    await microtasksFinished();
  });

  test('Setting text with selection', async () => {
    omnibox.omniboxViewState = {
      text: 'Hello',
      selection: {start: 1, end: 5},
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
    assertEquals('ello', getStringSelection());
    const style = omnibox.$.textContainer.computedStyleMap();
    assertEquals('clip', style.get('text-overflow')?.toString());
  });

  // Focus out should clear our selection, set line to use ellipsis, then
  // going back should restore it and show everything.
  test('Focus out and back in', async () => {
    omnibox.omniboxViewState = {
      text: 'Hello',
      selection: {start: 1, end: 5},
    };
    await microtasksFinished();

    other.focus();
    await microtasksFinished();
    let style = omnibox.$.textContainer.computedStyleMap();
    assertEquals('ellipsis', style.get('text-overflow')?.toString());
    assertEquals('', getStringSelection());

    omnibox.$.textContainer.focus();
    await microtasksFinished();
    style = omnibox.$.textContainer.computedStyleMap();
    assertEquals('clip', style.get('text-overflow')?.toString());
    assertEquals('ello', getStringSelection());
  });

  test('Focus out and back in w/user-blanked selection', async () => {
    omnibox.omniboxViewState = {
      text: 'Hello',
      selection: {start: 1, end: 5},
    };
    await microtasksFinished();
    const selection = document.getSelection();
    assertTrue(!!selection);
    selection.removeAllRanges();
    await microtasksFinished();

    other.focus();
    await microtasksFinished();
    assertEquals('', getStringSelection());

    omnibox.$.textContainer.focus();
    await microtasksFinished();
    assertEquals('', getStringSelection());
  });

  test('Focus out and back in w/custom user selection', async () => {
    omnibox.omniboxViewState = {
      text: 'Hello',
      selection: {start: 1, end: 5},
    };
    await microtasksFinished();
    const selection = document.getSelection();
    assertTrue(!!selection);
    selection.removeAllRanges();
    selection.selectAllChildren(omnibox.$.textContainer);
    await microtasksFinished();
    assertEquals('Hello', getStringSelection());

    other.focus();
    await microtasksFinished();
    assertEquals('', getStringSelection());

    omnibox.$.textContainer.focus();
    await microtasksFinished();
    // TODO(crbug.com/474060468): This is wrong (it confused offsets in parent
    // to those of text node), but next CL reworks some relevant code quite a
    // bit, so no point trying to fix it now.
    assertEquals('He', getStringSelection());
  });
});
