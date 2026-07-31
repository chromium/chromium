// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/searchbox/searchbox_input.js';

import {createSearchMatchForTesting, SearchboxBrowserProxy} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxInputElement} from 'chrome://resources/cr_components/searchbox/searchbox_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertIconMaskImageUrl, createClipboardEvent, createUrlMatch} from './searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

async function createInput(properties: Partial<SearchboxInputElement> = {}):
    Promise<SearchboxInputElement> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const input = document.createElement('cr-searchbox-input');
  Object.assign(input, {placeholderText: 'Search'}, properties);
  document.body.appendChild(input);
  await input.updateComplete;
  return input;
}

suite('SearchboxInputTest', () => {
  let input: SearchboxInputElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
  });

  test('default loupe icon', async () => {
    loadTimeData.resetForTesting({
      isLensSearchbox: false,
      isTopChromeSearchbox: false,
    });
    input = await createInput(
        {searchboxIcon: 'search.svg', placeholderText: 'Search'});
    assertIconMaskImageUrl(input.$.icon, 'search.svg');
  });

  test('Copying or cutting empty input fails', async () => {
    input = await createInput();
    input.inputElement.value = '';

    const copyEvent = createClipboardEvent('copy');
    input.inputElement.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    input.inputElement.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);
  });

  test('Copying or cutting search match fails', async () => {
    input = await createInput();
    input.setInput({text: 'hello ', inline: 'world'});
    input.selectedMatch = createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'world',
    });
    input.inputHasMatches = true;

    assertEquals('hello world', input.inputElement.value);

    // Select the entire input.
    input.setSelectionRange(0, input.inputElement.value.length);

    const copyEvent = createClipboardEvent('copy');
    input.inputElement.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    input.inputElement.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);
  });

  test('Copying or cutting URL match succeeds', async () => {
    input = await createInput();
    input.setInput({text: 'hello', inline: 'world.com'});
    input.selectedMatch = createUrlMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'world.com',
    });
    input.inputHasMatches = true;

    assertEquals('helloworld.com', input.inputElement.value);

    const copyEvent = createClipboardEvent('copy');
    input.inputElement.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    input.inputElement.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);

    // Select the entire input.
    input.setSelectionRange(0, input.inputElement.value.length);

    let textUpdatedEventCount = 0;
    input.addEventListener('searchbox-input-text-updated', () => {
      textUpdatedEventCount++;
    });

    input.inputElement.dispatchEvent(copyEvent);
    assertTrue(copyEvent.defaultPrevented);
    assertEquals(
        'https://helloworld.com/',
        copyEvent.clipboardData!.getData('text/plain'));
    assertEquals(0, textUpdatedEventCount);

    input.inputElement.dispatchEvent(cutEvent);
    assertTrue(cutEvent.defaultPrevented);
    assertEquals(
        'https://helloworld.com/',
        cutEvent.clipboardData!.getData('text/plain'));

    // Cut should update the text to empty.
    assertEquals(1, textUpdatedEventCount);
    assertEquals('', input.inputElement.value);
  });

  test('Tabbing or clicking fires event', async () => {
    input = await createInput();
    input.inputElement.value = 'hello';

    let eventCount = 0;
    let eventValue = '';
    input.addEventListener('input-focus-changed', (e: Event) => {
      eventCount++;
      eventValue = (e as CustomEvent<{value: string}>).detail.value;
    });

    const mousedownEvent = new MouseEvent('mousedown', {button: 0});
    input.inputElement.dispatchEvent(mousedownEvent);
    assertEquals(
        1, eventCount, 'Expected one event to be fired after mousedown');
    assertEquals('hello', eventValue);

    const mousedownEventRightClick = new MouseEvent('mousedown', {button: 1});
    input.inputElement.dispatchEvent(mousedownEventRightClick);
    assertEquals(
        1, eventCount, 'Expected one event to be fired after right-click');

    const keyupEvent = new KeyboardEvent('keyup', {key: 'Tab'});
    input.inputElement.dispatchEvent(keyupEvent);
    assertEquals(2, eventCount);
    assertEquals('hello', eventValue);

    const keyupEventOther = new KeyboardEvent('keyup', {key: 'Enter'});
    input.inputElement.dispatchEvent(keyupEventOther);
    assertEquals(2, eventCount);
  });

  test('input text appears on page call from browser', async () => {
    input = await createInput();
    assertEquals(input.inputElement.value, '');

    let textUpdatedEventCount = 0;
    input.addEventListener('searchbox-input-text-updated', () => {
      textUpdatedEventCount++;
    });

    testProxy.callbackRouterRemote.setInputText('Hello');
    await microtasksFinished();

    assertEquals(input.inputElement.value, 'Hello');
    assertEquals(0, textUpdatedEventCount);
  });

  test('Pasting file fires event', async () => {
    input = await createInput({allowFilePaste: true});

    let eventCount = 0;
    let eventFiles: FileList|null = null;
    input.addEventListener('searchbox-input-files-pasted', (e: Event) => {
      eventCount++;
      eventFiles = (e as CustomEvent<{files: FileList}>).detail.files;
    });

    const file = new File([''], 'test.png', {type: 'image/png'});
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      cancelable: true,
    });

    input.inputElement.dispatchEvent(pasteEvent);
    assertTrue(pasteEvent.defaultPrevented);
    assertEquals(1, eventCount);
    assertTrue(!!eventFiles);
    assertEquals(1, (eventFiles as unknown as FileList).length);
    assertEquals('test.png', (eventFiles as unknown as FileList)[0]!.name);
  });

  test('Typing over inline autocompletion', async () => {
    loadTimeData.overrideValues({
      reportMetrics: false,
    });
    input = await createInput();
    input.setInput({text: 'hel', inline: 'lo'});

    let textUpdatedEventCount = 0;
    let textUpdatedValue = '';
    input.addEventListener('searchbox-input-text-updated', (e: Event) => {
      textUpdatedEventCount++;
      textUpdatedValue = (e as CustomEvent<{value: string}>).detail.value;
    });

    // Press 'l' which is the next char in inline autocompletion 'lo'.
    const keydownEvent = new KeyboardEvent('keydown', {
      key: 'l',
      cancelable: true,
    });
    input.inputElement.dispatchEvent(keydownEvent);

    assertTrue(keydownEvent.defaultPrevented);
    assertEquals(1, textUpdatedEventCount);
    assertEquals('hell', textUpdatedValue);
    assertEquals('hello', input.inputElement.value);

    // The selection should now highlight 'o'.
    assertEquals(4, input.inputElement.selectionStart);
    assertEquals(5, input.inputElement.selectionEnd);
  });

  test('allows empty string placeholder without fallback', async () => {
    // Set `placeholderText` to empty string.
    input = await createInput({placeholderText: ''});
    assertEquals('', input.inputElement.placeholder);

    // Update `placeholderText` dynamically.
    input.placeholderText = 'Updated Search';
    await input.updateComplete;
    assertEquals('Updated Search', input.inputElement.placeholder);

    // Reset `placeholderText` to empty string'', to suppress it again.
    input.placeholderText = '';
    await input.updateComplete;
    assertEquals('', input.inputElement.placeholder);
  });

  test('Fires searchbox-input-pasted event on paste', async () => {
    input = await createInput();

    let pasteEventCount = 0;
    input.addEventListener('searchbox-input-pasted', () => {
      pasteEventCount++;
    });

    const pasteEvent = createClipboardEvent('paste');
    input.inputElement.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertEquals(1, pasteEventCount);
  });
});
