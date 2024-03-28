// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Module, ModuleWrapperElement, NamedWidth} from 'chrome://new-tab-page/lazy_load.js';
import {MODULE_CUSTOMIZE_ELEMENT_ID, ModuleDescriptor, ModuleRegistry, ModulesV2Element, SUPPORTED_MODULE_WIDTHS} from 'chrome://new-tab-page/lazy_load.js';
import {NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';

import {assertNotStyle, assertStyle, createElement, initNullModule, installMock} from '../../test_support.js';

const SAMPLE_SCREEN_WIDTH = 1080;
const MAX_COLUMN_COUNT = 5;
const NO_MAX_INSTANCE_COUNT = -1;

suite('NewTabPageModulesModulesV2Test', () => {
  let callbackRouterRemote: PageRemote;
  let handler: TestMock<PageHandlerRemote>;
  let metrics: MetricsTracker;
  let moduleRegistry: TestMock<ModuleRegistry>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      modulesMaxColumnCount: MAX_COLUMN_COUNT,
      multipleLoadedModulesMaxModuleInstanceCount: NO_MAX_INSTANCE_COUNT,
    });
    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    metrics = fakeMetricsPrivate();
    moduleRegistry = installMock(ModuleRegistry);
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function createModulesElement(
      modules: Module[], enabled: boolean,
      width: number): Promise<ModulesV2Element> {
    if (!enabled) {
      assertTrue(
          modules.length === 0,
          'modules array must be empty if modules disabled');
    }
    const modulesPromise = new Promise<Module[]>((resolve, _) => {
      callbackRouterRemote.setDisabledModules(!enabled, []);
      callbackRouterRemote.$.flushForTesting().then(() => {
        resolve(modules);
      });
    });

    moduleRegistry.setResultFor('initializeModulesHavingIds', modulesPromise);
    const element = new ModulesV2Element();
    document.body.style.width = `${width}px`;
    document.body.appendChild(element);
    await modulesPromise;
    return element;
  }

  async function createModulesElementFromDescriptors(
      descriptors: ModuleDescriptor[],
      instanceCount: number): Promise<HTMLElement> {
    handler.setResultFor('getModulesIdNames', {
      data: descriptors,
    });

    const modules: Module[] = descriptors.map(descriptor => {
      return {
        descriptor: descriptor,
        elements: Array(instanceCount).fill(0).map(_ => createElement()),
      } as Module;
    });
    const modulesElement =
        await createModulesElement(modules, true, SAMPLE_SCREEN_WIDTH);
    return modulesElement;
  }

  const NARROW_WIDTH = SUPPORTED_MODULE_WIDTHS[0]!;
  const MEDIUM_WIDTH = SUPPORTED_MODULE_WIDTHS[1]!;
  const WIDE_WIDTH = SUPPORTED_MODULE_WIDTHS[2]!;

  interface Scenario {
    width: number;
    count: number;
    rows: NamedWidth[][];
  }

  interface LayoutChangeScenario {
    setup: Array<{name: string, count: number}>;
    before: Scenario;
    after: Scenario;
  }

  function generateScenarioCompactName(scenario: Scenario): string {
    return `scenario: ${scenario.width}W-${scenario.count}I[${
        scenario.rows.map(row => `${row.length}C`)}]`;
  }

  [{
    // 312px (narrow) * 5 + 8px (gap) * 4 + 48px (margin) * 2
    width: 1688,
    count: MAX_COLUMN_COUNT,
    rows: [[
      NARROW_WIDTH,
      NARROW_WIDTH,
      NARROW_WIDTH,
      NARROW_WIDTH,
      NARROW_WIDTH,
    ]],
  },
   {
     // 312px (narrow) * 4 + 8px (gap) * 3 + 48px (margin) * 2
     width: 1368,
     count: MAX_COLUMN_COUNT,
     rows: [
       [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
       [WIDE_WIDTH],
     ],
   },
   {
     // 312px (narrow) * 3 + 8px (gap) * 2 + 48px (margin) * 2
     width: 1048,
     count: 3,
     rows: [[NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH]],
   },
   {
     // 360px (medium) * 2 + 8px (gap) + 48px (margin) * 2
     width: 856,
     count: 3,
     rows: [[MEDIUM_WIDTH, MEDIUM_WIDTH], [WIDE_WIDTH]],
   },
   {
     // 728px (wide) * 1, + 48px (margin) * 2
     width: 824,
     count: 1,
     rows: [[WIDE_WIDTH]],
   },
   {
     // 312px (narrow) * 2 + 8px (gap) + 48px (margin) * 2
     width: 728,
     count: 3,
     rows: [[NARROW_WIDTH, NARROW_WIDTH], [MEDIUM_WIDTH]],
   },
   {
     // 360 (medium) * 1 + 48px (margin) * 2
     width: 456,
     count: 3,
     rows: [[MEDIUM_WIDTH], [MEDIUM_WIDTH], [MEDIUM_WIDTH]],
   },
   {
     // 360 (medium) * 1 + 48px (margin) * 2
     width: 456,
     count: 1,
     rows: [[MEDIUM_WIDTH]],
   },
   {
     // 312 (narrow) * 1 + 48px (margin) * 2
     width: 408,
     count: 3,
     rows: [[NARROW_WIDTH], [NARROW_WIDTH], [NARROW_WIDTH]],
   },
   {
     // 312 (narrow) * 1 + 48px (margin) * 2
     width: 408,
     count: 1,
     rows: [[NARROW_WIDTH]],
   },
  ].forEach((scenario: Scenario) => {
    test(`Layout ${generateScenarioCompactName(scenario)}`, async () => {
      const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
      handler.setResultFor('getModulesIdNames', {
        data: [
          {id: fooDescriptor.id, name: fooDescriptor.id},
        ],
      });

      const modulesElement = await createModulesElement(
          [
            {
              descriptor: fooDescriptor,
              elements: Array(scenario.count).fill(0).map(_ => createElement()),
            },
          ],
          true, scenario.width);
      await waitAfterNextRender(modulesElement);

      const wrappers = modulesElement.shadowRoot!.querySelectorAll(
          'ntp-module-wrapper:not([hidden])');
      assertEquals(scenario.count, wrappers.length);

      let index = 0;
      scenario.rows.forEach((expectedRowWidths, i) => {
        expectedRowWidths.forEach((expectedWidth, j) => {
          const wrapper = wrappers[index]! as ModuleWrapperElement;
          const instance = wrapper.$.moduleElement.lastChild! as HTMLElement;
          assertEquals(expectedWidth.name, instance.getAttribute('format'));
          assertEquals(
              expectedWidth.value, wrapper.clientWidth,
              `Element at row ${i} column ${j}`);
          index++;
        });
      });
    });
  });

  test('No modules rendered when all disabled', async () => {
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    const barDescriptor = new ModuleDescriptor('bar', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
        {id: barDescriptor.id, name: barDescriptor.id},
      ],
    });
    const modulesElement =
        await createModulesElement([], false, SAMPLE_SCREEN_WIDTH);
    await waitAfterNextRender(modulesElement);

    const moduleWrappers =
        modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
    assertEquals(0, moduleWrappers.length);
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadedModulesCount', 0));
    assertEquals(1, metrics.count('NewTabPage.Modules.InstanceCount', 0));
    assertEquals(
        1, metrics.count('NewTabPage.Modules.VisibleOnNTPLoad', false));
  });

  test(
      'module(s) with multiple element instances render correcly', async () => {
        const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
        const barDescriptor = new ModuleDescriptor('bar', initNullModule);
        handler.setResultFor('getModulesIdNames', {
          data: [
            {id: fooDescriptor.id, name: fooDescriptor.id},
            {id: barDescriptor.id, name: barDescriptor.id},
          ],
        });

        const modulesElement = await createModulesElement(
            [
              {
                descriptor: fooDescriptor,
                elements: Array(3).fill(0).map(_ => createElement()),
              },
              {
                descriptor: barDescriptor,
                elements: [createElement()],
              },
            ],
            true, SAMPLE_SCREEN_WIDTH);

        const moduleWrappers =
            modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
        assertEquals(4, moduleWrappers.length);
        assertEquals(1, metrics.count('NewTabPage.Modules.LoadedModulesCount'));
        assertEquals(1, metrics.count('NewTabPage.Modules.InstanceCount', 4));
        assertEquals(
            1, metrics.count('NewTabPage.Modules.VisibleOnNTPLoad', true));

        // Assert metrics for module loaded with other modules.
        assertEquals(
            1, metrics.count('NewTabPage.Modules.LoadedWith.foo', 'bar'));
        assertEquals(
            0, metrics.count('NewTabPage.Modules.LoadedWith.foo', 'foo'));
        assertEquals(
            1, metrics.count('NewTabPage.Modules.LoadedWith.bar', 'foo'));
        assertEquals(
            0, metrics.count('NewTabPage.Modules.LoadedWith.bar', 'bar'));
      });

  test('help bubble can correctly find anchor elements', async () => {
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
      ],
    });

    const modulesElement = await createModulesElement(
        [
          {
            descriptor: fooDescriptor,
            elements: [createElement()],
          },
        ],
        true, SAMPLE_SCREEN_WIDTH);

    assertDeepEquals(
        modulesElement.getSortedAnchorStatusesForTesting(),
        [
          [MODULE_CUSTOMIZE_ELEMENT_ID, true],
        ],
    );
  });

  test('modules maxium instance count works correctly', async () => {
    const SAMPLE_MAX_MODULE_INSTANCE_COUNT = 2;
    loadTimeData.overrideValues({
      modulesMaxColumnCount: MAX_COLUMN_COUNT,
      multipleLoadedModulesMaxModuleInstanceCount:
          SAMPLE_MAX_MODULE_INSTANCE_COUNT,
    });

    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    const barDescriptor = new ModuleDescriptor('bar', initNullModule);
    const descriptors = [
      fooDescriptor,
      barDescriptor,
    ];
    const modulesElement = await createModulesElementFromDescriptors(
        descriptors, SAMPLE_MAX_MODULE_INSTANCE_COUNT + 1);
    const moduleWrappers =
        modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
    assertEquals(
        descriptors.length * SAMPLE_MAX_MODULE_INSTANCE_COUNT,
        moduleWrappers.length);
  });

  test('modules maxium instance capped to maximum column count', async () => {
    const SAMPLE_MAX_COLUMN_COUNT = 3;
    const SAMPLE_MAX_MODULE_INSTANCE_COUNT = 3;
    loadTimeData.overrideValues({
      modulesMaxColumnCount: SAMPLE_MAX_COLUMN_COUNT,
      multipleLoadedModulesMaxModuleInstanceCount:
          SAMPLE_MAX_MODULE_INSTANCE_COUNT,
    });

    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    const barDescriptor = new ModuleDescriptor('bar', initNullModule);
    const bazDescriptor = new ModuleDescriptor('baz', initNullModule);
    const descriptors = [
      fooDescriptor,
      barDescriptor,
      bazDescriptor,
    ];
    const modulesElement = await createModulesElementFromDescriptors(
        descriptors, SAMPLE_MAX_MODULE_INSTANCE_COUNT);
    const moduleWrappers =
        modulesElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
    assertEquals(SAMPLE_MAX_COLUMN_COUNT, moduleWrappers.length);
  });

  enum UndoStrategy {
    BUTTON_ACTIVATION = 'button activation',
    SHORTCUT_KEY = 'shortcut key',
  }

  [UndoStrategy.BUTTON_ACTIVATION, UndoStrategy.SHORTCUT_KEY].forEach(
      (undoStrategy: UndoStrategy) => {
        test(
            `modules can be disabled and restored via ${undoStrategy}`,
            async () => {
              // Arrange.
              const moduleId = 'foo';
              const fooDescriptor =
                  new ModuleDescriptor(moduleId, initNullModule);
              handler.setResultFor('getModulesIdNames', {
                data: [
                  {id: fooDescriptor.id, name: fooDescriptor.id},
                ],
              });
              const modulesElement = await createModulesElement(
                  [{
                    descriptor: fooDescriptor,
                    elements: [createElement()],
                  }],
                  true, SAMPLE_SCREEN_WIDTH);

              // Assert.
              const moduleWrappers =
                  modulesElement.shadowRoot!.querySelectorAll(
                      'ntp-module-wrapper');
              assertEquals(1, moduleWrappers.length);
              assertNotStyle(moduleWrappers[0]!, 'display', 'none');
              assertFalse(modulesElement.$.undoToast.open);

              // Act.
              let restoreCalled = false;
              moduleWrappers[0]!.dispatchEvent(
                  new CustomEvent('disable-module', {
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
              assertDeepEquals(
                  ['foo', true], handler.getArgs('setModuleDisabled')[0]);

              // Act.
              callbackRouterRemote.setDisabledModules(false, [moduleId]);
              await callbackRouterRemote.$.flushForTesting();

              // Assert.
              assertStyle(moduleWrappers[0]!, 'display', 'none');
              assertTrue(modulesElement.$.undoToast.open);
              assertEquals(
                  'Foo', modulesElement.$.undoToastMessage.textContent!.trim());
              assertEquals(
                  1, metrics.count('NewTabPage.Modules.Disabled', moduleId));
              assertEquals(
                  1,
                  metrics.count(
                      'NewTabPage.Modules.Disabled.ModuleRequest', moduleId));
              assertFalse(restoreCalled);

              // Act.
              await waitAfterNextRender(modulesElement);
              if (undoStrategy === UndoStrategy.BUTTON_ACTIVATION) {
                const undoButton =
                    modulesElement.shadowRoot!.querySelector<HTMLElement>(
                        '#undoButton');
                assertTrue(!!undoButton);
                undoButton.click();
              } else if (undoStrategy === UndoStrategy.SHORTCUT_KEY) {
                window.dispatchEvent(new KeyboardEvent('keydown', {
                  key: 'z',
                  ctrlKey: true,
                }));
              }

              // Assert.
              assertDeepEquals(
                  ['foo', false], handler.getArgs('setModuleDisabled')[1]);

              // Act.
              callbackRouterRemote.setDisabledModules(false, []);
              await callbackRouterRemote.$.flushForTesting();

              // Assert.
              assertNotStyle(moduleWrappers[0]!, 'display', 'none');
              assertFalse(modulesElement.$.undoToast.open);
              assertTrue(restoreCalled);
              assertEquals(
                  1, metrics.count('NewTabPage.Modules.Enabled', moduleId));
              assertEquals(
                  1,
                  metrics.count('NewTabPage.Modules.Enabled.Toast', moduleId));
            });

        test(
            `modules can be dismissed and restored via ${undoStrategy}`,
            async () => {
              const moduleId = 'foo';
              const fooDescriptor =
                  new ModuleDescriptor(moduleId, initNullModule);
              handler.setResultFor('getModulesIdNames', {
                data: [
                  {id: fooDescriptor.id, name: fooDescriptor.id},
                ],
              });
              const modulesElement = await createModulesElement(
                  [{
                    descriptor: fooDescriptor,
                    elements: [createElement()],
                  }],
                  true, SAMPLE_SCREEN_WIDTH);

              let moduleWrappers = modulesElement.shadowRoot!.querySelectorAll(
                  'ntp-module-wrapper');
              assertEquals(1, moduleWrappers.length);
              assertFalse(modulesElement.$.undoToast.open);

              let restoreCalled = false;
              moduleWrappers[0]!.dispatchEvent(
                  new CustomEvent('dismiss-module-instance', {
                    bubbles: true,
                    composed: true,
                    detail: {
                      message: 'Foo',
                      restoreCallback: () => {
                        restoreCalled = true;
                      },
                    },
                  }));

              assertEquals(
                  0,
                  modulesElement.shadowRoot!
                      .querySelectorAll('ntp-module-wrapper')
                      .length);
              assertTrue(modulesElement.$.undoToast.open);
              assertFalse(restoreCalled);
              assertEquals(1, handler.getCallCount('onDismissModule'));
              assertEquals(moduleId, handler.getArgs('onDismissModule')[0]);

              await waitAfterNextRender(modulesElement);
              if (undoStrategy === UndoStrategy.BUTTON_ACTIVATION) {
                const undoButton =
                    modulesElement.shadowRoot!.querySelector<HTMLElement>(
                        '#undoButton');
                assertTrue(!!undoButton);
                undoButton.click();
              } else if (undoStrategy === UndoStrategy.SHORTCUT_KEY) {
                window.dispatchEvent(new KeyboardEvent('keydown', {
                  key: 'z',
                  ctrlKey: true,
                }));
              }

              moduleWrappers = modulesElement.shadowRoot!.querySelectorAll(
                  'ntp-module-wrapper');
              assertEquals(1, moduleWrappers.length);
              assertFalse(modulesElement.$.undoToast.open);
              assertTrue(restoreCalled);
              assertEquals(
                  1, metrics.count('NewTabPage.Modules.Restored'),
                  'Restore metric value');
              assertEquals(1, metrics.count('NewTabPage.Modules.Restored.foo'));
            });
      });

  test('Undo shortcut ignored if no undo state', async () => {
    // Arrange
    const fooDescriptor = new ModuleDescriptor('foo', initNullModule);
    handler.setResultFor('getModulesIdNames', {
      data: [
        {id: fooDescriptor.id, name: fooDescriptor.id},
      ],
    });
    const modulesElement = await createModulesElement(
        [{
          descriptor: fooDescriptor,
          elements: [createElement()],
        }],
        true, SAMPLE_SCREEN_WIDTH);
    await waitAfterNextRender(modulesElement);

    // Act.
    window.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'z',
      ctrlKey: true,
    }));

    // Assert: no crash.
  });

  function assertContainerLayout(
      moduleWrappers: ModuleWrapperElement[], scenario: Scenario) {
    assertEquals(scenario.count, moduleWrappers.length);

    let index = 0;
    scenario.rows.forEach((expectedRowWidths, i) => {
      expectedRowWidths.forEach((expectedWidth, j) => {
        const wrapper = moduleWrappers[index]!;
        const instance = wrapper.$.moduleElement.lastChild! as HTMLElement;
        assertEquals(expectedWidth.name, instance.getAttribute('format'));
        assertEquals(
            expectedWidth.value, wrapper.clientWidth,
            `Element at row ${i} column ${j}`);
        index++;
      });
    });

    assertEquals(
        scenario.rows.length,
        new Set(moduleWrappers.map(wrapper => wrapper.offsetTop)).size);
  }

  [{
    setup: [
      {name: 'foo', count: 3},
      {name: 'bar', count: 2},
      {name: 'baz', count: 1},
    ],
    before: {
      width: 1080,
      count: 6,
      rows: [
        [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
        [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
      ],
    },
    after: {
      width: 1080,
      count: 3,
      rows: [
        [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
      ],
    },
  },
   {
     setup: [
       {name: 'foo', count: 1},
       {name: 'bar', count: 2},
       {name: 'baz', count: 3},
     ],
     before: {
       width: 1080,
       count: 6,
       rows: [
         [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
         [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
       ],
     },
     after: {
       width: 1080,
       count: 5,
       rows: [
         [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
         [MEDIUM_WIDTH, MEDIUM_WIDTH],
       ],
     },
   }].forEach((layoutChangeScenario: LayoutChangeScenario, index) => {
    test(
        `Disabling one of many modules updates layout correctly ${index}`,
        async () => {
          const modules = layoutChangeScenario.setup.map(details => {
            return {
              descriptor: new ModuleDescriptor(details.name, initNullModule),
              elements: Array(details.count).fill(0).map(() => createElement()),
            };
          });
          handler.setResultFor('getModulesIdNames', {
            data: modules.map((module) => {
              return {id: module.descriptor.id, name: module.descriptor.id};
            }),
          });
          const modulesElement =
              await createModulesElement(modules, true, SAMPLE_SCREEN_WIDTH);
          await waitAfterNextRender(modulesElement);

          const moduleWrappers =
              Array.from(
                  modulesElement.shadowRoot!.querySelectorAll<HTMLElement>(
                      'ntp-module-wrapper')) as ModuleWrapperElement[];
          assertContainerLayout(moduleWrappers, layoutChangeScenario.before);

          moduleWrappers[0]!.dispatchEvent(new CustomEvent('disable-module', {
            bubbles: true,
            composed: true,
            detail: {
              message: 'Foo',
            },
          }));
          assertDeepEquals(
              ['foo', true], handler.getArgs('setModuleDisabled')[0]);
          callbackRouterRemote.setDisabledModules(false, ['foo']);
          await callbackRouterRemote.$.flushForTesting();
          await waitAfterNextRender(modulesElement);

          assertContainerLayout(
              Array.from(
                  modulesElement.shadowRoot!.querySelectorAll<HTMLElement>(
                      'ntp-module-wrapper:not([hidden])')) as
                  ModuleWrapperElement[],
              layoutChangeScenario.after);
        });
  });

  [{
    setup: [
      {name: 'foo', count: 3},
      {name: 'bar', count: 2},
      {name: 'baz', count: 1},
    ],
    before: {
      width: 1080,
      count: 6,
      rows: [
        [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
        [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
      ],
    },
    after: {
      width: 1080,
      count: 5,
      rows: [
        [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
        [MEDIUM_WIDTH, MEDIUM_WIDTH],
      ],
    },
  },
   {
     setup: [
       {name: 'foo', count: 1},
       {name: 'bar', count: 2},
       {name: 'baz', count: 3},
     ],
     before: {
       width: 1080,
       count: 6,
       rows: [
         [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
         [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
       ],
     },
     after: {
       width: 1080,
       count: 5,
       rows: [
         [NARROW_WIDTH, NARROW_WIDTH, NARROW_WIDTH],
         [MEDIUM_WIDTH, MEDIUM_WIDTH],
       ],
     },
   }].forEach((layoutChangeScenario: LayoutChangeScenario, index) => {
    test(
        `Dismissing one of many modules updates layout correctly ${index}`,
        async () => {
          const modules = layoutChangeScenario.setup.map(details => {
            return {
              descriptor: new ModuleDescriptor(details.name, initNullModule),
              elements: Array(details.count).fill(0).map(() => createElement()),
            };
          });
          handler.setResultFor('getModulesIdNames', {
            data: modules.map((module) => {
              return {id: module.descriptor.id, name: module.descriptor.id};
            }),
          });
          const modulesElement =
              await createModulesElement(modules, true, SAMPLE_SCREEN_WIDTH);
          await waitAfterNextRender(modulesElement);

          const moduleWrappers =
              Array.from(
                  modulesElement.shadowRoot!.querySelectorAll<HTMLElement>(
                      'ntp-module-wrapper')) as ModuleWrapperElement[];
          assertContainerLayout(moduleWrappers, layoutChangeScenario.before);

          let restoreCalled = false;
          moduleWrappers[0]!.dispatchEvent(
              new CustomEvent('dismiss-module-instance', {
                bubbles: true,
                composed: true,
                detail: {
                  message: 'Foo',
                  restoreCallback: () => {
                    restoreCalled = true;
                  },
                },
              }));
          assertFalse(restoreCalled);
          await waitAfterNextRender(modulesElement);

          assertContainerLayout(
              Array.from(
                  modulesElement.shadowRoot!.querySelectorAll<HTMLElement>(
                      'ntp-module-wrapper')) as ModuleWrapperElement[],
              layoutChangeScenario.after);
        });
  });
});
