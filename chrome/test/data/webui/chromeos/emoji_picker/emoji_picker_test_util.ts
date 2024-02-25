// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EMOJI_PICKER_READY, EmojiButton, EmojiGroupComponent, EmojiPickerApiProxy, EmojiPickerApp} from 'chrome://emoji-picker/emoji_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

export function assertCloseTo(actual: number, expected: number) {
  assertTrue(
      Math.abs(1 - actual / expected) <= 0.001,
      `expected ${expected} to be close to ${actual}`);
}

/**
 * Queries for an element through a path of custom elements.
 * This is needed because querySelector() does not query into
 * custom elements' shadow roots.
 */
export function deepQuerySelector(root: Element, path: string[]): HTMLElement|
    null {
  assert(root, 'deepQuerySelector called with null root');

  let el: ShadowRoot|Element|null = root;

  for (const part of path) {
    if (el.shadowRoot) {
      el = el.shadowRoot;
    }

    el = el.querySelector(part);
    if (!el) {
      break;
    }
  }

  return el as HTMLElement | null;
}

/**
 * Constructs a promise which resolves when the given condition function
 * evaluates to a truthy value.
 */
export async function waitForCondition<T>(
    condition: () => T, message: string,
    maxWait = 5000): Promise<NonNullable<T>> {
  const interval = 10;
  let waiting = 0;

  /** @type {T} */
  let result;
  while (!(result = condition()) && waiting < maxWait) {
    await timeout(interval);
    waiting += interval;
  }
  assert(
      result,
      message || 'waitForCondition timed out after ' + maxWait + ' ms.');

  return result;
}

/**
 * Constructs a promise which resolves after the given amount of time.
 */
export function timeout(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * Constructs a promise which resolves after 0 seconds.
 */
export function completePendingMicrotasks(): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, 0);
  });
}

/**
 * Constructs a promise which resolves when the given promise resolves,
 * or fails after the given timeout - whichever occurs first.
 */
export function waitWithTimeout<T>(
    promise: Promise<T>, ms: number, message: string): Promise<T> {
  message = message || 'waiting for promise timed out after ' + ms + ' ms.';
  return Promise.race(
      [promise, timeout(ms).then(() => Promise.reject(new Error(message)))]);
}

/**
 * Constructs a promise which resolves when the given element receives
 * an event of the given type.
 * Note: this function should be called *before* event is expected to set up
 * the handler, then it should be awaited when the event is required.
 */
export function waitForEvent(
    element: Element, eventType: string): Promise<Event> {
  return new Promise(
      resolve => element.addEventListener(eventType, resolve, {once: true}));
}


/**
 * Simulates a mouse click event on the given element.
 */
export function dispatchMouseEvent(
    element: Element, button: number, eventType = 'contextmenu') {
  element.dispatchEvent(new MouseEvent(eventType, {
    bubbles: true,
    cancelable: true,
    view: window,
    button: button,
    buttons: 0,
    clientX: element.getBoundingClientRect().x,
    clientY: element.getBoundingClientRect().y,
  }));
}

const ACTIVE_EMOJI_GROUP_CLASS = 'emoji-group-active';
const ACTIVE_TEXT_GROUP_CLASS = 'text-group-active';
/**
 * Checks if the given emoji-group-button or text-group-button element is
 * activated.
 */
export function isGroupButtonActive(element: Element|null): boolean {
  assert(element, 'group button element should not be null');
  return element!.classList.contains(ACTIVE_EMOJI_GROUP_CLASS) ||
      element!.classList.contains(ACTIVE_TEXT_GROUP_CLASS);
}

/**
 * Set up emoji picker and some helper functions for tests.
 * Ideally this is called at the top level (outside setup()), but a bunch of
 * tests rely on using this to reset state - in that case this must go inisde
 * setup() which is more like jasmine beforeEach()
 */
