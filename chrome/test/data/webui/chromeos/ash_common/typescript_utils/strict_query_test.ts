// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assertEquals, assertThrows} from 'chrome://webui-test/chai_assert.js';

const initSectionElement = (): HTMLElement => {
  const sectionElement = document.createElement('section');
  sectionElement.innerText = 'test of strictQuery';
  document.body.appendChild(sectionElement);
  return sectionElement;
};

suite('StrictQueryTest', function() {
  let sectionElement: HTMLElement|null;

  setup(() => {
    // Reset the document HTML before each test. We cast to unknown and string
    // since TrustedHTML cannot be assigned to innerHTML (which is a string).
    document.body.innerHTML =
        (window.trustedTypes!.emptyHTML as unknown) as string;
  });

  teardown(() => {
    if (sectionElement) {
      sectionElement.remove();
    }
    sectionElement = null;
  });

  test('BasicQuery', function() {
    sectionElement = initSectionElement();
    const queriedElement = strictQuery('section', document.body, HTMLElement);
    assertEquals(sectionElement, queriedElement);
  });

  test('ThrowsIfNotFound', function() {
    sectionElement = initSectionElement();
    assertThrows(() => {
      strictQuery('invalid-element', document.body, HTMLElement);
    });
  });

  test('ThrowsIfDifferentType', function() {
    sectionElement = initSectionElement();
    assertThrows(() => {
      strictQuery('section', document.body, HTMLInputElement);
    });
  });
});