// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Module} from 'chrome://new-tab-page/lazy_load.js';
import {ModuleDescriptor, ModuleRegistry, ModulesElement} from 'chrome://new-tab-page/lazy_load.js';
import {NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';

import {assertNotStyle, assertStyle, createElement, initNullModule, installMock} from '../test_support.js';

suite('NewTabPageModulesModulesTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let callbackRouterRemote: PageRemote;
  let metrics: MetricsTracker;
  let moduleRegistry: TestMock<ModuleRegistry>;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      modulesRedesignedEnabled: false,
    });
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = fakeMetricsPrivate();
    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    moduleRegistry = installMock(ModuleRegistry);
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function createModulesElement(modules: Module[]):
      Promise<ModulesElement> {
    const modulesPromise = Promise.resolve(modules);
    moduleRegistry.setResultFor('initializeModulesHavingIds', modulesPromise);
    const modulesElement = new ModulesElement();
    document.body.appendChild(modulesElement);
    await modulesPromise;
    return modulesElement;
  }

  [true, false].forEach(visible => {
    test(`modules rendered if visibility ${visible}`, async () => {
      // Arrange.
      const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
      const barDescriptor = new ModuleDescriptor('bar', initNullModule);
      const bazDescriptor = new ModuleDescriptor('baz', initNullModule);
      handler.setResultFor('getModulesIdNames', {
        data: [
          {id: fooDescriptor.id, name: fooDescriptor.id},
          {id: barDescriptor.id, name: barDescriptor.id},
          {id: bazDescriptor.id, name: bazDescriptor.id},
        ],
      });

      // Act.
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          elements: [createElement()],
        },
        {
          descriptor: barDescriptor,
          elements: [createElement()],
        },
      ]);
      callbackRouterRemote.setDisabledModules(
          !visible, [barDescriptor.id, bazDescriptor.id]);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      const moduleWrappers =
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
      const moduleWrapperContainers =
          modulesElement.shadowRoot!.querySelectorAll('.module-container');
      assertEquals(2, moduleWrappers.length);
      assertEquals(2, moduleWrapperContainers.length);
      assertNotStyle(moduleWrappers[0]!, 'display', 'none');
      if (visible) {
        assertNotStyle(moduleWrapperContainers[0]!, 'display', 'none');
      } else {
        assertStyle(moduleWrapperContainers[0]!, 'display', 'none');
      }
      assertNotStyle(moduleWrappers[1]!, 'display', 'none');
      assertStyle(moduleWrapperContainers[1]!, 'display', 'none');
      assertNotStyle(moduleWrappers[0]!, 'cursor', 'grab');
      assertNotStyle(moduleWrappers[1]!, 'cursor', 'grab');
      const histogram = 'NewTabPage.Modules.EnabledOnNTPLoad';
      assertEquals(1, metrics.count(`${histogram}.foo`, visible));
      assertEquals(1, metrics.count(`${histogram}.bar`, false));
      assertEquals(1, metrics.count(`${histogram}.baz`, false));
      assertEquals(
          1, metrics.count('NewTabPage.Modules.VisibleOnNTPLoad', visible));
      assertEquals(1, handler.getCallCount('updateDisabledModules'));
      assertEquals(1, handler.getCallCount('onModulesLoadedWithData'));
    });
  });

  test('single module multiple element instances', async () => {
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    const barDescriptor = new ModuleDescriptor('bar', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
        {id: barDescriptor.id, name: barDescriptor.id},
      ],
    });

    const modulesElement = await createModulesElement([
      {
        descriptor: fooDescriptor,
        elements: Array(3).fill(0).map(_ => createElement()),
      },
      {
        descriptor: barDescriptor,
        elements: [createElement()],
      },
    ]);
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    const moduleContainers =
        modulesElement.shadowRoot!.querySelectorAll('.module-container');
    assertEquals(4, moduleContainers.length);
    const histogram = 'NewTabPage.Modules.EnabledOnNTPLoad';
    assertEquals(1, metrics.count(`${histogram}.foo`, true));
    assertEquals(1, metrics.count(`${histogram}.bar`, true));
    assertEquals(1, metrics.count('NewTabPage.Modules.VisibleOnNTPLoad', true));
    assertEquals(1, handler.getCallCount('updateDisabledModules'));
    assertEquals(1, handler.getCallCount('onModulesLoadedWithData'));
  });

  test('modules can be dismissed with no restore action', async () => {
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
      ],
    });

    // Act.
    const modulesElement = await createModulesElement([
      {
        descriptor: fooDescriptor,
        elements: [createElement()],
      },
    ]);
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    const moduleWrappers =
        modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
    const moduleWrapperContainers =
        modulesElement.shadowRoot!.querySelectorAll('.module-container');
    assertEquals(1, moduleWrappers.length);
    assertEquals(1, moduleWrapperContainers.length);
    assertNotStyle(moduleWrappers[0]!, 'display', 'none');
    assertNotStyle(moduleWrapperContainers[0]!, 'display', 'none');
    assertFalse(modulesElement.$.removeModuleToast.open);

    // Act.
    moduleWrappers[0]!.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: 'Foo',
      },
    }));
    await waitAfterNextRender(modulesElement);

    // Assert.
    assertNotStyle(moduleWrappers[0]!, 'display', 'none');
    assertStyle(moduleWrapperContainers[0]!, 'display', 'none');
    assertTrue(modulesElement.$.removeModuleToast.open);
    assertEquals(
        'Foo', modulesElement.$.removeModuleToastMessage.textContent!.trim());
    assertEquals(1, handler.getCallCount('onDismissModule'));
    assertEquals('foo', handler.getArgs('onDismissModule')[0]);
    assertEquals(
        null,
        modulesElement.shadowRoot!.querySelector('#undoRemoveModuleButton'));
  });

  test('modules can be dismissed and restored', async () => {
    // Arrange.
    let restoreCalled = false;
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
      ],
    });

    // Act.
    const modulesElement = await createModulesElement([
      {
        descriptor: fooDescriptor,
        elements: [createElement()],
      },
    ]);
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    const moduleWrappers =
        modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
    const moduleWrapperContainers =
        modulesElement.shadowRoot!.querySelectorAll('.module-container');
    assertEquals(1, moduleWrappers.length);
    assertEquals(1, moduleWrapperContainers.length);
    assertNotStyle(moduleWrappers[0]!, 'display', 'none');
    assertNotStyle(moduleWrapperContainers[0]!, 'display', 'none');
    assertFalse(modulesElement.$.removeModuleToast.open);

    // Act.
    moduleWrappers[0]!.dispatchEvent(new CustomEvent('dismiss-module', {
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
    assertNotStyle(moduleWrappers[0]!, 'display', 'none');
    assertStyle(moduleWrapperContainers[0]!, 'display', 'none');
    assertTrue(modulesElement.$.removeModuleToast.open);
    assertEquals(
        'Foo', modulesElement.$.removeModuleToastMessage.textContent!.trim());
    assertEquals(1, handler.getCallCount('onDismissModule'));
    assertEquals('foo', handler.getArgs('onDismissModule')[0]);
    assertFalse(restoreCalled);

    // Act.
    await waitAfterNextRender(modulesElement);
    const undoRemoveModuleButton =
        modulesElement.shadowRoot!.querySelector<HTMLElement>(
            '#undoRemoveModuleButton');
    assertTrue(!!undoRemoveModuleButton);
    undoRemoveModuleButton.click();

    // Assert.
    assertNotStyle(moduleWrappers[0]!, 'display', 'none');
    assertNotStyle(moduleWrapperContainers[0]!, 'display', 'none');
    assertFalse(modulesElement.$.removeModuleToast.open);
    assertTrue(restoreCalled);
    assertEquals('foo', handler.getArgs('onRestoreModule')[0]);
  });

  test('modules can be disabled and restored', async () => {
    // Arrange.
    let restoreCalled = false;
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
      ],
    });

    // Act.
    const modulesElement = await createModulesElement([{
      descriptor: fooDescriptor,
      elements: [createElement()],
    }]);
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    const moduleWrappers =
        modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
    const moduleWrapperContainers =
        modulesElement.shadowRoot!.querySelectorAll('.module-container');
    assertEquals(1, moduleWrappers.length);
    assertEquals(1, moduleWrapperContainers.length);
    assertNotStyle(moduleWrappers[0]!, 'display', 'none');
    assertNotStyle(moduleWrapperContainers[0]!, 'display', 'none');
    assertFalse(modulesElement.$.removeModuleToast.open);

    // Act.
    moduleWrappers[0]!.dispatchEvent(new CustomEvent('disable-module', {
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
    assertNotStyle(moduleWrappers[0]!, 'display', 'none');
    assertStyle(moduleWrapperContainers[0]!, 'display', 'none');
    assertTrue(modulesElement.$.removeModuleToast.open);
    assertEquals(
        'Foo', modulesElement.$.removeModuleToastMessage.textContent!.trim());
    assertEquals(1, metrics.count('NewTabPage.Modules.Disabled', 'foo'));
    assertEquals(
        1, metrics.count('NewTabPage.Modules.Disabled.ModuleRequest', 'foo'));
    assertFalse(restoreCalled);

    // Act.
    await waitAfterNextRender(modulesElement);
    const undoRemoveModuleButton =
        modulesElement.shadowRoot!.querySelector<HTMLElement>(
            '#undoRemoveModuleButton');
    assertTrue(!!undoRemoveModuleButton);
    undoRemoveModuleButton.click();

    // Assert.
    assertDeepEquals(['foo', false], handler.getArgs('setModuleDisabled')[1]);

    // Act.
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertNotStyle(moduleWrappers[0]!, 'display', 'none');
    assertNotStyle(moduleWrapperContainers[0]!, 'display', 'none');
    assertFalse(modulesElement.$.removeModuleToast.open);
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

  test('record number of loaded modules', async () => {
    // Arrange.
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    const barDescriptor = new ModuleDescriptor('bar', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
        {id: barDescriptor.id, name: barDescriptor.id},
      ],
    });
    await createModulesElement([
      {
        descriptor: fooDescriptor,
        elements: [createElement()],
      },
      {
        descriptor: barDescriptor,
        elements: [createElement()],
      },
    ]);

    // Assert.
    assertEquals(
        1, metrics.count('NewTabPage.Modules.LoadedModulesCount', 2),
        'Rendered count is 2 should be recored once');
  });

  test('record module loaded with other modules', async () => {
    // Arrange.
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    const barDescriptor = new ModuleDescriptor('bar', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
        {id: barDescriptor.id, name: barDescriptor.id},
      ],
    });
    await createModulesElement([
      {
        descriptor: fooDescriptor,
        elements: [createElement()],
      },
      {
        descriptor: barDescriptor,
        elements: [createElement()],
      },
    ]);

    // Assert.
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadedWith.foo', 'bar'));
    assertEquals(0, metrics.count('NewTabPage.Modules.LoadedWith.foo', 'foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadedWith.bar', 'foo'));
    assertEquals(0, metrics.count('NewTabPage.Modules.LoadedWith.bar', 'bar'));
  });

  suite('modules drag and drop', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesDragAndDropEnabled: true,
      });
    });

    test('drag first module to third then second position', async () => {
      // Arrange.
      const moduleArray = [];
      for (let i = 0; i < 3; ++i) {
        const module = createElement();
        moduleArray.push(module);
      }
      const fooDescriptor =
          new ModuleDescriptor('foo', async () => createElement());
      const barDescriptor =
          new ModuleDescriptor('bar', async () => createElement());
      const fooBarDescriptor =
          new ModuleDescriptor('foo bar', async () => createElement());

      handler.setResultFor('getModulesIdNames', {
        data: [
          {id: fooDescriptor.id, name: fooDescriptor.id},
          {id: barDescriptor.id, name: barDescriptor.id},
          {id: fooBarDescriptor.id, name: fooBarDescriptor.id},
        ],
      });
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          elements: [moduleArray[0]!],
        },
        {
          descriptor: barDescriptor,
          elements: [moduleArray[1]!],
        },
        {
          descriptor: fooBarDescriptor,
          elements: [moduleArray[2]!],
        },
      ]);
      callbackRouterRemote.setDisabledModules(false, []);
      await callbackRouterRemote.$.flushForTesting();

      let moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      const firstModule = moduleWrappers[0];
      const secondModule = moduleWrappers[1];
      const thirdModule = moduleWrappers[2];
      assertTrue(!!firstModule);
      assertTrue(!!secondModule);
      assertTrue(!!thirdModule);
      assertStyle(firstModule, 'cursor', 'grab');
      assertStyle(secondModule, 'cursor', 'grab');
      assertStyle(thirdModule, 'cursor', 'grab');

      const firstPositionRect = moduleWrappers[0]!.getBoundingClientRect();
      const secondPositionRect = moduleWrappers[1]!.getBoundingClientRect();
      const thirdPositionRect = moduleWrappers[2]!.getBoundingClientRect();

      const startX = firstPositionRect.x + firstPositionRect.width / 2;
      const startY = firstPositionRect.y + firstPositionRect.height / 2;
      let changeX = 10;
      let changeY = 2 * firstPositionRect.height;

      // Act.
      firstModule.dispatchEvent(new MouseEvent('mousedown', {
        clientX: startX,
        clientY: startY,
      }));

      document.dispatchEvent(new MouseEvent('mousemove', {
        clientX: startX + changeX,
        clientY: startY + changeY,
      }));

      // Assert.
      assertEquals(
          firstPositionRect.x + changeX, firstModule.getBoundingClientRect().x);
      assertEquals(
          firstPositionRect.y + changeY, firstModule.getBoundingClientRect().y);

      // Act.
      thirdModule.dispatchEvent(new MouseEvent('mouseover'));

      // Assert.
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(secondModule));
      assertEquals(1, moduleWrappers.indexOf(thirdModule));
      assertEquals(2, moduleWrappers.indexOf(firstModule));
      assertEquals(
          secondPositionRect.x, secondModule.getBoundingClientRect().x);
      assertEquals(
          secondPositionRect.y, secondModule.getBoundingClientRect().y);
      assertEquals(thirdPositionRect.x, thirdModule.getBoundingClientRect().x);
      assertEquals(thirdPositionRect.y, thirdModule.getBoundingClientRect().y);

      assertEquals(1, secondModule.getAnimations().length);
      assertEquals(1, thirdModule.getAnimations().length);
      secondModule.getAnimations()[0]!.finish();
      thirdModule.getAnimations()[0]!.finish();
      assertEquals(0, secondModule.getAnimations().length);
      assertEquals(0, thirdModule.getAnimations().length);

      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(secondModule));
      assertEquals(1, moduleWrappers.indexOf(thirdModule));
      assertEquals(2, moduleWrappers.indexOf(firstModule));
      assertEquals(firstPositionRect.x, secondModule.getBoundingClientRect().x);
      assertEquals(firstPositionRect.y, secondModule.getBoundingClientRect().y);
      assertEquals(secondPositionRect.x, thirdModule.getBoundingClientRect().x);
      assertEquals(secondPositionRect.y, thirdModule.getBoundingClientRect().y);

      // Act.
      changeX += 5;
      changeY -= firstPositionRect.height;
      document.dispatchEvent(new MouseEvent('mousemove', {
        clientX: startX + changeX,
        clientY: startY + changeY,
      }));

      // Assert.
      assertEquals(
          firstPositionRect.x + changeX, firstModule.getBoundingClientRect().x);
      assertEquals(
          firstPositionRect.y + changeY, firstModule.getBoundingClientRect().y);

      // Act.
      thirdModule.dispatchEvent(new MouseEvent('mouseover'));

      // Assert.
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(secondModule));
      assertEquals(1, moduleWrappers.indexOf(firstModule));
      assertEquals(2, moduleWrappers.indexOf(thirdModule));
      assertEquals(firstPositionRect.x, secondModule.getBoundingClientRect().x);
      assertEquals(firstPositionRect.y, secondModule.getBoundingClientRect().y);
      assertEquals(secondPositionRect.x, thirdModule.getBoundingClientRect().x);
      assertEquals(secondPositionRect.y, thirdModule.getBoundingClientRect().y);

      assertEquals(1, secondModule.getAnimations().length);
      assertEquals(1, thirdModule.getAnimations().length);
      secondModule.getAnimations()[0]!.finish();
      thirdModule.getAnimations()[0]!.finish();
      assertEquals(0, secondModule.getAnimations().length);
      assertEquals(0, thirdModule.getAnimations().length);

      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(secondModule));
      assertEquals(1, moduleWrappers.indexOf(firstModule));
      assertEquals(2, moduleWrappers.indexOf(thirdModule));
      assertEquals(firstPositionRect.x, secondModule.getBoundingClientRect().x);
      assertEquals(firstPositionRect.y, secondModule.getBoundingClientRect().y);
      assertEquals(thirdPositionRect.x, thirdModule.getBoundingClientRect().x);
      assertEquals(thirdPositionRect.y, thirdModule.getBoundingClientRect().y);

      // Act.
      document.dispatchEvent(new MouseEvent('mouseup'));

      // Assert.
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(1, moduleWrappers.indexOf(firstModule));

      assertEquals(1, firstModule.getAnimations().length);
      firstModule.getAnimations()[0]!.finish();
      assertEquals(0, firstModule.getAnimations().length);

      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(1, moduleWrappers.indexOf(firstModule));
      assertEquals(secondPositionRect.x, firstModule.getBoundingClientRect().x);
      assertEquals(secondPositionRect.y, firstModule.getBoundingClientRect().y);
    });

    test('hidden module goes to end of NTP when layout changes', async () => {
      // Arrange.
      let restoreCalled = false;
      const moduleArray = [];
      for (let i = 0; i < 3; ++i) {
        const module = createElement();
        moduleArray.push(module);
      }
      const fooDescriptor =
          new ModuleDescriptor('foo', async () => createElement());
      const barDescriptor =
          new ModuleDescriptor('bar', async () => createElement());
      const fooBarDescriptor =
          new ModuleDescriptor('foo bar', async () => createElement());

      handler.setResultFor('getModulesIdNames', {
        data: [
          {id: fooDescriptor.id, name: fooDescriptor.id},
          {id: barDescriptor.id, name: barDescriptor.id},
          {id: fooBarDescriptor.id, name: fooBarDescriptor.id},
        ],
      });
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          elements: [moduleArray[0]!],
        },
        {
          descriptor: barDescriptor,
          elements: [moduleArray[1]!],
        },
        {
          descriptor: fooBarDescriptor,
          elements: [moduleArray[2]!],
        },
      ]);
      callbackRouterRemote.setDisabledModules(false, []);
      await callbackRouterRemote.$.flushForTesting();

      let moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      const tallModule = moduleWrappers[0];
      const shortModule1 = moduleWrappers[1];
      const shortModule2 = moduleWrappers[2];
      assertTrue(!!tallModule);
      assertTrue(!!shortModule1);
      assertTrue(!!shortModule2);
      assertStyle(tallModule, 'cursor', 'grab');
      assertStyle(shortModule1, 'cursor', 'grab');
      assertStyle(shortModule2, 'cursor', 'grab');

      // Act.
      moduleWrappers[1]!.dispatchEvent(new CustomEvent('disable-module', {
        bubbles: true,
        composed: true,
        detail: {
          message: 'Bar',
          restoreCallback: () => {
            restoreCalled = true;
          },
        },
      }));

      // Assert.
      assertDeepEquals(['bar', true], handler.getArgs('setModuleDisabled')[0]);

      // Act.
      callbackRouterRemote.setDisabledModules(false, ['bar']);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      assertFalse(restoreCalled);
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(tallModule));
      assertEquals(1, moduleWrappers.indexOf(shortModule1));
      assertEquals(2, moduleWrappers.indexOf(shortModule2));

      // Act.
      tallModule.dispatchEvent(new MouseEvent('mousedown'));
      document.dispatchEvent(new MouseEvent('mousemove'));

      // Act.
      shortModule2.dispatchEvent(new MouseEvent('mouseover'));

      // Assert.
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(shortModule2));
      assertEquals(1, moduleWrappers.indexOf(tallModule));
      assertEquals(2, moduleWrappers.indexOf(shortModule1));

      // // Act.
      await waitAfterNextRender(modulesElement);
      const undoRemoveModuleButton =
          modulesElement.shadowRoot!.querySelector<HTMLElement>(
              '#undoRemoveModuleButton');
      assertTrue(!!undoRemoveModuleButton);
      undoRemoveModuleButton.click();

      // Assert.
      assertDeepEquals(['bar', false], handler.getArgs('setModuleDisabled')[1]);

      // Act.
      callbackRouterRemote.setDisabledModules(false, []);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(shortModule2));
      assertEquals(1, moduleWrappers.indexOf(tallModule));
      assertEquals(2, moduleWrappers.indexOf(shortModule1));
    });
  });
});