export function initialiseEmojiPickerForTest(
    incognito = false, localStorage: Array<{key: string, value: string}> = []) {
  const setIncognito = (incognito: boolean) => {
    EmojiPickerApiProxy.getInstance().isIncognitoTextField = async () =>
        ({incognito});
  };

  // Set default incognito state to False.
  setIncognito(incognito);
  EmojiPickerApp.configs = () => ({
    dataUrls: {
      emoji: [
        '/emoji_test_ordering_start.json',
        '/emoji_test_ordering_remaining.json',
      ],
      emoticon: ['/emoticon_test_ordering.json'],
      symbol: ['/symbol_test_ordering.json'],
      gif: [],
    },
  });

  // Reset DOM state.
  assert(window.trustedTypes);
  document.body.innerHTML = window.trustedTypes.emptyHTML;
  window.localStorage.clear();

  for (const {key, value} of localStorage) {
    window.localStorage.setItem(key, value);
  }

  let emojiPicker =
      document.createElement('emoji-picker-app') as EmojiPickerApp;

  const findInEmojiPicker = (...path: string[]) =>
      deepQuerySelector(emojiPicker, path);

  const waitUntilFindInEmojiPicker = async(...path: string[]):
      Promise<HTMLElement> => {
        await waitForCondition(
            () => findInEmojiPicker(...path) !== null,
            'element should not be null');
        return findInEmojiPicker(...path)!;
      };

  const findEmojiFirstButton = (...path: string[]) => {
    const emojiElement = findInEmojiPicker(...path);
    return (emojiElement as EmojiGroupComponent | null)?.firstEmojiButton();
  };

  const findEmojiButtonByText = (text: string, group: HTMLElement) => {
    const buttons = Array.from(
        group.shadowRoot!.querySelectorAll<HTMLElement>('.emoji-button'));
    return buttons.find(button => button.innerText === text) ?? null;
  };

  const findGroup = (groupId: string) =>
      findInEmojiPicker(`[data-group="${groupId}"] > emoji-group`);

  const findSearchGroup = (category: string) =>
      findInEmojiPicker('emoji-search', `emoji-group[category="${category}"]`);

  const expectEmojiButton = (text: string, getGroup = () => findGroup('0')) =>
      waitForCondition(() => {
        const group = getGroup();
        return group ? findEmojiButtonByText(text, group) : null;
      }, `wait for emoji ${text} to render`);

  const expectEmojiButtons =
      (texts: string[], getGroup?: () => HTMLElement | null) =>
          Promise.all(texts.map(text => expectEmojiButton(text, getGroup)));

  const findVariant = (text: string, button: HTMLElement) => {
    const variants =
        button.parentElement?.querySelector<HTMLElement>('emoji-variants');

    if (!variants || variants.style.display === 'none') {
      return null;
    }

    const variantButtons =
        Array.from(variants?.shadowRoot!.querySelectorAll('emoji-button'));
    const component =
        variantButtons.find(button => (button as EmojiButton).emoji === text);

    return component?.shadowRoot!.querySelector<HTMLElement>('#emoji-button') ??
        null;
  };

  const clickVariant = async (text: string, button: HTMLElement) => {
    dispatchMouseEvent(button, 2);
    const variant = await waitForCondition(
        () => findVariant(text, button),
        `wait for variants for emoji ${text} to render`);
    variant.click();
  };

  const scrollDown = (height: number) => {
    const thisRect = emojiPicker.$.groups;
    if (thisRect) {
      thisRect.scrollTop += height;
    }
  };

  const scrollToBottom = () => {
    const thisRect = emojiPicker.$.groups;
    if (!thisRect) {
      return;
    }
    const searchResultRect =
        emojiPicker.getActiveGroupAndId(thisRect.getBoundingClientRect()).group;
    if (searchResultRect) {
      thisRect.scrollTop += searchResultRect.getBoundingClientRect().bottom;
    }
  };

  // Wait until emoji data is loaded before executing tests.
  const createReadyPromise = () => new Promise<void>((resolve) => {
    emojiPicker.addEventListener(EMOJI_PICKER_READY, () => {
      flush();
      resolve();
    });
    document.body.appendChild(emojiPicker);
  });

  const reload = async () => {
    emojiPicker.remove();
    emojiPicker = document.createElement('emoji-picker-app');
    await createReadyPromise();
  };

  return {
    emojiPicker,
    findInEmojiPicker,
    waitUntilFindInEmojiPicker,
    findEmojiFirstButton,
    expectEmojiButton,
    expectEmojiButtons,
    clickVariant,
    findGroup,
    findSearchGroup,
    readyPromise: createReadyPromise(),
    reload,
    setIncognito,
    scrollDown,
    scrollToBottom,
  };
}

/**
 * Asserts the alt attribute of EmojiImage.
 */
export function assertEmojiImageAlt(element: HTMLElement|undefined, alt: string): void {
  assert(element!.shadowRoot?.querySelector('img')?.alt, alt);
}