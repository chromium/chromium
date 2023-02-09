// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {Module, ModuleDescriptor, ModuleDescriptorV2, ModuleHeight, ModuleRegistry, ModulesElement, ModuleWrapperElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote, PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {assertNotStyle, assertStyle, capture, createElement, initNullModule, installMock, render} from '../test_support.js';

suite('NewTabPageModulesModulesTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let callbackRouterRemote: PageRemote;
  let metrics: MetricsTracker;
  let moduleRegistry: TestMock<ModuleRegistry>;

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
    moduleRegistry.setResultFor('initializeModules', modulesPromise);
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
      moduleRegistry.setResultFor(
          'getDescriptors', [fooDescriptor, barDescriptor, bazDescriptor]);
      // Act.
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          element: createElement(),
        },
        {
          descriptor: barDescriptor,
          element: createElement(),
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

  suite('modules first run experience', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesFirstRunExperienceEnabled: true,
      });
    });

    [true, false].forEach(visible => {
      test(`first run experience shows if modules ${visible}`, async () => {
        // Arrange.
        const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
        const barDescriptor = new ModuleDescriptor('bar', initNullModule);
        const bazDescriptor = new ModuleDescriptor('baz', initNullModule);
        moduleRegistry.setResultFor(
            'getDescriptors', [fooDescriptor, barDescriptor, bazDescriptor]);
        // Act.
        const modulesElement = await createModulesElement([
          {
            descriptor: fooDescriptor,
            element: createElement(),
          },
          {
            descriptor: barDescriptor,
            element: createElement(),
          },
        ]);
        callbackRouterRemote.setDisabledModules(
            !visible, [barDescriptor.id, bazDescriptor.id]);
        callbackRouterRemote.setModulesFreVisibility(visible);
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
        assertEquals(1, handler.getCallCount('updateModulesFreVisibility'));
      });
    });

    test(`clicking customize chrome link sends event`, async () => {
      // Arrange.
      const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
      moduleRegistry.setResultFor('getDescriptors', [fooDescriptor]);
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          element: createElement(),
        },
      ]);
      callbackRouterRemote.setModulesFreVisibility(true);
      callbackRouterRemote.setDisabledModules(false, []);
      await callbackRouterRemote.$.flushForTesting();
      const customizeModule = capture(modulesElement, 'customize-module');
      render(modulesElement);

      // Act
      $$<HTMLElement>(modulesElement, '#customizeChromeLink')!.click();

      // Assert.
      assertTrue(customizeModule.received);
    });

    test(`fre buttons work`, async () => {
      // Arrange.
      const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
      const barDescriptor = new ModuleDescriptor('bar', initNullModule);

      moduleRegistry.setResultFor(
          'getDescriptors', [fooDescriptor, barDescriptor]);
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          element: createElement(),
        },
        {
          descriptor: barDescriptor,
          element: createElement(),
        },
      ]);
      callbackRouterRemote.setModulesFreVisibility(true);
      callbackRouterRemote.setDisabledModules(false, []);
      const moduleWrappers =
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
      moduleWrappers[0]!.dispatchEvent(new Event('detect-impression'));
      moduleWrappers[1]!.dispatchEvent(new Event('detect-impression'));
      await callbackRouterRemote.$.flushForTesting();

      // Act
      $$<HTMLElement>(modulesElement, '.cancel-button')!.click();

      // Assert.
      assertDeepEquals(false, handler.getArgs('setModulesFreVisible')[0]);
      assertDeepEquals(false, handler.getArgs('setModulesVisible')[0]);
      assertTrue(modulesElement.$.removeModuleFreToast.open);
      assertFalse(modulesElement.$.removeModuleToast.open);
      assertEquals(1, metrics.count('NewTabPage.Modules.FreLoaded', false));

      // Act.
      modulesElement.$.undoRemoveModuleFreButton.click();

      // Assert.
      assertFalse(modulesElement.$.removeModuleFreToast.open);
      assertDeepEquals(true, handler.getArgs('setModulesFreVisible')[1]);
      assertDeepEquals(true, handler.getArgs('setModulesVisible')[1]);
      assertEquals(1, handler.getCallCount('logModulesFreOptInStatus'));
      assertEquals(1, metrics.count('NewTabPage.Modules.FreLoaded', true));

      // Act.
      $$<HTMLElement>(modulesElement, '.action-button')!.click();

      // Assert.
      assertDeepEquals(false, handler.getArgs('setModulesFreVisible')[2]);
      assertEquals(2, handler.getCallCount('logModulesFreOptInStatus'));
    });
  });

  suite('modules redesigned layout', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesRedesignedLayoutEnabled: true,
        modulesFirstRunExperienceEnabled: false,
      });
    });

    test(`sibling short modules wrapped in a container`, async () => {
      // Arrange.
      const moduleArray = [];
      for (let i = 0; i < 4; ++i) {
        const module = createElement();
        moduleArray.push(module);
      }
      const fooDescriptor = new ModuleDescriptorV2(
          'foo', ModuleHeight.SHORT, async () => createElement());
      const barDescriptor = new ModuleDescriptorV2(
          'bar', ModuleHeight.SHORT, async () => createElement());
      const bazDescriptor = new ModuleDescriptorV2(
          'baz', ModuleHeight.SHORT, async () => createElement());
      const quzDescriptor = new ModuleDescriptorV2(
          'quz', ModuleHeight.TALL, async () => createElement());
      moduleRegistry.setResultFor(
          'getDescriptors',
          [fooDescriptor, barDescriptor, bazDescriptor, quzDescriptor]);

      // Act.
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          element: moduleArray[0]!,
        },
        {
          descriptor: barDescriptor,
          element: moduleArray[1]!,
        },
        {
          descriptor: bazDescriptor,
          element: moduleArray[2]!,
        },
        {
          descriptor: quzDescriptor,
          element: moduleArray[3]!,
        },
      ]);

      callbackRouterRemote.setDisabledModules(false, []);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      const modules =
          Array.from(modulesElement.shadowRoot!.querySelectorAll('#modules'));
      const moduleWrappers =
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
      const moduleWrapperContainers =
          modulesElement.shadowRoot!.querySelectorAll('.module-container');
      const shortModuleSiblingsContainers =
          modulesElement.shadowRoot!.querySelectorAll(
              '.short-module-siblings-container');
      assertEquals(4, moduleWrappers.length);
      assertEquals(4, moduleWrapperContainers.length);
      assertEquals(1, shortModuleSiblingsContainers.length);
      assertEquals(modules[0]!.children[0], shortModuleSiblingsContainers[0]);
      assertEquals(
          moduleArray[0],
          (shortModuleSiblingsContainers[0]!.children[0]!.children[0] as
           ModuleWrapperElement)
              .module.element);
      assertEquals(
          moduleArray[1],
          (shortModuleSiblingsContainers[0]!.children[1]!.children[0] as
           ModuleWrapperElement)
              .module.element);
    });

    test('modules can be disabled and restored', async () => {
      // Arrange.
      let restoreCalled = false;
      const moduleArray = [];
      for (let i = 0; i < 3; ++i) {
        const module = createElement();
        moduleArray.push(module);
      }
      const fooDescriptor = new ModuleDescriptorV2(
          'foo', ModuleHeight.SHORT, async () => createElement());
      const barDescriptor = new ModuleDescriptorV2(
          'bar', ModuleHeight.SHORT, async () => createElement());
      const bazDescriptor = new ModuleDescriptorV2(
          'baz', ModuleHeight.SHORT, async () => createElement());
      moduleRegistry.setResultFor(
          'getDescriptors', [fooDescriptor, barDescriptor, bazDescriptor]);

      // Act.
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          element: moduleArray[0]!,
        },
        {
          descriptor: barDescriptor,
          element: moduleArray[1]!,
        },
        {
          descriptor: bazDescriptor,
          element: moduleArray[2]!,
        },
      ]);

      callbackRouterRemote.setDisabledModules(false, []);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      const modules =
          Array.from(modulesElement.shadowRoot!.querySelectorAll('#modules'));
      const moduleWrappers =
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
      const moduleWrapperContainers =
          modulesElement.shadowRoot!.querySelectorAll('.module-container');
      let shortModuleSiblingsContainers =
          modulesElement.shadowRoot!.querySelectorAll(
              '.short-module-siblings-container');
      assertEquals(3, moduleWrappers.length);
      assertEquals(3, moduleWrapperContainers.length);
      assertEquals(1, shortModuleSiblingsContainers.length);
      assertEquals(modules[0]!.children[0], shortModuleSiblingsContainers[0]);
      assertEquals(
          moduleArray[0],
          (shortModuleSiblingsContainers[0]!.children[0]!.children[0] as
           ModuleWrapperElement)
              .module.element);
      assertEquals(
          moduleArray[1],
          (shortModuleSiblingsContainers[0]!.children[1]!.children[0] as
           ModuleWrapperElement)
              .module.element);
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
      shortModuleSiblingsContainers =
          modulesElement.shadowRoot!.querySelectorAll(
              '.short-module-siblings-container');
      assertEquals(1, shortModuleSiblingsContainers.length);
      assertEquals(modules[0]!.children[1], shortModuleSiblingsContainers[0]);
      assertEquals(
          moduleArray[1],
          (shortModuleSiblingsContainers[0]!.children[0]!.children[0] as
           ModuleWrapperElement)
              .module.element);
      assertEquals(
          moduleArray[2],
          (shortModuleSiblingsContainers[0]!.children[1]!.children[0] as
           ModuleWrapperElement)
              .module.element);
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
      modulesElement.$.undoRemoveModuleButton.click();

      // Assert.
      assertDeepEquals(['foo', false], handler.getArgs('setModuleDisabled')[1]);

      // Act.
      callbackRouterRemote.setDisabledModules(false, []);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      shortModuleSiblingsContainers =
          modulesElement.shadowRoot!.querySelectorAll(
              '.short-module-siblings-container');
      assertEquals(1, shortModuleSiblingsContainers.length);
      assertEquals(modules[0]!.children[0], shortModuleSiblingsContainers[0]);
      assertEquals(
          moduleArray[0],
          (shortModuleSiblingsContainers[0]!.children[0]!.children[0] as
           ModuleWrapperElement)
              .module.element);
      assertEquals(
          moduleArray[1],
          (shortModuleSiblingsContainers[0]!.children[1]!.children[0] as
           ModuleWrapperElement)
              .module.element);
      assertNotStyle(moduleWrappers[0]!, 'display', 'none');
      assertNotStyle(moduleWrapperContainers[0]!, 'display', 'none');
      assertFalse(modulesElement.$.removeModuleToast.open);
      assertTrue(restoreCalled);
      assertEquals(1, metrics.count('NewTabPage.Modules.Enabled', 'foo'));
      assertEquals(1, metrics.count('NewTabPage.Modules.Enabled.Toast', 'foo'));

      // // Act.
      window.dispatchEvent(new KeyboardEvent('keydown', {
        key: 'z',
        ctrlKey: true,
      }));

      // Assert: no crash.
    });
  });

  test('modules can be dismissed and restored', async () => {
    // Arrange.
    let restoreCalled = false;
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    moduleRegistry.setResultFor('getDescriptors', [fooDescriptor]);

    // Act.
    const modulesElement = await createModulesElement([
      {
        descriptor: fooDescriptor,
        element: createElement(),
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
    modulesElement.$.undoRemoveModuleButton.click();

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
    moduleRegistry.setResultFor('getDescriptors', [fooDescriptor]);

    // Act.
    const modulesElement = await createModulesElement([{
      descriptor: fooDescriptor,
      element: createElement(),
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
    modulesElement.$.undoRemoveModuleButton.click();

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
    moduleRegistry.setResultFor(
        'getDescriptors', [fooDescriptor, barDescriptor]);
    await createModulesElement([
      {
        descriptor: fooDescriptor,
        element: createElement(),
      },
      {
        descriptor: barDescriptor,
        element: createElement(),
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
    moduleRegistry.setResultFor(
        'getDescriptors', [fooDescriptor, barDescriptor]);
    await createModulesElement([
      {
        descriptor: fooDescriptor,
        element: createElement(),
      },
      {
        descriptor: barDescriptor,
        element: createElement(),
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
        modulesFirstRunExperienceEnabled: false,
      });
    });

    test('drag first module to third then second position', async () => {
      // Arrange.
      const moduleArray = [];
      for (let i = 0; i < 3; ++i) {
        const module = createElement();
        moduleArray.push(module);
      }
      const fooDescriptor = new ModuleDescriptorV2(
          'foo', ModuleHeight.TALL, async () => createElement());
      const barDescriptor = new ModuleDescriptorV2(
          'bar', ModuleHeight.TALL, async () => createElement());
      const fooBarDescriptor = new ModuleDescriptorV2(
          'foo bar', ModuleHeight.TALL, async () => createElement());

      moduleRegistry.setResultFor(
          'getDescriptors', [fooDescriptor, barDescriptor, fooBarDescriptor]);
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          element: moduleArray[0]!,
        },
        {
          descriptor: barDescriptor,
          element: moduleArray[1]!,
        },
        {
          descriptor: fooBarDescriptor,
          element: moduleArray[2]!,
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

    test('drag tall module over short module sibling container', async () => {
      // Arrange.
      const moduleArray = [];
      for (let i = 0; i < 3; ++i) {
        const module = createElement();
        moduleArray.push(module);
      }
      const fooDescriptor = new ModuleDescriptorV2(
          'foo', ModuleHeight.TALL, async () => createElement());
      const barDescriptor = new ModuleDescriptorV2(
          'bar', ModuleHeight.SHORT, async () => createElement());
      const fooBarDescriptor = new ModuleDescriptorV2(
          'foo bar', ModuleHeight.SHORT, async () => createElement());

      moduleRegistry.setResultFor(
          'getDescriptors', [fooDescriptor, barDescriptor, fooBarDescriptor]);
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          element: moduleArray[0]!,
        },
        {
          descriptor: barDescriptor,
          element: moduleArray[1]!,
        },
        {
          descriptor: fooBarDescriptor,
          element: moduleArray[2]!,
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
      tallModule.dispatchEvent(new MouseEvent('mousedown'));
      document.dispatchEvent(new MouseEvent('mousemove'));

      // Act.
      shortModule1.dispatchEvent(new MouseEvent('mouseover'));

      // Assert.
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(shortModule1));
      assertEquals(1, moduleWrappers.indexOf(shortModule2));
      assertEquals(2, moduleWrappers.indexOf(tallModule));

      // Act.
      shortModule2.dispatchEvent(new MouseEvent('mouseover'));

      // Assert.
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(tallModule));
      assertEquals(1, moduleWrappers.indexOf(shortModule1));
      assertEquals(2, moduleWrappers.indexOf(shortModule2));

      // Act.
      shortModule1.dispatchEvent(new MouseEvent('mousedown'));
      document.dispatchEvent(new MouseEvent('mousemove'));

      // Act.
      tallModule.dispatchEvent(new MouseEvent('mouseover'));

      // Assert.
      moduleWrappers = Array.from(
          modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper'));
      assertEquals(0, moduleWrappers.indexOf(shortModule1));
      assertEquals(1, moduleWrappers.indexOf(tallModule));
      assertEquals(2, moduleWrappers.indexOf(shortModule2));
    });

    test('hidden module goes to end of NTP when layout changes', async () => {
      // Arrange.
      let restoreCalled = false;
      const moduleArray = [];
      for (let i = 0; i < 3; ++i) {
        const module = createElement();
        moduleArray.push(module);
      }
      const fooDescriptor = new ModuleDescriptorV2(
          'foo', ModuleHeight.TALL, async () => createElement());
      const barDescriptor = new ModuleDescriptorV2(
          'bar', ModuleHeight.SHORT, async () => createElement());
      const fooBarDescriptor = new ModuleDescriptorV2(
          'foo bar', ModuleHeight.SHORT, async () => createElement());

      moduleRegistry.setResultFor(
          'getDescriptors', [fooDescriptor, barDescriptor, fooBarDescriptor]);
      const modulesElement = await createModulesElement([
        {
          descriptor: fooDescriptor,
          element: moduleArray[0]!,
        },
        {
          descriptor: barDescriptor,
          element: moduleArray[1]!,
        },
        {
          descriptor: fooBarDescriptor,
          element: moduleArray[2]!,
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

      // Act.
      modulesElement.$.undoRemoveModuleButton.click();

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
