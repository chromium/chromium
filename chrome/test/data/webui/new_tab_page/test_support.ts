// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BackgroundImage, Theme} from 'chrome://new-tab-page/new_tab_page.js';
import {NtpBackgroundImageSource} from 'chrome://new-tab-page/new_tab_page.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
  installer(mock);
  return mock;
}

export function createBackgroundImage(url: string): BackgroundImage {
  return {
    url: url,
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

export function createTheme({
  isDark = false,
  isBaseline = true,
  isGm3 = true,
  backgroundColor = {
    value: 0xffff0000,
  },
} = {}): Theme {
  const mostVisited = {
    backgroundColor: {value: 0xff00ff00},
    isDark,
    useWhiteTileIcon: false,
  };
  return {
    backgroundColor: backgroundColor,
    backgroundImage: null,
    backgroundImageAttributionUrl: null,
    backgroundImageAttribution1: '',
    backgroundImageAttribution2: '',
    dailyRefreshEnabled: false,
    backgroundImageCollectionId: '',
    logoColor: null,
    isBaseline: isBaseline,
    isDark,
    isGm3,
    mostVisited: mostVisited,
    textColor: {value: 0xff0000ff},
    isCustomBackground: true,
  };
}

export function initNullModule(): Promise<null> {
  return Promise.resolve(null);
}

export function createElement(): HTMLElement {
  return document.createElement('div');
}

export function capture(
    target: HTMLElement, event: string): {received: boolean} {
  const capture = {received: false};
  target.addEventListener(event, () => capture.received = true);
  return capture;
}

export interface Coordinate {
  x: number;
  y: number;
}

/**
 * Calculates the bounding rectangle center coordinate for a given element,
 * or returns null if the element is not provided.
 */
export function getCenter(element: Element): Coordinate;
export function getCenter(element: null|undefined): null;
export function getCenter(element: Element|null|undefined): Coordinate|null;
export function getCenter(element: Element|null|undefined): Coordinate|null {
  if (!element) {
    return null;
  }
  const rect = element.getBoundingClientRect();
  return {
    x: rect.left + rect.width / 2,
    y: rect.top + rect.height / 2,
  };
}

/**
 * Calculates the visual text centerline coordinate for an input, textarea,
 * or text-bearing element by accounting for top padding, border, and
 * line-height. Returns null if the element is not provided.
 */
export function getTextCenter(element: Element): Coordinate;
export function getTextCenter(element: null|undefined): null;
export function getTextCenter(element: Element|null|undefined): Coordinate|null;
export function getTextCenter(element: Element|null|undefined): Coordinate|
    null {
  if (!element) {
    return null;
  }
  const rect = element.getBoundingClientRect();
  const style = window.getComputedStyle(element);
  const borderTop = parseFloat(style.borderTopWidth) || 0;
  const paddingTop = parseFloat(style.paddingTop) || 0;
  const lineHeight = parseFloat(style.lineHeight) || rect.height;
  return {
    x: rect.left + rect.width / 2,
    y: rect.top + borderTop + paddingTop + lineHeight / 2,
  };
}



/**
 * Traverses nested shadow DOM boundaries step-by-step using an array of
 * selectors (one selector per shadow DOM level).
 */
export function queryShadowPath(
    root: Element|null|undefined, ...selectors: string[]): Element|null {
  let current: Element|null|undefined = root;
  for (const selector of selectors) {
    if (!current) {
      return null;
    }
    const container = current.shadowRoot ?? current;
    current = container.querySelector(selector);
  }
  return current ?? null;
}

/**
 * Asserts that multiple DOM elements share an identical center coordinate
 * along the specified axis ('x' or 'y').
 *
 * @param root The root element from which to resolve selector paths.
 * @param elements A map of human-readable element names to either resolved
 *     Elements, selector paths, pre-computed coordinate objects, or
 * null/undefined.
 * @param axis The coordinate axis to compare:
 *     - 'x' compares the x values of the elements' center (i.e. confirms that
 *       elements arranged vertically in a column are aligned).
 *     - 'y' compares the y values of the elements' center (i.e. confirms that
 *       elements arranged horizontally in a row are aligned).
 */
export function assertCenterAligned(
    root: Element|null|undefined,
    elements: Record<string, Element|string|string[]|Coordinate|null|undefined>,
    axis: 'x'|'y'): void {
  const centers: Record<string, number> = {};
  const missingElements: string[] = [];

  for (const [name, target] of Object.entries(elements)) {
    // 1. Resolve selector or selector path to an Element (or null).
    let value: Element|Coordinate|null|undefined =
        typeof target === 'string' || Array.isArray(target) ?
        queryShadowPath(root, ...(Array.isArray(target) ? target : [target])) :
        target;


    // 2. Convert Element to its bounding-box center.
    if (value instanceof Element) {
      value = getCenter(value);
    }

    // 3. Extract coordinate along the requested axis.
    const coord = value?.[axis];
    if (coord === undefined) {
      missingElements.push(name);
      continue;
    }
    centers[name] = coord;
  }

  assertTrue(
      missingElements.length === 0,
      `Expected all elements to exist, but missing: ${
          missingElements.join(', ')}`);

  const uniqueValues = new Set(Object.values(centers));
  if (uniqueValues.size !== 1) {
    const details = Object.entries(centers)
                        .map(([name, val]) => `  - ${name}: ${val}px`)
                        .join('\n');
    assertEquals(
        1, uniqueValues.size,
        `Elements are not ${
            axis === 'y' ? 'vertically' :
                           'horizontally'} center-aligned (distinct ${
            axis.toUpperCase()} values: [${
            Array.from(uniqueValues).join(', ')}]):\n${details}`);
  }
}
