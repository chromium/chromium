// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {CrIconButtonElement, PrivateStateTokensMetadataElement} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertArrayEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {dummyMetadata} from './test_data.js';

suite('MetadataTest', () => {
  let metadata: PrivateStateTokensMetadataElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metadata = document.createElement('private-state-tokens-metadata');
    document.body.appendChild(metadata);
    metadata.issuerOrigin = dummyMetadata.issuerOrigin;
    metadata.expiration = dummyMetadata.expiration;
    metadata.purposes = dummyMetadata.purposes;
    await microtasksFinished();
  });

  test('check layout', async () => {
    assertTrue(isVisible(metadata));
  });

  test('check data', () => {
    assertEquals(metadata.issuerOrigin, 'issuerTest.com');
    assertEquals(metadata.expiration, 'today');
    assertArrayEquals(dummyMetadata.purposes, metadata.purposes);
  });

  test('check back button functionality', async () => {
    const backRow = $$<HTMLElement>(metadata, '#backRow');
    const rowChild = backRow!.querySelector('#backRowText');
    const backButton =
        rowChild!.querySelector<CrIconButtonElement>('#backButton');
    assertTrue(!!backButton);
    backButton.click();
    const url = window.location.href;
    assertEquals(
        'chrome://privacy-sandbox-internals/private-state-tokens', url);
  });
});
