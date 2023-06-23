// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {MAX_COLUMN_COUNT, Module, ModuleDescriptor, ModuleRegistry, ModulesV2Element, ModuleWrapperElement, NamedWidth, SUPPORTED_MODULE_WIDTHS} from 'chrome://new-tab-page/lazy_load.js';
import {NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote, PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {createElement, initNullModule, installMock} from '../../test_support.js';

suite('NewTabPageModulesModulesV2Test', () => {
  let handler: TestMock<PageHandlerRemote>;
  let callbackRouterRemote: PageRemote;
  let moduleRegistry: TestMock<ModuleRegistry>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    moduleRegistry = installMock(ModuleRegistry);
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function createModulesElement(
      modules: Module[], width: number): Promise<ModulesV2Element> {
    const modulesPromise = Promise.resolve(modules);
    moduleRegistry.setResultFor('initializeModulesHavingIds', modulesPromise);
    const element = new ModulesV2Element();
    document.body.style.width = `${width}px`;
    document.body.appendChild(element);
    await modulesPromise;
    return element;
  }

  const NARROW_WIDTH = SUPPORTED_MODULE_WIDTHS[0]!;
  const MEDIUM_WIDTH = SUPPORTED_MODULE_WIDTHS[1]!;
  const WIDE_WIDTH = SUPPORTED_MODULE_WIDTHS[2]!;

  interface Scenario {
    width: number;
    count: number;
    rows: NamedWidth[][];
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

      const launchpadElement = await createModulesElement(
          [
            {
              descriptor: fooDescriptor,
              elements: Array(scenario.count).fill(0).map(_ => createElement()),
            },
          ],
          scenario.width);
      callbackRouterRemote.setDisabledModules(false, []);
      await callbackRouterRemote.$.flushForTesting();
      await waitAfterNextRender(launchpadElement);

      const wrappers =
          launchpadElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
      assertEquals(scenario.count, wrappers.length);

      let index = 0;
      scenario.rows.forEach((expectedRowWidths, i) => {
        expectedRowWidths.forEach((expectedWidth, j) => {
          const wrapper = wrappers[index]! as ModuleWrapperElement;
          const instance = (wrapper.$.moduleElement.lastChild! as HTMLElement);
          assertEquals(expectedWidth.name, instance.getAttribute('format'));
          assertEquals(
              expectedWidth.value, wrapper.clientWidth,
              `Element at row ${i} column ${j}`);
          index++;
        });
      });
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

    const launchpadElement = await createModulesElement(
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
        1080);
    callbackRouterRemote.setDisabledModules(false, []);
    await callbackRouterRemote.$.flushForTesting();

    const moduleWrappers =
        launchpadElement.shadowRoot!.querySelectorAll('ntp-module-wrapper');
    assertEquals(4, moduleWrappers.length);
  });
});
