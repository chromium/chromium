// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://commander/app.js';

import {CommanderAppElement} from 'chrome://commander/app.js';
import {BrowserProxyImpl} from 'chrome://commander/browser_proxy.js';
import {Action, Entity, ViewModel} from 'chrome://commander/types.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestCommanderBrowserProxy} from './test_commander_browser_proxy.js';

suite('CommanderWebUIBrowserTest', () => {
  let app: CommanderAppElement;
  let testProxy: TestCommanderBrowserProxy;

  /**
   * Creates a basic view model skeleton from the provided data.
   * Populated with action = DISPLAY_RESULTS, and an option per each title
   * in `titles` with entity = COMMAND and `matchedRanges` = [[0, 1]].
   * @param resultSetId The value of `resultSetId` in the view model.
   * @param titles A list of option titles.
   */
  function createStubViewModel(
      resultSetId: number, titles: string[]): ViewModel {
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
   * @param elements An ordered list of elements.
   * @param focusedIndex The index at which the focused element is
   *     expected to appear.
   */
  function assertFocused(
      elements: NodeListOf<HTMLElement>, focusedIndex: number) {
    assertGT(elements.length, 0);
    Array.from(elements).forEach((element, index) => {
      const isFocused = element.classList.contains('focused');
      const isAriaSelected =
          element.getAttribute('aria-selected') === 'true' ? true : false;
      assertEquals(index === focusedIndex, isFocused);
      assertEquals(isFocused, isAriaSelected);
    });
  }

  setup(async () => {
    testProxy = new TestCommanderBrowserProxy();
    BrowserProxyImpl.setInstance(testProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

    const optionElements = app.shadowRoot!.querySelectorAll('commander-option');
    assertEquals(titles.length, optionElements.length);

    const actualTitles = Array.from(optionElements).map(el => {
      return Array
          .from(el.shadowRoot!.querySelectorAll<HTMLElement>('.title-piece'))
          .map(piece => piece.innerText)
          .join('');
    });
    assertDeepEquals(titles, actualTitles);
  });

  test('display results view model change sends heightChanged', async () => {
    testProxy.resetResolver('heightChanged');
    webUIListenerCallback('view-model-updated', createStubViewModel(42, [
                            'William of Orange',
                            'Orangutan',
                            'Orange Juice',
                          ]));
    await flushTasks();
    const height = await testProxy.whenCalled('heightChanged');
    assertEquals(document.body.offsetHeight, height);
  });

  test('clicking option sends optionSelected', async () => {
    const expectedResultSetId = 42;
    webUIListenerCallback(
        'view-model-updated', createStubViewModel(expectedResultSetId, [
          'William of Orange',
          'Orangutan',
          'Orange Juice',
        ]));
    await flushTasks();

    const optionElements = app.shadowRoot!.querySelectorAll('commander-option');
    assertTrue(!!optionElements[1]);
    optionElements[1].click();
    const [optionIndex, resultID] =
        await testProxy.whenCalled('optionSelected');
    assertEquals(1, optionIndex);
    assertEquals(expectedResultSetId, resultID);
  });

  test('first option selected by default', async () => {
    webUIListenerCallback('view-model-updated', createStubViewModel(42, [
                            'William of Orange',
                            'Orangutan',
                            'Orange Juice',
                          ]));
    await flushTasks();

    const optionElements = app.shadowRoot!.querySelectorAll('commander-option');
    assertFocused(optionElements, 0);
  });

  test('no results view shown if no results', async () => {
    assertEquals(null, app.shadowRoot!.querySelector('#noResults'));
    app.$.input.value = 'A';
    webUIListenerCallback('view-model-updated', createStubViewModel(42, []));
    await flushTasks();

    assertEquals(
        0, app.shadowRoot!.querySelectorAll('commander-option').length);
    assertNotEquals(null, app.shadowRoot!.querySelector('#noResults'));
  });

  test('no results view not shown for empty input', async () => {
    assertEquals(null, app.shadowRoot!.querySelector('#noResults'));
    webUIListenerCallback('view-model-updated', createStubViewModel(42, []));
    await flushTasks();

    assertEquals('', app.$.input.value);
    assertEquals(
        0, app.shadowRoot!.querySelectorAll('commander-option').length);
    assertEquals(null, app.shadowRoot!.querySelector('#noResults'));
  });

  test('arrow keys change selection', async () => {
    const input = app.$.input;
    webUIListenerCallback('view-model-updated', createStubViewModel(42, [
                            'William of Orange',
                            'Orangutan',
                            'Orange Juice',
                          ]));
    await flushTasks();

    const optionElements = app.shadowRoot!.querySelectorAll('commander-option');
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
          'William of Orange',
          'Orangutan',
          'Orange Juice',
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

    const chips = app.shadowRoot!.querySelectorAll<HTMLElement>('.chip');
    assertEquals(1, chips.length);
    assertEquals(expectedPrompt, chips[0]!.innerText);
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

  test('focusing options updates aria-activedescendant', async () => {
    const input = app.$.input;
    const inputRow = app.$.inputRow;
    assertEquals(null, inputRow.getAttribute('aria-selected'));

    webUIListenerCallback('view-model-updated', createStubViewModel(42, [
                            'William of Orange',
                            'Orangutan',
                            'Orange Juice',
                          ]));
    await flushTasks();

    const optionElements = app.shadowRoot!.querySelectorAll('commander-option');
    assertEquals(3, optionElements.length);
    assertEquals(
        optionElements[0]!.id, inputRow.getAttribute('aria-activedescendant'));
    keyDownOn(input, 0, [], 'ArrowDown');
    assertEquals(
        optionElements[1]!.id, inputRow.getAttribute('aria-activedescendant'));
  });
});
