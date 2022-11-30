// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DomIf} from 'chrome://new-tab-page/new_tab_page.js';
import {BackgroundImage, Theme} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertEquals, assertNotEquals} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.js';

export const NONE_ANIMATION: string = 'none 0s ease 0s 1 normal none running';

export function keydown(element: HTMLElement, key: string) {
  keyDownOn(element, 0, [], key);
}

/**
 * Asserts the computed style value for an element.
 * @param name The name of the style to assert.
 * @param expected The expected style value.
 */
export function assertStyle(element: Element, name: string, expected: string) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual);
}

/**
 * Asserts the computed style for an element is not value.
 * @param name The name of the style to assert.
 * @param not The value the style should not be.
 */
export function assertNotStyle(element: Element, name: string, not: string) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertNotEquals(not, actual);
}

/** Asserts that an element is focused. */
export function assertFocus(element: HTMLElement) {
  assertEquals(element, getDeepActiveElement());
}

type Constructor<T> = new (...args: any[]) => T;

export function createMock<T extends object>(clazz: Constructor<T>):
    {mock: T, callTracker: TestBrowserProxy} {
  const callTracker = new TestBrowserProxy(
      Object.getOwnPropertyNames(clazz.prototype)
          .filter(methodName => methodName !== 'constructor'));
  const handler = {
    get: function(_target: T, prop: string) {
      if (clazz.prototype[prop] instanceof Function) {
        return (...args: any[]) => callTracker.methodCalled(prop, ...args);
      }
      if (Object.getOwnPropertyDescriptor(clazz.prototype, prop)!.get) {
        return callTracker.methodCalled(prop);
      }
      return undefined;
    },
  };
  return {mock: new Proxy<T>({} as unknown as T, handler), callTracker};
}

type Installer<T> = (instance: T) => void;

export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestBrowserProxy {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const {mock, callTracker} = createMock(clazz);
  installer!(mock);
  return callTracker;
}

export function createBackgroundImage(url: string): BackgroundImage {
  return {
    url: {url},
    attributionUrl: undefined,
    positionX: undefined,
    positionY: undefined,
    repeatX: undefined,
    repeatY: undefined,
    scrimDisplay: undefined,
    size: undefined,
    url2x: undefined,
  };
}

export function createTheme(isDark: boolean = false): Theme {
  const mostVisited = {
    backgroundColor: {value: 0xff00ff00},
    isDark,
    useTitlePill: false,
    useWhiteTileIcon: false,
  };
  return {
    backgroundColor: {value: 0xffff0000},
    backgroundImage: undefined,
    backgroundImageAttribution1: '',
    backgroundImageAttribution2: '',
    backgroundImageAttributionUrl: undefined,
    dailyRefreshCollectionId: '',
    isDark,
    logoColor: undefined,
    mostVisited: mostVisited,
    textColor: {value: 0xff0000ff},
    isCustomBackground: true,
  };
}

export async function initNullModule(): Promise<null> {
  return null;
}

export function createElement(): HTMLElement {
  return document.createElement('div');
}

export function render(element: HTMLElement) {
  element.shadowRoot!.querySelectorAll<DomIf>('dom-if').forEach(
      tmpl => tmpl.render());
}

export function capture(
    target: HTMLElement, event: string): {received: boolean} {
  const capture = {received: false};
  target.addEventListener(event, () => capture.received = true);
  return capture;
}
