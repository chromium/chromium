// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, Module, ModuleRegistry, ModulesElement, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
import {fakeMetricsPrivate, MetricsTracker} from '../metrics_test_support.js';
import {assertNotStyle, assertStyle, createMock} from '../test_support.js';

/** @return {!TestBrowserProxy} */
function installMockHandler() {
  const {mock, callTracker} = createMock(newTabPage.mojom.PageHandlerRemote);
  NewTabPageProxy.setInstance(mock, new newTabPage.mojom.PageCallbackRouter());
  return callTracker;
}

/** @return {!TestBrowserProxy} */
function installMockModuleRegistry() {
  const {mock, callTracker} = createMock(ModuleRegistry);
  ModuleRegistry.setInstance(mock);
  return callTracker;
}

suite('NewTabPageModulesModulesTest', () => {
  /** @type {!TestBrowserProxy} */
  let handler;

  /** @type {!newTabPage.mojom.PageRemote} */
  let callbackRouterRemote;

  /** @type {!MetricsTracker} */
  let metrics;

  /** @type {!TestBrowserProxy} */
  let moduleRegistry;

  setup(async () => {
    document.body.innerHTML = '';
    metrics = fakeMetricsPrivate();
    handler = installMockHandler();
    moduleRegistry = installMockModuleRegistry();
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  /**
   * @param {!Array<!Module>} modules
   * @return {!Promise<!ModulesElement>}
   */
  async function createModulesElement(modules) {
    const modulesPromise = Promise.resolve(modules);
    moduleRegistry.setResultFor('initializeModules', modulesPromise);
    const modulesElement = new ModulesElement();
    document.body.appendChild(modulesElement);
    await modulesPromise;
    $$(modulesElement, '#modules').render();
    return modulesElement;
  }

  [true, false].forEach(visible => {
    test(`modules rendered if visibility ${visible}`, async () => {
      // Act.
      const modulesElement = await createModulesElement([
        {
          descriptor: {id: 'foo'},
          element: document.createElement('div'),
        },
        {
          descriptor: {id: 'bar'},
          element: document.createElement('div'),
        }
      ]);
      callbackRouterRemote.setDisabledModules(!visible, ['bar']);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      const moduleWrappers =
          modulesElement.shadowRoot.querySelectorAll('ntp-module-wrapper');
      assertEquals(2, moduleWrappers.length);
      if (visible) {
        assertNotStyle(moduleWrappers[0], 'display', 'none');
      } else {
        assertStyle(moduleWrappers[0], 'display', 'none');
      }
      assertStyle(moduleWrappers[1], 'display', 'none');
      const histogram = 'NewTabPage.Modules.EnabledOnNTPLoad';
      assertEquals(1, metrics.count(`${histogram}.foo`, visible));
      assertEquals(1, metrics.count(`${histogram}.bar`, false));
      assertEquals(
          1, metrics.count('NewTabPage.Modules.VisibleOnNTPLoad', visible));
      assertEquals(1, handler.getCallCount('updateDisabledModules'));
      assertEquals(1, handler.getCallCount('onModulesLoadedWithData'));
    });
  });

  test('modules can be dismissed and restored', async () => {
    // Arrange.
    let restoreCalled = false;

    // Act.
    const modulesElement = await createModulesElement([
      {
        descriptor: {id: 'foo'},
        element: document.createElement('div'),
      },
    ]);
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    const moduleWrappers =
        modulesElement.shadowRoot.querySelectorAll('ntp-module-wrapper');
    assertEquals(1, moduleWrappers.length);
    assertNotStyle(moduleWrappers[0], 'display', 'none');
    assertFalse($$(modulesElement, '#removeModuleToast').open);

    // Act.
    moduleWrappers[0].dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: 'Foo',
        restoreCallback: () => {
          restoreCalled = true;
        },
      },
    }));

    // Assert.
    assertStyle(moduleWrappers[0], 'display', 'none');
    assertTrue($$(modulesElement, '#removeModuleToast').open);
    assertEquals(
        'Foo',
        $$(modulesElement, '#removeModuleToastMessage').textContent.trim());
    assertEquals(1, handler.getCallCount('onDismissModule'));
    assertEquals('foo', handler.getArgs('onDismissModule')[0]);
    assertFalse(restoreCalled);

    // Act.
    $$(modulesElement, '#undoRemoveModuleButton').click();

    // Assert.
    assertNotStyle(moduleWrappers[0], 'display', 'none');
    assertFalse($$(modulesElement, '#removeModuleToast').open);
    assertTrue(restoreCalled);
    assertEquals('foo', handler.getArgs('onRestoreModule')[0]);
  });

  test('modules can be disabled and restored', async () => {
    // Arrange.
    let restoreCalled = false;

    // Act.
    const modulesElement = await createModulesElement([{
      descriptor: {
        id: 'foo',
        name: 'bar',
      },
      element: document.createElement('div'),
    }]);
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    const moduleWrappers =
        modulesElement.shadowRoot.querySelectorAll('ntp-module-wrapper');
    assertEquals(1, moduleWrappers.length);
    assertNotStyle(moduleWrappers[0], 'display', 'none');
    assertFalse($$(modulesElement, '#removeModuleToast').open);

    // Act.
    moduleWrappers[0].dispatchEvent(new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: 'Foo',
        restoreCallback: () => {
          restoreCalled = true;
        },
      },
    }));

    // Assert.
    assertDeepEquals(['foo', true], handler.getArgs('setModuleDisabled')[0]);

    // Act.
    callbackRouterRemote.setDisabledModules(false, ['foo']);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertStyle(moduleWrappers[0], 'display', 'none');
    assertTrue($$(modulesElement, '#removeModuleToast').open);
    assertEquals(
        'Foo',
        $$(modulesElement, '#removeModuleToastMessage').textContent.trim());
    assertEquals(1, metrics.count('NewTabPage.Modules.Disabled', 'foo'));
    assertEquals(
        1, metrics.count('NewTabPage.Modules.Disabled.ModuleRequest', 'foo'));
    assertFalse(restoreCalled);

    // Act.
    $$(modulesElement, '#undoRemoveModuleButton').click();

    // Assert.
    assertDeepEquals(['foo', false], handler.getArgs('setModuleDisabled')[1]);

    // Act.
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertNotStyle(moduleWrappers[0], 'display', 'none');
    assertFalse($$(modulesElement, '#removeModuleToast').open);
    assertTrue(restoreCalled);
    assertEquals(1, metrics.count('NewTabPage.Modules.Enabled', 'foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Enabled.Toast', 'foo'));

    // Act.
    window.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'z',
      ctrlKey: true,
    }));

    // Assert: no crash.
  });
});
