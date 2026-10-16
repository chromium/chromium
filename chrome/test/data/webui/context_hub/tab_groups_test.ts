// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/tab_groups/tab_groups.js';

import {browserProxyFactory, ChatRole, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {TabGroup, TabInfo} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {TabGroupsElement} from 'chrome://context-hub/tab_groups/tab_groups.js';
import {GROUPING_FAILED_MESSAGE, NOT_ENOUGH_TABS_MESSAGE} from 'chrome://context-hub/tab_groups/tab_groups.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createTab(id: number, title: string): TabInfo {
  return {id: BigInt(id), title, url: `https://example.com/${id}`};
}

function createGroup(label: string, ...tabs: TabInfo[]): TabGroup {
  return {label, tabs, savedGuid: null};
}

const TAB_1 = createTab(1, 'Tab 1');
const TAB_2 = createTab(2, 'Tab 2');
const TAB_3 = createTab(3, 'Tab 3');
const TAB_4 = createTab(4, 'Tab 4');

function groupingResult(groups: TabGroup[], ungroupedTabs: TabInfo[] = []) {
  return {groups, ungroupedTabs, llmResponse: null};
}

suite('TabGroups', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let view: TabGroupsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues(
        {kAutoTabGroups: true, kMaxTabGroupChatHistoryTurns: 20});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);

    handler.setResultFor(
        'getExistingTabGroupsAndChats',
        Promise.resolve({groups: [], ungroupedTabs: [], history: []}));
    handler.setResultFor(
        'getConfirmedTabGroups', Promise.resolve({groups: []}));
    handler.setResultFor(
        'getTabs', Promise.resolve({tabs: [TAB_1, TAB_2, TAB_3]}));
    handler.setResultFor('replaceTabGroups', Promise.resolve());
    handler.setResultFor('clearTabGroups', Promise.resolve());
    handler.setResultFor('clearTabGroupChatHistory', Promise.resolve());
    handler.setResultFor(
        'confirmAllTabGroups', Promise.resolve({success: true}));
  });

  async function createView() {
    view = document.createElement('tab-groups');
    document.body.appendChild(view);
    await microtasksFinished();
  }

  function $<E extends HTMLElement = HTMLElement>(selector: string): E|null {
    return view.shadowRoot.querySelector<E>(selector);
  }

  function $all(selector: string): HTMLElement[] {
    return Array.from(view.shadowRoot.querySelectorAll<HTMLElement>(selector));
  }

  function groupLabels(): string[] {
    return $all('.group-label').map(el => el.textContent.trim());
  }

  function primaryButton(): CrButtonElement {
    return $<CrButtonElement>('#primary-button')!;
  }

  async function click(selector: string) {
    $(selector)!.click();
    await microtasksFinished();
  }

  // Lands on the suggestions view with TAB_1/TAB_2 grouped and TAB_3 left
  // ungrouped.
  async function checkNow() {
    handler.setResultFor(
        'retrieveAndGroupTabs',
        Promise.resolve(
            groupingResult([createGroup('Work', TAB_1, TAB_2)], [TAB_3])));
    await createView();
    await click('#check-now-button');
  }

  async function typePrompt(text: string) {
    const input = $<CrTextareaElement>('#change-input')!;
    input.value = text;
    input.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: text}}));
    await microtasksFinished();
  }

  test('landing card requests the default grouping', async () => {
    const resolver = new PromiseResolver<ReturnType<typeof groupingResult>>();
    handler.setResultFor('retrieveAndGroupTabs', resolver.promise);
    await createView();

    assertTrue(!!$('#landing-card'));
    assertFalse(!!$('#prompt-card'));
    assertDeepEquals(
        ['Tab 1', 'Tab 2', 'Tab 3'],
        $all('#landing-card .tab-title').map(el => el.textContent.trim()));

    await click('#check-now-button');
    assertEquals('', handler.getArgs('retrieveAndGroupTabs')[0]);
    assertTrue(!!$('.skeleton-card'));
    assertFalse(!!$('#landing-card'));

    resolver.resolve(groupingResult([createGroup('Work', TAB_1, TAB_2)]));
    await microtasksFinished();
    assertFalse(!!$('.skeleton-card'));
    assertDeepEquals(['Work'], groupLabels());
    assertEquals(
        '1 tab groups suggested', $('.card-title')!.textContent.trim());
    assertEquals('Create groups', primaryButton().textContent.trim());
    assertTrue($<CrButtonElement>('#undo-button')!.disabled);
  });

  test('landing is disabled when the feature is off', async () => {
    loadTimeData.overrideValues({kAutoTabGroups: false});
    await createView();
    assertTrue($<CrButtonElement>('#check-now-button')!.disabled);
    assertEquals(0, handler.getCallCount('getExistingTabGroupsAndChats'));
  });

  test('shows status when grouping yields nothing', async () => {
    handler.setResultFor(
        'retrieveAndGroupTabs', Promise.resolve(groupingResult([], [TAB_1])));
    await createView();
    await click('#check-now-button');
    assertEquals(NOT_ENOUGH_TABS_MESSAGE, $('.status')!.textContent.trim());
    assertTrue(!!$('#landing-card'));
  });

  test('restores stored suggestions on load', async () => {
    handler.setResultFor('getExistingTabGroupsAndChats', Promise.resolve({
      groups: [createGroup('Saved', TAB_1, TAB_2)],
      ungroupedTabs: [],
      history: [{role: ChatRole.kUser, content: 'hi'}],
    }));
    await createView();
    assertDeepEquals(['Saved'], groupLabels());
    assertTrue(!!$('#prompt-card'));
  });

  test('create groups confirms and returns to landing', async () => {
    await checkNow();
    handler.resetResolver('getConfirmedTabGroups');
    await click('#primary-button');
    assertEquals(1, handler.getCallCount('confirmAllTabGroups'));
    await handler.whenCalled('getConfirmedTabGroups');
    await microtasksFinished();
    assertTrue(!!$('#landing-card'));
    assertFalse(!!$('#prompt-card'));
  });

  test('typing a prompt turns the primary action into Update', async () => {
    await checkNow();
    await typePrompt('Split by task');
    assertEquals('Update', primaryButton().textContent.trim());

    handler.setResultFor(
        'retrieveAndGroupTabs',
        Promise.resolve(groupingResult(
            [createGroup('A', TAB_1), createGroup('B', TAB_2, TAB_3)])));
    await click('#primary-button');
    assertEquals('Split by task', handler.getArgs('retrieveAndGroupTabs')[1]);
    assertEquals(0, handler.getCallCount('confirmAllTabGroups'));
    assertDeepEquals(['A', 'B'], groupLabels());
    assertEquals('', $<CrTextareaElement>('#change-input')!.value);
    assertFalse($<CrButtonElement>('#undo-button')!.disabled);

    // Undo restores the previous suggestions and syncs them to the browser.
    await click('#undo-button');
    assertDeepEquals(['Work'], groupLabels());
    assertEquals(1, handler.getCallCount('replaceTabGroups'));
    assertDeepEquals(
        ['Work'],
        handler.getArgs('replaceTabGroups')[0].map((g: TabGroup) => g.label));
    assertTrue($<CrButtonElement>('#undo-button')!.disabled);
  });

  test('failed update keeps the current suggestions', async () => {
    await checkNow();
    await typePrompt('Nope');
    handler.setResultFor(
        'retrieveAndGroupTabs', Promise.reject(new Error('failed')));
    await click('#primary-button');
    assertDeepEquals(['Work'], groupLabels());
    assertEquals(GROUPING_FAILED_MESSAGE, $('.status')!.textContent.trim());
    assertTrue($<CrButtonElement>('#undo-button')!.disabled);
  });

  test('rename updates the label and is undoable', async () => {
    await checkNow();
    await click('.edit-group-btn');
    const input = $<CrInputElement>('#rename-input')!;
    input.value = 'Renamed';
    input.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: 'Renamed'}}));
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    assertDeepEquals(['Renamed'], groupLabels());
    assertEquals('Renamed', handler.getArgs('replaceTabGroups')[0][0].label);
    await click('#undo-button');
    assertDeepEquals(['Work'], groupLabels());
  });

  test('dismissing the last group keeps the session active', async () => {
    await checkNow();
    await click('.dismiss-group-btn');
    assertDeepEquals([], groupLabels());
    assertDeepEquals([], handler.getArgs('replaceTabGroups')[0]);
    assertEquals(
        'No suggested groups left', $('.card-title')!.textContent.trim());
    assertEquals(
        'Not grouped (3)',
        $('#ungrouped-card .card-title')!.textContent.trim());
    assertTrue(primaryButton().disabled);
    assertFalse($<CrButtonElement>('#undo-button')!.disabled);
  });

  test('shows the model response with the suggestions', async () => {
    handler.setResultFor('retrieveAndGroupTabs', Promise.resolve({
      ...groupingResult([createGroup('Work', TAB_1, TAB_2)], [TAB_3]),
      llmResponse: {role: ChatRole.kAssistant, content: 'Grouped by task.'},
    }));
    await createView();
    await click('#check-now-button');
    assertEquals('Grouped by task.', $('.response-text')!.textContent.trim());

    // A follow-up replaces the note; Undo brings the previous one back.
    await typePrompt('Split it');
    handler.setResultFor('retrieveAndGroupTabs', Promise.resolve({
      ...groupingResult([createGroup('A', TAB_1), createGroup('B', TAB_2)]),
      llmResponse: {role: ChatRole.kAssistant, content: 'Split in two.'},
    }));
    await click('#primary-button');
    assertEquals('Split in two.', $('.response-text')!.textContent.trim());
    await click('#undo-button');
    assertEquals('Grouped by task.', $('.response-text')!.textContent.trim());

    await click('#clear-button');
    assertFalse(!!$('.response-text'));
  });

  test('clear discards suggestions and chat context', async () => {
    await checkNow();
    await click('#clear-button');
    assertEquals(1, handler.getCallCount('clearTabGroups'));
    assertEquals(1, handler.getCallCount('clearTabGroupChatHistory'));
    assertTrue(!!$('#landing-card'));
    assertFalse(!!$('#prompt-card'));
  });

  test('clicking a tab switches to it', async () => {
    await checkNow();
    await click('.tab-row');
    assertEquals(TAB_1.id, handler.getArgs('switchToTab')[0]);
  });

  test('confirmed groups keep their actions', async () => {
    handler.setResultFor('getConfirmedTabGroups', Promise.resolve({
      groups: [{
        label: 'Done',
        tabs: [TAB_4],
        savedGuid: {value: 'guid-1'},
      }],
    }));
    handler.setResultFor('removeConfirmedTabGroup', Promise.resolve());
    handler.setResultFor('removeAllConfirmedTabGroups', Promise.resolve());
    await createView();
    assertEquals(
        'Your tab groups (1)',
        $('#confirmed-card .card-title')!.textContent.trim());
    assertTrue(!!$('#confirmed-card .readonly-tab .favicon'));

    await click('.ungroup-one-group-btn');
    assertDeepEquals(
        {value: 'guid-1'}, handler.getArgs('removeConfirmedTabGroup')[0]);
    await click('#ungroup-all-confirmed-groups-button');
    assertEquals(1, handler.getCallCount('removeAllConfirmedTabGroups'));
  });
});
