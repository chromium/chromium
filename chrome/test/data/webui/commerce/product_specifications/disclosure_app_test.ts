// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/header.js';
import 'chrome://compare/disclosure/app.js';

import type {DisclosureAppElement} from 'chrome://compare/disclosure/app.js';
import {ProductSpecificationsDisclosureVersion} from 'chrome://compare/shopping_service.mojom-webui.js';
import type {ProductSpecificationsSet} from 'chrome://compare/shopping_service.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {$$} from './test_support.js';

declare const chrome: {
  send(message: string, args: any): void,
  getVariableValue(variable: string): string,
};

suite('DisclosureAppTest', () => {
  let app: DisclosureAppElement;
  let metrics: MetricsTracker;
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);
  const fakeUserEmail = 'test@gmail.com';

  setup(async () => {
    metrics = fakeMetricsPrivate();
    shoppingServiceApi.reset();
    BrowserProxyImpl.setInstance(shoppingServiceApi);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('product-specifications-disclosure-app');
    document.body.appendChild(app);

    loadTimeData.overrideValues({userEmail: fakeUserEmail});
    await flushTasks();
  });

  test('records metrics for disclosure show', async () => {
    assertEquals(1, metrics.count('Commerce.Compare.FirstRunExperience.Shown'));
  });

  test('disclosure has 4 items', async () => {
    const container = app.shadowRoot!.querySelectorAll('.item');
    assertEquals(4, container.length);
  });

  test('disclosure has correct icons', async () => {
    const icons = app.shadowRoot!.querySelectorAll('.item cr-icon');
    assertEquals(4, icons.length);
    assertEquals(
        'product-specifications-disclosure:plant',
        icons[0]!.getAttribute('icon'));
    assertEquals(
        'product-specifications-disclosure:google',
        icons[1]!.getAttribute('icon'));
    assertEquals(
        'product-specifications-disclosure:frame',
        icons[2]!.getAttribute('icon'));
    assertEquals(
        'product-specifications-disclosure:user',
        icons[3]!.getAttribute('icon'));
  });

  test('disclosure has correct item text', async () => {
    const items = app.shadowRoot!.querySelectorAll('.item div');
    assertEquals(4, items.length);
    assertEquals(app.i18n('disclosureAboutItem'), items[0]!.textContent);
    assertEquals(app.i18n('disclosureTabItem'), items[1]!.textContent);
    assertEquals(app.i18n('disclosureDataItem'), items[2]!.textContent);
    assertEquals(
        app.i18n('disclosureAccountItem', fakeUserEmail),
        items[3]!.textContent);
  });

  test('disclosure has correct learn more link', async () => {
    const learnMoreLinkElement = $$<HTMLElement>(app, '#learnMoreLink');
    assertTrue(!!learnMoreLinkElement);
    assertTrue(!!learnMoreLinkElement!.textContent);
    assertEquals(
        app.i18n('learnMore'), learnMoreLinkElement!.textContent!.trim());
    assertEquals(
        loadTimeData.getString('compareLearnMoreUrl'),
        learnMoreLinkElement!.getAttribute('href'));
  });

  test('click disclosure learn more link', async () => {
    // Overwrite `chrome.send` for testing.
    const chromeSend = chrome.send;
    let receivedMessage = 'none';
    // chrome.send is used for test implementation, so we retain its function.
    const mockChromeSend = (message: string, args: any) => {
      receivedMessage = message;
      chromeSend(message, args);
    };
    chrome.send = mockChromeSend;
    assertEquals(
        0, metrics.count('Commerce.Compare.FirstRunExperience.LearnMore'));

    const learnMoreLinkElement = $$<HTMLElement>(app, '#learnMoreLink');
    assertTrue(!!learnMoreLinkElement);
    learnMoreLinkElement!.click();

    assertEquals(
        1, metrics.count('Commerce.Compare.FirstRunExperience.LearnMore'));
    // Received signal to close dialog.
    assertEquals(receivedMessage, 'dialogClose');

    // Restore chrome.send.
    chrome.send = chromeSend;
  });

  test('accept button shows the correct text', async () => {
    const acceptButton = $$<HTMLElement>(app, 'cr-button.action-button');
    assertTrue(!!acceptButton);
    assertEquals(app.i18n('acceptDisclosure'), acceptButton!.innerText);
  });

  test('click accept button', async () => {
    const setValue = {
      name: '',
      uuid: {value: '123'},
      urls: [],
    };
    const set = setValue as ProductSpecificationsSet;
    shoppingServiceApi.setResultFor(
        'addProductSpecificationsSet', Promise.resolve({createdSet: set}));

    // Overwrite `chrome.getVariableValue` for testing.
    const chromeGetVariableValue = chrome.getVariableValue;
    const testObject = {
      in_new_tab: false,
      name: 'test_name',
      urls: ['https://foo.com', 'https://bar.com'],
      set_id: '',
    };
    const testJson = JSON.stringify(testObject);
    chrome.getVariableValue = (message) => {
      if (message === 'dialogArguments') {
        return testJson;
      }
      return '';
    };

    // Overwrite `chrome.send` for testing.
    const chromeSend = chrome.send;
    let receivedMessage = 'none';
    // chrome.send is used for test implementation, so we retain its function.
    const mockChromeSend = (message: string, args: any) => {
      receivedMessage = message;
      chromeSend(message, args);
    };
    chrome.send = mockChromeSend;
    assertEquals(
        0, metrics.count('Commerce.Compare.FirstRunExperience.Accept'));

    const acceptButton = $$<HTMLElement>(app, 'cr-button.action-button');
    assertTrue(!!acceptButton);
    acceptButton.click();

    assertEquals(
        1, metrics.count('Commerce.Compare.FirstRunExperience.Accept'));
    // Ensure browser is called to update prefs.
    assertEquals(
        1,
        shoppingServiceApi.getCallCount(
            'setProductSpecificationDisclosureAcceptVersion'));
    assertEquals(
        ProductSpecificationsDisclosureVersion.kV1,
        shoppingServiceApi.getArgs(
            'setProductSpecificationDisclosureAcceptVersion')[0] as
            ProductSpecificationsDisclosureVersion);

    // Create product spec set.
    assertEquals(
        1, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
    const addSetArgs =
        shoppingServiceApi.getArgs('addProductSpecificationsSet');
    assertEquals('test_name', addSetArgs[0][0]);
    assertEquals('https://foo.com', addSetArgs[0][1][0].url);
    assertEquals('https://bar.com', addSetArgs[0][1][1].url);

    // Show product spec set.
    await shoppingServiceApi.whenCalled('showProductSpecificationsSetForUuid');
    assertEquals(
        1,
        shoppingServiceApi.getCallCount('showProductSpecificationsSetForUuid'));
    const showArgs =
        shoppingServiceApi.getArgs('showProductSpecificationsSetForUuid');
    assertEquals('123', showArgs[0][0].value);
    assertEquals(false, showArgs[0][1]);

    // Received signal to close dialog.
    assertEquals(receivedMessage, 'dialogClose');

    // Restore chrome.getVariableValue and chrome.send.
    chrome.getVariableValue = chromeGetVariableValue;
    chrome.send = chromeSend;
  });

  test('click accept button to create set with default name', async () => {
    const setValue = {
      name: '',
      uuid: {value: '123'},
      urls: [],
    };
    const set = setValue as ProductSpecificationsSet;
    shoppingServiceApi.setResultFor(
        'addProductSpecificationsSet', Promise.resolve({createdSet: set}));
    // Overwrite `chrome.getVariableValue` for testing.
    const chromeGetVariableValue = chrome.getVariableValue;
    const testObject = {
      in_new_tab: false,
      name: '',
      urls: ['https://foo.com', 'https://bar.com'],
      set_id: '',
    };
    const testJson = JSON.stringify(testObject);
    chrome.getVariableValue = (message) => {
      if (message === 'dialogArguments') {
        return testJson;
      }
      return '';
    };

    const acceptButton = $$<HTMLElement>(app, 'cr-button.action-button');
    assertTrue(!!acceptButton);
    acceptButton.click();

    // Create product spec set with the default name.
    assertEquals(
        1, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
    const addSetArgs =
        shoppingServiceApi.getArgs('addProductSpecificationsSet');
    assertEquals(loadTimeData.getString('defaultTableTitle'), addSetArgs[0][0]);

    // Restore chrome.getVariableValue.
    chrome.getVariableValue = chromeGetVariableValue;
  });

  test('click accept button to open existing set', async () => {
    // Overwrite `chrome.getVariableValue` for testing.
    const set_id = '123';
    const chromeGetVariableValue = chrome.getVariableValue;
    const testObject = {
      in_new_tab: false,
      name: '',
      urls: [],
      set_id: set_id,
    };
    const testJson = JSON.stringify(testObject);
    chrome.getVariableValue = (message) => {
      if (message === 'dialogArguments') {
        return testJson;
      }
      return '';
    };

    // Overwrite `chrome.send` for testing.
    const chromeSend = chrome.send;
    let receivedMessage = 'none';
    // chrome.send is used for test implementation, so we retain its function.
    const mockChromeSend = (message: string, args: any) => {
      receivedMessage = message;
      chromeSend(message, args);
    };
    chrome.send = mockChromeSend;

    const acceptButton = $$<HTMLElement>(app, 'cr-button.action-button');
    assertTrue(!!acceptButton);
    acceptButton.click();

    // Open product spec set with the default name.
    assertEquals(
        0, shoppingServiceApi.getCallCount('addProductSpecificationsSet'));
    assertEquals(
        1,
        shoppingServiceApi.getCallCount('showProductSpecificationsSetForUuid'));
    const showSetArgs =
        shoppingServiceApi.getArgs('showProductSpecificationsSetForUuid');
    assertEquals(set_id, showSetArgs[0][0].value);

    // Received signal to close dialog.
    assertEquals(receivedMessage, 'dialogClose');

    // Restore chrome.getVariableValue and chrome.send.
    chrome.getVariableValue = chromeGetVariableValue;
    chrome.send = chromeSend;
  });

  test('decline button shows the correct text', async () => {
    const declineButton = $$<HTMLElement>(app, 'cr-button.tonal-button');
    assertTrue(!!declineButton);
    assertEquals(app.i18n('declineDisclosure'), declineButton!.innerText);
  });

  test('click decline button', async () => {
    // Overwrite `chrome.send` for testing.
    const chromeSend = chrome.send;
    let receivedMessage = 'none';
    // chrome.send is used for test implementation, so we retain its function.
    const mockChromeSend = (message: string, args: any) => {
      receivedMessage = message;
      chromeSend(message, args);
    };
    chrome.send = mockChromeSend;
    assertEquals(
        0, metrics.count('Commerce.Compare.FirstRunExperience.Reject'));

    const declineButton = $$<HTMLElement>(app, 'cr-button.tonal-button');
    assertTrue(!!declineButton);
    declineButton.click();

    assertEquals(
        1, metrics.count('Commerce.Compare.FirstRunExperience.Reject'));
    // Ensure browser is called about declining the disclosure.
    assertEquals(
        1,
        shoppingServiceApi.getCallCount(
            'declineProductSpecificationDisclosure'));

    // Received signal to close dialog.
    assertEquals(receivedMessage, 'dialogClose');

    // Restore chrome.send.
    chrome.send = chromeSend;
  });

});
