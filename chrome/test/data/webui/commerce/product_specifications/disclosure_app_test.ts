// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/header.js';
import 'chrome://compare/disclosure/app.js';

import type {DisclosureAppElement} from 'chrome://compare/disclosure/app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {$$} from './test_support.js';

suite('DisclosureAppTest', () => {
  let app: DisclosureAppElement;
  const fakeAboutString = 'fake string 1';
  const fakeAccountString = 'fake string 2';
  const fakeDataString = 'fake string 3';
  const fakeAcceptButtonString = 'fake accept string';
  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('product-specifications-disclosure-app');
    document.body.appendChild(app);
    loadTimeData.overrideValues({
      acceptDisclosure: fakeAcceptButtonString,
      disclosureAboutItem: fakeAboutString,
      disclosureAccountItem: fakeAccountString,
      disclosureDataItem: fakeDataString,
    });

    await flushTasks();
  });

  test('disclosure has 3 items', async () => {
    const container = app.shadowRoot!.querySelectorAll('.item');
    assertEquals(3, container.length);
  });

  test('disclosure has correct icons', async () => {
    const icons = app.shadowRoot!.querySelectorAll('.item cr-icon');
    assertEquals(3, icons.length);
    assertEquals(
        'product-specifications-disclosure:plant',
        icons[0]!.getAttribute('icon'));
    assertEquals(
        'product-specifications-disclosure:google',
        icons[1]!.getAttribute('icon'));
    assertEquals(
        'product-specifications-disclosure:frame',
        icons[2]!.getAttribute('icon'));
  });

  test('disclosure has correct item text', async () => {
    const items = app.shadowRoot!.querySelectorAll('.item div');
    assertEquals(3, items.length);
    assertEquals(fakeAboutString, items[0]!.textContent);
    assertEquals(fakeAccountString, items[1]!.textContent);
    assertEquals(fakeDataString, items[2]!.textContent);
  });

  test('disclosure has an accept button', async () => {
    const acceptButton = $$<HTMLElement>(app, '#acceptButton');
    assertTrue(!!acceptButton);
    assertEquals(fakeAcceptButtonString, acceptButton!.innerText);
  });
});
