// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://commander/app.js';

import {BrowserProxyImpl} from 'chrome://commander/browser_proxy.js';
import {Action, Entity, ViewModel} from 'chrome://commander/types.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertDeepEquals, assertEquals, assertGT} from '../chai_assert.js';
import {flushTasks} from '../test_util.m.js';

import {TestCommanderBrowserProxy} from './test_commander_browser_proxy.js';

suite('CommanderWebUIBrowserTest', () => {
  let app;
  let testProxy;

  /**
   * Creates a basic view model skeleton from the provided data.
   * Populated with action = DISPLAY_RESULTS, and an option per each title
   * in `titles` with entity = COMMAND and `matchedRanges` = [[0, 1]].
   * @param {number} resultSetId The value of `resultSetId` in the view model.
   * @param {!Array<string>} titles A list of option titles.
   * @returns {!ViewModel}
   */
  function createStubViewModel(resultSetId, titles) {
    const options = titles.map(title => ({
                                 title: title,
                                 entity: Entity.COMMAND,
                                 matchedRanges: [[0, 1]],
                               }));
    return {
      resultSetId: resultSetId,
      options: options,
      action: Action.DISPLAY_RESULTS,
    };
  }

  /**
   * Asserts that of the elements in `elements`, only the element at
   * `focusedIndex` has class 'focused'.
   * @param {!NodeList<!HTMLElement>} elements An ordered list of elements.
   * @param {number} focusedIndex The index at which the focused element is
   *     expected to appear.
   */
  function assertFocused(elements, focusedIndex) {
    assertGT(elements.length, 0);
    Array.from(elements).forEach((element, index) => {
      const isFocused = element.classList.contains('focused');
      assertEquals(index === focusedIndex, isFocused);
    });
  }

  setup(async () => {
    testProxy = new TestCommanderBrowserProxy();
    BrowserProxyImpl.instance_ = testProxy;
    document.body.innerHTML = '';
    app = document.createElement('commander-app');
    document.body.appendChild(app);
    await flushTasks();
  });

  test('esc dismisses', () => {
    assertEquals(0, testProxy.getCallCount('dismiss'));
    const input = app.$.input;
    keyDownOn(input, 0, [], 'Escape');

    assertEquals(1, testProxy.getCallCount('dismiss'));
  });

  test('typing sends textChanged', async () => {
    const expectedText = 'orange';
    const input = app.$.input;
    input.value = expectedText;
    input.dispatchEvent(new Event('input'));

    const actualText = await testProxy.whenCalled('textChanged');
    assertEquals(expectedText, actualText);
  });

  test('display results view model change renders options', async () => {
    const titles = ['William of Orange', 'Orangutan', 'Orange Juice'];
    webUIListenerCallback(
        'view-model-updated', createStubViewModel(42, titles));
    await flushTasks();

    const optionElements = app.shadowRoot.querySelectorAll('commander-option');
    assertEquals(titles.length, optionElements.length);

    const actualTitles = Array.from(optionElements).map(el => {
      return Array.from(el.shadowRoot.querySelectorAll('.title-piece'))
          .map(piece => piece.innerText)
          .join('');
    });
    assertDeepEquals(titles, actualTitles);
  });

  test('display results view model change sends heightChanged', async () => {
    webUIListenerCallback('view-model-updated', createStubViewModel(42, [
                            'William of Orange', 'Orangutan', 'Orange Juice'
                          ]));
    await flushTasks();

    const height = await testProxy.whenCalled('heightChanged');
    assertEquals(document.body.offsetHeight, height);
  });

  test('clicking option sends optionSelected', async () => {
    const expectedResultSetId = 42;
    webUIListenerCallback(
        'view-model-updated', createStubViewModel(expectedResultSetId, [
          'William of Orange', 'Orangutan', 'Orange Juice'
        ]));
    await flushTasks();

    const optionElements = app.shadowRoot.querySelectorAll('commander-option');
    optionElements[1].click();
    const [optionIndex, resultID] =
        await testProxy.whenCalled('optionSelected');
    assertEquals(1, optionIndex);
    assertEquals(expectedResultSetId, resultID);
  });

  test('first option selected by default', async () => {
    webUIListenerCallback('view-model-updated', createStubViewModel(42, [
                            'William of Orange', 'Orangutan', 'Orange Juice'
                          ]));
    await flushTasks();

    const optionElements = app.shadowRoot.querySelectorAll('commander-option');
    assertFocused(optionElements, 0);
  });

  test('arrow keys change selection', async () => {
    const input = app.$.input;
    webUIListenerCallback('view-model-updated', createStubViewModel(42, [
                            'William of Orange', 'Orangutan', 'Orange Juice'
                          ]));
    await flushTasks();

    const optionElements = app.shadowRoot.querySelectorAll('commander-option');
    keyDownOn(input, 0, [], 'ArrowDown');
    assertFocused(optionElements, 1);
    keyDownOn(input, 0, [], 'ArrowDown');
    assertFocused(optionElements, 2);
    keyDownOn(input, 0, [], 'ArrowDown');
    assertFocused(optionElements, 0);

    keyDownOn(input, 0, [], 'ArrowUp');
    assertFocused(optionElements, 2);
    keyDownOn(input, 0, [], 'ArrowUp');
    assertFocused(optionElements, 1);
    keyDownOn(input, 0, [], 'ArrowUp');
    assertFocused(optionElements, 0);
  });

  test('return sends optionSelected', async () => {
    const input = app.$.input;
    const expectedResultSetId = 42;
    webUIListenerCallback(
        'view-model-updated', createStubViewModel(expectedResultSetId, [
          'William of Orange', 'Orangutan', 'Orange Juice'
        ]));
    await flushTasks();

    keyDownOn(input, 0, [], 'Enter');
    const [optionIndex, resultID] =
        await testProxy.whenCalled('optionSelected');
    assertEquals(0, optionIndex);
    assertEquals(expectedResultSetId, resultID);
  });

  test('prompt view model draws chip', async () => {
    const expectedPrompt = 'Select fruit';
    webUIListenerCallback(
        'view-model-updated',
        {resultSetId: 42, action: Action.PROMPT, promptText: expectedPrompt});
    await flushTasks();

    const chips = app.shadowRoot.querySelectorAll('.chip');
    assertEquals(1, chips.length);
    assertEquals(expectedPrompt, chips[0].innerText);
  });

  test('backspacing over chip cancels prompt', async () => {
    const expectedPrompt = 'Select fruit';
    webUIListenerCallback(
        'view-model-updated',
        {resultSetId: 42, action: Action.PROMPT, promptText: expectedPrompt});
    await flushTasks();

    const input = app.$.input;
    input.value = 'A';

    // Backspace over text doesn't delete the chip.
    keyDownOn(input, 0, [], 'Backspace');
    assertEquals(0, testProxy.getCallCount('promptCancelled'));

    input.value = '';
    keyDownOn(input, 0, [], 'Backspace');
    assertEquals(1, testProxy.getCallCount('promptCancelled'));
  });
});
