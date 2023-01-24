// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/lazy_load.js';

import {CartHandlerRemote} from 'chrome://new-tab-page/chrome_cart.mojom-webui.js';
import {ChromeCartProxy, CustomizeModulesElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {ModuleIdName, PageCallbackRouter, PageHandlerRemote, PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, installMock} from './test_support.js';

suite('NewTabPageCustomizeModulesTest', () => {
  let handler: TestBrowserProxy<PageHandlerRemote>;
  let callbackRouterRemote: PageRemote;
  let metrics: MetricsTracker;
  let cartHandler: TestBrowserProxy<CartHandlerRemote>;

  async function createCustomizeModules(
      allDisabled: boolean,
      modules: Array<{id: string, name: string, disabled: boolean}>):
      Promise<CustomizeModulesElement> {
    handler.setResultFor('getModulesIdNames', Promise.resolve({
      data: modules.map(({id, name}) => ({id, name} as ModuleIdName)),
    }));
    const customizeModules = document.createElement('ntp-customize-modules');
    document.body.appendChild(customizeModules);
    assertStyle(customizeModules.$.container, 'display', 'none');
    callbackRouterRemote.setDisabledModules(
        allDisabled,
        modules.filter(({disabled}) => disabled).map(({id}) => id));
    await callbackRouterRemote.$.flushForTesting();
    return customizeModules;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
    loadTimeData.overrideValues({modulesVisibleManagedByPolicy: false});
    cartHandler = installMock(CartHandlerRemote, ChromeCartProxy.setHandler);
    cartHandler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: false}));
    cartHandler.setResultFor(
        'getDiscountToggleVisible', Promise.resolve({toggleVisible: false}));
  });

  [true, false].forEach(visible => {
    const mode = visible ? 'customize' : 'hide';
    test(`creating element shows correct config in ${mode} mode`, async () => {
      // Arrange & Act.
      const customizeModules = await createCustomizeModules(!visible, [
        {id: 'foo', name: 'foo name', disabled: false},
        {id: 'bar', name: 'bar name', disabled: false},
        {id: 'baz', name: 'baz name', disabled: true},
      ]);

      // Assert.
      assertEquals(visible, customizeModules.$.customizeButton.checked);
      assertEquals(!visible, customizeModules.$.hideButton.checked);
      const names = customizeModules.shadowRoot!.querySelectorAll<HTMLElement>(
          '.toggle-name');
      assertEquals(3, names.length);
      assertEquals('foo name', names[0]!.textContent!.trim());
      assertEquals('bar name', names[1]!.textContent!.trim());
      assertEquals('baz name', names[2]!.textContent!.trim());
      const toggles =
          customizeModules.shadowRoot!.querySelectorAll('cr-toggle');
      assertEquals(3, toggles.length);
      assertEquals(!visible, toggles[0]!.disabled);
      assertEquals(!visible, toggles[1]!.disabled);
      assertEquals(!visible, toggles[2]!.disabled);
      assertEquals(visible, toggles[0]!.checked);
      assertEquals(visible, toggles[1]!.checked);
      assertFalse(toggles[2]!.checked);
      customizeModules.shadowRoot!.querySelectorAll('cr-policy-indicator')
          .forEach(el => {
            assertStyle(el, 'display', 'none');
          });
    });

    test(`toggle in ${mode} mode sets module status`, async () => {
      // Arrange.
      const customizeModules = await createCustomizeModules(!visible, [
        {id: 'foo', name: 'foo name', disabled: false},
        {id: 'bar', name: 'bar name', disabled: false},
        {id: 'baz', name: 'baz name', disabled: true},
      ]);

      // Act.
      $$<HTMLElement>(
          customizeModules,
          `#${visible ? 'hide' : 'customize'}Button`)!.click();
      customizeModules.apply();

      // Assert.
      assertEquals(!visible, handler.getArgs('setModulesVisible')[0]);
      const toggles =
          customizeModules.shadowRoot!.querySelectorAll('cr-toggle');
      assertEquals(3, toggles.length);
      assertEquals(visible, toggles[0]!.disabled);
      assertEquals(visible, toggles[1]!.disabled);
      assertEquals(visible, toggles[2]!.disabled);
      assertEquals(!visible, toggles[0]!.checked);
      assertEquals(!visible, toggles[1]!.checked);
      assertFalse(toggles[2]!.checked);
      const visibleEnabled = visible ? 'Enabled' : 'Disabled';
      const hideEnabled = !visible ? 'Enabled' : 'Disabled';
      const base = 'NewTabPage.Modules';
      assertEquals(2, metrics.count(`${base}.${hideEnabled}`));
      assertEquals(2, metrics.count(`${base}.${hideEnabled}.Customize`));
      assertEquals(0, metrics.count(`${base}.${visibleEnabled}`));
      assertEquals(0, metrics.count(`${base}.${visibleEnabled}.Customize`));
    });

    test(`policy disables UI in ${mode} mode`, async () => {
      // Act.
      loadTimeData.overrideValues({modulesVisibleManagedByPolicy: true});
      const customizeModules = await createCustomizeModules(!visible, [
        {id: 'foo', name: 'foo name', disabled: false},
        {id: 'bar', name: 'bar name', disabled: false},
        {id: 'baz', name: 'baz name', disabled: true},
      ]);

      // Assert.
      assertTrue($$(customizeModules, 'cr-radio-group')!.disabled);
      const toggles =
          customizeModules.shadowRoot!.querySelectorAll('cr-toggle');
      assertEquals(3, toggles.length);
      assertTrue(toggles[0]!.disabled);
      assertTrue(toggles[1]!.disabled);
      assertTrue(toggles[2]!.disabled);
      assertEquals(visible, toggles[0]!.checked);
      assertEquals(visible, toggles[1]!.checked);
      assertFalse(toggles[2]!.checked);
      customizeModules.shadowRoot!.querySelectorAll('cr-policy-indicator')
          .forEach(el => {
            assertNotStyle(el, 'display', 'none');
          });
    });
  });

  test('toggling disables, enables module', async () => {
    // Arrange.
    const customizeModules = await createCustomizeModules(false, [
      {id: 'foo', name: 'foo name', disabled: false},
      {id: 'bar', name: 'bar name', disabled: false},
      {id: 'baz', name: 'baz name', disabled: true},
    ]);

    // Act.
    const toggles = customizeModules.shadowRoot!.querySelectorAll('cr-toggle');
    toggles[0]!.click();
    toggles[2]!.click();
    customizeModules.apply();

    // Assert.
    const base = 'NewTabPage.Modules';
    assertEquals(2, handler.getCallCount('setModuleDisabled'));
    assertDeepEquals(['foo', true], handler.getArgs('setModuleDisabled')[0]);
    assertDeepEquals(['baz', false], handler.getArgs('setModuleDisabled')[1]);
    assertEquals(1, metrics.count(`${base}.Disabled`));
    assertEquals(1, metrics.count(`${base}.Disabled.Customize`));
    assertEquals(1, metrics.count(`${base}.Disabled`, 'foo'));
    assertEquals(1, metrics.count(`${base}.Enabled`));
    assertEquals(1, metrics.count(`${base}.Enabled.Customize`));
    assertEquals(1, metrics.count(`${base}.Enabled`, 'baz'));
  });

  test('discount toggle shows correct config', async () => {
    // Arrange.
    cartHandler.setResultFor(
        'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
    cartHandler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: true}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
      {id: 'bar', name: 'bar name', disabled: false},
    ]);
    const toggleRows =
        customizeModules.shadowRoot!.querySelectorAll<HTMLElement>(
            '.toggle-row');
    const toggleNames =
        customizeModules.shadowRoot!.querySelectorAll<HTMLElement>(
            '.toggle-name');
    const subToggleRows =
        customizeModules.shadowRoot!.querySelectorAll<HTMLElement>(
            '.discount-toggle-row');

    // Assert.
    assertEquals(3, toggleNames.length);
    assertEquals(2, toggleRows.length);
    assertEquals(1, subToggleRows.length);
    assertEquals('foo name', toggleNames[0]!.innerText);
    assertEquals(
        loadTimeData.getString('modulesCartDiscountConsentAccept'),
        toggleNames[1]!.innerText);
    assertEquals('bar name', toggleNames[2]!.innerText);
    assertTrue(subToggleRows[0]!.querySelector('cr-toggle')!.checked);
  });

  test(`discount toggle sets discount status`, async () => {
    // Arrange.
    cartHandler.setResultFor(
        'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
    cartHandler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: true}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
    ]);
    const subToggleRows =
        customizeModules.shadowRoot!.querySelectorAll('.discount-toggle-row');

    // Act.
    subToggleRows[0]!.querySelector('cr-toggle')!.click();
    customizeModules.apply();

    // Assert.
    assertEquals(1, cartHandler.getCallCount('setDiscountEnabled'));
    assertDeepEquals(false, cartHandler.getArgs('setDiscountEnabled')[0]);
  });

  test(`toggling off cart module hides discount toggle`, async () => {
    // Arrange.
    cartHandler.setResultFor(
        'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
    cartHandler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: true}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
      {id: 'bar', name: 'bar name', disabled: false},
    ]);
    const toggleRows =
        customizeModules.shadowRoot!.querySelectorAll('.toggle-row');
    const toggleNames =
        customizeModules.shadowRoot!.querySelectorAll('.toggle-name');
    const subToggleRows =
        customizeModules.shadowRoot!.querySelectorAll('.discount-toggle-row');

    // Assert.
    assertEquals(3, toggleNames.length);
    assertEquals(2, toggleRows.length);
    assertEquals(1, subToggleRows.length);
    assertTrue(toggleRows[0]!.querySelector('cr-toggle')!.checked);
    assertTrue(toggleRows[1]!.querySelector('cr-toggle')!.checked);
    assertTrue(subToggleRows[0]!.querySelector('cr-toggle')!.checked);

    // Act.
    toggleRows[0]!.querySelector('cr-toggle')!.click();
    customizeModules.$.toggleRepeat.render();

    // Assert.
    assertFalse(toggleRows[0]!.querySelector('cr-toggle')!.checked);
    assertFalse(isVisible(subToggleRows[0]!));

    // Act.
    toggleRows[0]!.querySelector('cr-toggle')!.click();
    customizeModules.$.toggleRepeat.render();

    // Assert.
    assertTrue(toggleRows[0]!.querySelector('cr-toggle')!.checked);
    assertTrue(isVisible(subToggleRows[0]!));
    assertTrue(subToggleRows[0]!.querySelector('cr-toggle')!.checked);

    // Act.
    $$<HTMLElement>(customizeModules, '#hideButton')!.click();
    customizeModules.apply();
    customizeModules.$.toggleRepeat.render();

    // Assert.
    assertFalse(toggleRows[0]!.querySelector('cr-toggle')!.checked);
    assertFalse(isVisible(subToggleRows[0]!));
  });

  test('record disable discount', async () => {
    // Arrange.
    cartHandler.setResultFor(
        'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
    cartHandler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: true}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
    ]);
    const subToggleRows =
        customizeModules.shadowRoot!.querySelectorAll('.discount-toggle-row');

    assertEquals(0, metrics.count('NewTabPage.Carts.DisableDiscount'));

    // Act.
    subToggleRows[0]!.querySelector('cr-toggle')!.click();
    customizeModules.apply();

    // Assert.
    assertDeepEquals(false, cartHandler.getArgs('setDiscountEnabled')[0]);
    assertEquals(1, metrics.count('NewTabPage.Carts.DisableDiscount'));
  });

  test('record enable discount', async () => {
    // Arrange.
    cartHandler.setResultFor(
        'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
    cartHandler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: false}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
    ]);
    const subToggleRows =
        customizeModules.shadowRoot!.querySelectorAll('.discount-toggle-row');

    assertEquals(0, metrics.count('NewTabPage.Carts.DisableDiscount'));

    // Act.
    subToggleRows[0]!.querySelector('cr-toggle')!.click();
    customizeModules.apply();

    // Assert.
    assertDeepEquals(true, cartHandler.getArgs('setDiscountEnabled')[0]);
    assertEquals(1, metrics.count('NewTabPage.Carts.EnableDiscount'));
  });

  test('discount toggle is visible', async () => {
    // Arrange.
    cartHandler.setResultFor(
        'getDiscountToggleVisible', Promise.resolve({toggleVisible: true}));
    cartHandler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: false}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
    ]);
    const subToggleRows =
        customizeModules.shadowRoot!.querySelectorAll('.discount-toggle-row');

    // Assert.
    assertEquals(1, subToggleRows.length);
  });

  test('discount toggle is not visible', async () => {
    // Arrange.
    cartHandler.setResultFor(
        'getDiscountToggleVisible', Promise.resolve({toggleVisible: false}));
    cartHandler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: false}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
    ]);
    const subToggleRows =
        customizeModules.shadowRoot!.querySelectorAll('.discount-toggle-row');

    // Assert.
    assertEquals(0, subToggleRows.length);
  });

  test('should show modules after loaded', async () => {
    const customizeModules = await createCustomizeModules(true, [
      {id: 'foo', name: 'foo name', disabled: false},
    ]);
    assertNotStyle(customizeModules.$.container, 'display', 'none');
  });
});
