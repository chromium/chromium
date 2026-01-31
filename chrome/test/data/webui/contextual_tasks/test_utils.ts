// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

// Base64 encoding of a UI handshake request message [1, 2, 3].
// Generated from btoa(String.fromCharCode(...[1, 2, 3]))
export const HANDSHAKE_REQUEST_MESSAGE_BASE64 = 'AQID';

// Byte array of a typical handshake response from the webview.
// Equivalent to base64 decoding 'CgIIAA=='
export const HANDSHAKE_RESPONSE_BYTES = new Uint8Array([10, 2, 8, 0]);

export function assertHTMLElement(element: Element|null|undefined):
    asserts element is HTMLElement {
  assertTrue(!!element);
  assertTrue(element instanceof HTMLElement);
}

export function assertStyle(
    element: Element|null, name: string, expected: string, error: string = '') {
  assertTrue(!!element, `Element is null`);
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual, error);
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
