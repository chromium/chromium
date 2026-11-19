// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DisableModuleEvent, LaunchpadWidgetElement} from 'chrome://new-tab-page/lazy_load.js';
import {launchpadDescriptor, ModuleRegistry} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabPageModulesLaunchpadWidgetTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  async function createLaunchpadWidget(): Promise<LaunchpadWidgetElement> {
    const module =
        await launchpadDescriptor.initialize(0) as LaunchpadWidgetElement;
    assertTrue(!!module);
    document.body.append(module);
    await microtasksFinished();
    return module;
  }

  test('creates module with zero state greeting', async () => {
    const module = await createLaunchpadWidget();

    assertTrue(isVisible(module.$.moduleHeader));
    assertEquals(
        loadTimeData.getString('modulesLaunchpadTitle'),
        module.$.moduleHeader.headerText);
    assertTrue(isVisible(module.$.greeting));
    assertEquals(
        loadTimeData.getString('modulesLaunchpadZeroStateGreeting'),
        module.$.greeting.textContent.trim());
  });

  test('clicking the disable button fires a disable module event', async () => {
    const module = await createLaunchpadWidget();

    const whenFired = eventToPromise('disable-module', module);
    const disableButton =
        module.$.moduleHeader.shadowRoot.querySelector<HTMLElement>('#disable');
    assertTrue(!!disableButton);
    disableButton.click();

    const event: DisableModuleEvent = await whenFired;
    assertEquals(
        loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesLaunchpadTitle')),
        event.detail.message);
  });

  test('registers descriptor in module registry', async () => {
    const module =
        await ModuleRegistry.getInstance().initializeModuleById('launchpad', 0);
    assertTrue(!!module);
    assertEquals(launchpadDescriptor, module.descriptor);
    assertEquals(1, module.elements.length);
  });
});
