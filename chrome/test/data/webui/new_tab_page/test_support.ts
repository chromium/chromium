// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DomIf} from 'chrome://new-tab-page/new_tab_page.js';
import type {BackgroundImage, Theme} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {NtpBackgroundImageSource} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

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
type Installer<T> = (instance: T) => void;

export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);
  installer!(mock);
  return mock;
}

export function createBackgroundImage(url: string): BackgroundImage {
  return {
    url: {url},
    url2x: null,
    attributionUrl: null,
    size: null,
    repeatX: null,
    repeatY: null,
    positionX: null,
    positionY: null,
    imageSource: NtpBackgroundImageSource.kNoImage,
  };
}

export function createTheme({isDark = false, isBaseline = true} = {}): Theme {
  const mostVisited = {
    backgroundColor: {value: 0xff00ff00},
    isDark,
    useWhiteTileIcon: false,
  };
  return {
    backgroundColor: {value: 0xffff0000},
    backgroundImage: null,
    backgroundImageAttributionUrl: null,
    backgroundImageAttribution1: '',
    backgroundImageAttribution2: '',
    dailyRefreshEnabled: false,
    backgroundImageCollectionId: '',
    logoColor: null,
    isBaseline: isBaseline,
    isDark,
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
