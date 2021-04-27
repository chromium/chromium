// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {$$, ChromeCartProxy, ModuleDescriptor, ModuleRegistry, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {assertNotStyle, assertStyle} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {isVisible} from 'chrome://test/test_util.m.js';

/**
 * @param {!HTMLElement} host
 * @param {string} selector
 * @return {!Array<!HTMLElement>}
 */
function queryAll(host, selector) {
  return Array.from(host.shadowRoot.querySelectorAll(selector));
}

suite('NewTabPageCustomizeModulesTest', () => {
  /**
   * @implements {newTabPage.mojom.PageHandlerRemote}
   * @extends {TestBrowserProxy}
   */
  let handler;

  /** @type {newTabPage.mojom.PageHandlerRemote} */
  let callbackRouterRemote;

  /**
   * @implements {ModuleRegistry}
   * @extends {TestBrowserProxy}
   */
  let moduleRegistry;

  /** @type {MetricsTracker} */
  let metrics;

  /**
   * @implements {ChromeCartProxy}
   * @extends {TestBrowserProxy}
   */
  let cartTestProxy;

  /**
   * @param {boolean} allDisabled
   * @param {!Array<!{id: string, name: string, disabled: boolean}>} modules
   * @return {!Promise<!HTMLElement>}
   */
  async function createCustomizeModules(allDisabled, modules) {
    moduleRegistry.setResultFor(
        'getDescriptors',
        modules.map(
            ({id, name}) => new ModuleDescriptor(id, name, 1, () => null)));
    const customizeModules = document.createElement('ntp-customize-modules');
    document.body.appendChild(customizeModules);
    callbackRouterRemote.setDisabledModules(
        allDisabled,
        modules.filter(({disabled}) => disabled).map(({id}) => id));
    await callbackRouterRemote.$.flushForTesting();
    return customizeModules;
  }

  setup(() => {
    PolymerTest.clearBody();

    handler = TestBrowserProxy.fromClass(newTabPage.mojom.PageHandlerRemote);
    const callbackRouter = new newTabPage.mojom.PageCallbackRouter();
    NewTabPageProxy.setInstance(handler, callbackRouter);
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
    moduleRegistry = TestBrowserProxy.fromClass(ModuleRegistry);
    ModuleRegistry.setInstance(moduleRegistry);
    metrics = fakeMetricsPrivate();
    loadTimeData.overrideValues({modulesVisibleManagedByPolicy: false});
    cartTestProxy = TestBrowserProxy.fromClass(ChromeCartProxy);
    cartTestProxy.handler =
        TestBrowserProxy.fromClass(chromeCart.mojom.CartHandlerRemote);
    ChromeCartProxy.setInstance(cartTestProxy);
    cartTestProxy.handler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: false}));
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
      const names = queryAll(customizeModules, '.toggle-name');
      assertEquals(3, names.length);
      assertEquals('foo name', names[0].textContent.trim());
      assertEquals('bar name', names[1].textContent.trim());
      assertEquals('baz name', names[2].textContent.trim());
      const toggles = queryAll(customizeModules, 'cr-toggle');
      assertEquals(3, toggles.length);
      assertEquals(!visible, toggles[0].disabled);
      assertEquals(!visible, toggles[1].disabled);
      assertEquals(!visible, toggles[2].disabled);
      assertEquals(visible, toggles[0].checked);
      assertEquals(visible, toggles[1].checked);
      assertFalse(toggles[2].checked);
      queryAll(customizeModules, 'cr-policy-indicator').forEach((el) => {
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
      $$(customizeModules, `#${visible ? 'hide' : 'customize'}Button`).click();
      customizeModules.apply();

      // Assert.
      assertEquals(!visible, handler.getArgs('setModulesVisible')[0]);
      const toggles = queryAll(customizeModules, 'cr-toggle');
      assertEquals(3, toggles.length);
      assertEquals(visible, toggles[0].disabled);
      assertEquals(visible, toggles[1].disabled);
      assertEquals(visible, toggles[2].disabled);
      assertEquals(!visible, toggles[0].checked);
      assertEquals(!visible, toggles[1].checked);
      assertFalse(toggles[2].checked);
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
      assertTrue($$(customizeModules, 'cr-radio-group').disabled);
      const toggles = queryAll(customizeModules, 'cr-toggle');
      assertEquals(3, toggles.length);
      assertTrue(toggles[0].disabled);
      assertTrue(toggles[1].disabled);
      assertTrue(toggles[2].disabled);
      assertEquals(visible, toggles[0].checked);
      assertEquals(visible, toggles[1].checked);
      assertFalse(toggles[2].checked);
      queryAll(customizeModules, 'cr-policy-indicator').forEach((el) => {
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
    const toggles = queryAll(customizeModules, 'cr-toggle');
    toggles[0].click();
    toggles[2].click();
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
    loadTimeData.overrideValues({ruleBasedDiscountEnabled: true});
    cartTestProxy.handler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: true}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
      {id: 'bar', name: 'bar name', disabled: false},
    ]);
    const toggleRows = queryAll(customizeModules, '.toggle-row');
    const toggleNames = queryAll(customizeModules, '.toggle-name');
    const subToggleRows = queryAll(customizeModules, '.discount-toggle-row');

    // Assert.
    assertEquals(3, toggleNames.length);
    assertEquals(2, toggleRows.length);
    assertEquals(1, subToggleRows.length);
    assertEquals('foo name', toggleNames[0].innerText);
    assertEquals(
        loadTimeData.getString('modulesCartDiscountConsentAccept'),
        toggleNames[1].innerText);
    assertEquals('bar name', toggleNames[2].innerText);
    assertTrue(subToggleRows[0].querySelector('cr-toggle').checked);
  });

  test(`discount toggle sets discount status`, async () => {
    // Arrange.
    loadTimeData.overrideValues({ruleBasedDiscountEnabled: true});
    cartTestProxy.handler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: true}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
    ]);
    const subToggleRows = queryAll(customizeModules, '.discount-toggle-row');

    // Act.
    subToggleRows[0].querySelector('cr-toggle').click();
    customizeModules.apply();

    // Assert.
    assertEquals(1, cartTestProxy.handler.getCallCount('setDiscountEnabled'));
    assertDeepEquals(
        false, cartTestProxy.handler.getArgs('setDiscountEnabled')[0]);
  });

  test(`toggling off cart module hides discount toggle`, async () => {
    // Arrange.
    loadTimeData.overrideValues({ruleBasedDiscountEnabled: true});
    cartTestProxy.handler.setResultFor(
        'getDiscountEnabled', Promise.resolve({enabled: true}));
    const customizeModules = await createCustomizeModules(false, [
      {id: 'chrome_cart', name: 'foo name', disabled: false},
      {id: 'bar', name: 'bar name', disabled: false},
    ]);
    const toggleRows = queryAll(customizeModules, '.toggle-row');
    const toggleNames = queryAll(customizeModules, '.toggle-name');
    const subToggleRows = queryAll(customizeModules, '.discount-toggle-row');

    // Assert.
    assertEquals(3, toggleNames.length);
    assertEquals(2, toggleRows.length);
    assertEquals(1, subToggleRows.length);
    assertTrue(toggleRows[0].querySelector('cr-toggle').checked);
    assertTrue(toggleRows[1].querySelector('cr-toggle').checked);
    assertTrue(subToggleRows[0].querySelector('cr-toggle').checked);

    // Act.
    toggleRows[0].querySelector('cr-toggle').click();
    customizeModules.$.toggleRepeat.render();

    // Assert.
    assertFalse(toggleRows[0].querySelector('cr-toggle').checked);
    assertFalse(isVisible(subToggleRows[0]));

    // Act.
    toggleRows[0].querySelector('cr-toggle').click();
    customizeModules.$.toggleRepeat.render();

    // Assert.
    assertTrue(toggleRows[0].querySelector('cr-toggle').checked);
    assertTrue(isVisible(subToggleRows[0]));
    assertTrue(subToggleRows[0].querySelector('cr-toggle').checked);

    // Act.
    $$(customizeModules, '#hideButton').click();
    customizeModules.apply();
    customizeModules.$.toggleRepeat.render();

    // Assert.
    assertFalse(toggleRows[0].querySelector('cr-toggle').checked);
    assertFalse(isVisible(subToggleRows[0]));
  });
});
