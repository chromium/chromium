// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BackgroundImage, Theme, ThirdPartyThemeInfo} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

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

export function assertStyle(element: Element, name: string, expected: string) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual);
}

export function assertNotStyle(element: Element, name: string, not: string) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertNotEquals(not, actual);
}

/**
 * Queries |selector| on |element|'s shadow root and returns the resulting
 * element if there is any.
 */
export function $$<E extends Element = Element>(
    element: Element, selector: string): E|null;
export function $$(element: Element, selector: string) {
  return element.shadowRoot!.querySelector(selector);
}

export function createBackgroundImage(url: string): BackgroundImage {
  return {
    url: {url},
    snapshotUrl: {url},
    isUploadedImage: false,
    localBackgroundId: null,
    title: '',
    collectionId: '',
    dailyRefreshEnabled: false,
  };
}

export function createThirdPartyThemeInfo(
    id: string, name: string): ThirdPartyThemeInfo {
  return {
    id: id,
    name: name,
  };
}

export function createTheme(): Theme {
  return {
    backgroundImage: null,
    thirdPartyThemeInfo: null,
    backgroundColor: {value: 0xffff0000},
    foregroundColor: null,
    backgroundManagedByPolicy: false,
    followDeviceTheme: false,
  };
}

export function capture(
    target: HTMLElement, event: string): {received: boolean} {
  const capture = {received: false};
  target.addEventListener(event, () => capture.received = true);
  return capture;
}
