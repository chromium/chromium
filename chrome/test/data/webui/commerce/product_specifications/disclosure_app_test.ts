// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/header.js';
import 'chrome://compare/disclosure/app.js';

import type {DisclosureAppElement} from 'chrome://compare/disclosure/app.js';
import {ProductSpecificationsDisclosureVersion} from 'chrome://compare/shopping_service.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {$$} from './test_support.js';

suite('DisclosureAppTest', () => {
  let app: DisclosureAppElement;
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);

  const fakeAboutString = 'fake string 1';
  const fakeAccountString = 'fake string 2';
  const fakeDataString = 'fake string 3';
  const fakeDeclineString = 'fake decline string';
  const fakeAcceptButtonString = 'fake accept string';

  setup(async () => {
    shoppingServiceApi.reset();
    BrowserProxyImpl.setInstance(shoppingServiceApi);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('product-specifications-disclosure-app');
    document.body.appendChild(app);

    loadTimeData.overrideValues({
      acceptDisclosure: fakeAcceptButtonString,
      declineDisclosure: fakeDeclineString,
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

  test('accept button shows the correct text', async () => {
    const acceptButton = $$<HTMLElement>(app, 'cr-button.action-button');
    assertTrue(!!acceptButton);
    assertEquals(fakeAcceptButtonString, acceptButton!.innerText);
  });

  test('accept button updates pref to current version', async () => {
    const acceptButton = $$<HTMLElement>(app, 'cr-button.action-button');
    assertTrue(!!acceptButton);
    acceptButton?.click();

    assertEquals(
        1,
        shoppingServiceApi.getCallCount(
            'setProductSpecificationDisclosureAcceptVersion'));
    assertEquals(
        ProductSpecificationsDisclosureVersion.kV1,
        shoppingServiceApi.getArgs(
            'setProductSpecificationDisclosureAcceptVersion')[0] as
            ProductSpecificationsDisclosureVersion);
  });

  test('decline button shows the correct text', async () => {
    const declineButton = $$<HTMLElement>(app, 'cr-button.tonal-button');
    assertTrue(!!declineButton);
    assertEquals(fakeDeclineString, declineButton!.innerText);
  });
});
