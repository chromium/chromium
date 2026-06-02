// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewDestinationListElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, getTrustedHTML} from 'chrome://print/print_preview.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('DestinationListFocusTest', function() {
  let list: PrintPreviewDestinationListElement;

  setup(async function() {
    // Create destinations
    const destinations = [
      new Destination(
          'id1', DestinationOrigin.LOCAL, 'One', {description: 'ABC'}),
      new Destination(
          'id2', DestinationOrigin.LOCAL, 'Two', {description: 'XYZ'}),
      new Destination(
          'id3', DestinationOrigin.LOCAL, 'Three',
          {description: 'ABC', location: '123'}),
      new Destination(
          'id4', DestinationOrigin.LOCAL, 'Four',
          {description: 'XYZ', location: '123'}),
      new Destination(
          'id5', DestinationOrigin.LOCAL, 'Five',
          {description: 'XYZ', location: '123'}),
      new Destination(
          'ext_id', DestinationOrigin.EXTENSION, 'ExtensionPrinter', {
            extensionId: 'ext1',
            extensionName: 'ExtensionOne',
          }),
    ];

    // Set up list with dummy inputs for focus/tab tests and set height to
    // ensure all items render
    document.body.innerHTML = getTrustedHTML`
          <input id="prev" tabindex="0">
          <print-preview-destination-list id="testList" style="height: 300px;">
          </print-preview-destination-list>
          <input id="next" tabindex="0">`;
    list = document.body.querySelector<PrintPreviewDestinationListElement>(
        '#testList')!;
    list.searchQuery = null;

    await microtasksFinished();
    const infiniteList = list.$.list;
    const lazyList = infiniteList.querySelector('cr-lazy-list');
    assertTrue(!!lazyList);

    const viewportFilled = eventToPromise('viewport-filled', lazyList);
    list.destinations = destinations;
    await viewportFilled;
  });

  test('DelegatesFocusToExtensionIcon', async () => {
    const items =
        list.shadowRoot.querySelectorAll('print-preview-destination-list-item');
    const extItem =
        Array.from(items).find(item => item.destination!.isExtension);
    assertTrue(extItem !== undefined);

    await microtasksFinished();
    extItem.focus();
    await microtasksFinished();

    const extensionIcon = extItem.shadowRoot.querySelector('.extension-icon');

    assertTrue(!!extensionIcon);
    assertEquals(extensionIcon, getDeepActiveElement());
  });

  test('FocusStaysOnNormalItem', async () => {
    const items =
        list.shadowRoot.querySelectorAll('print-preview-destination-list-item');
    const normalItem =
        Array.from(items).find(item => !item.destination!.isExtension);
    assertTrue(normalItem !== undefined);

    await microtasksFinished();
    normalItem.focus();
    await microtasksFinished();

    const active = getDeepActiveElement();

    assertTrue(normalItem === active);
  });
});
