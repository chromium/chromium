// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {ColorMode, PageSize, Scanner, ScanSource} from 'chrome://scanning/scanning.mojom-webui.js';
import {alphabeticalCompare} from 'chrome://scanning/scanning_app_util.js';
import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

export function assertOrderedAlphabetically<T>(
    arr: T[], conversionFn = (val: T): string => `${val}`) {
  for (let i = 0; i < arr.length - 1; i++) {
    // |alphabeticalCompare| will return -1 if the first argument is less than
    // the second and 0 if the two arguments are equal.
    assertTrue(
        alphabeticalCompare(conversionFn(arr[i]!), conversionFn(arr[i + 1]!)) <=
        0);
  }
}

export function createScanner(
    id: UnguessableToken, displayName: string): Scanner {
  return {id, displayName: strToMojoString16(displayName)};
}

export function createScannerSource(
    type: number, name: string, pageSizes: PageSize[], colorModes: ColorMode[],
    resolutions: number[]): ScanSource {
  return {type, name, pageSizes, colorModes, resolutions} as ScanSource;
}

/**
 * Converts a JS string to a mojo_base::mojom::String16 object.
 */
function strToMojoString16(str: string): String16 {
  const arr: number[] = [];
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }

  return {data: arr};
}

export function changeSelectedValue(
    select: HTMLSelectElement, value: string): Promise<void> {
  select.value = value;

  select.dispatchEvent(new CustomEvent('change'));
  return flushTasks();
}

export function changeSelectedIndex(
    select: HTMLSelectElement, index: number): Promise<void> {
  select.selectedIndex = index;

  select.dispatchEvent(new CustomEvent('change'));
  return flushTasks();
}

/**
 * Fake MediaQueryList for mocking behavior of |window.matchMedia|.
 */
export class FakeMediaQueryList extends EventTarget implements MediaQueryList {
  mediaString: string;
  mediaMatches = false;
  listener: EventListener|null = null;

  constructor(media: string) {
    super();
    this.mediaString = media;
  }

  addListener(listener: EventListener): void {
    this.listener = listener;
  }

  removeListener(): void {
    this.listener = null;
  }

  onchange(): void {
    if (!this.listener) {
      return;
    }

    this.listener(new window.MediaQueryListEvent(
        'change', {media: this.mediaString, matches: this.mediaMatches}));
  }

  get media(): string {
    return this.mediaString;
  }

  get matches(): boolean {
    return this.mediaMatches;
  }

  set matches(matches: boolean) {
    this.mediaMatches = matches;
    this.onchange();
  }
}
