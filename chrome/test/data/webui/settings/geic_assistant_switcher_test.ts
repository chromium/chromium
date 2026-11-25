// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {GlicAssistantSelection, GlicBrowserProxyImpl, LifetimeBrowserProxyImpl, loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsGeicAssistantSwitcherElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
// <if expr="not is_chromeos">
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// </if>

import {TestGlicBrowserProxy} from './test_glic_browser_proxy.js';
import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';
// clang-format on

suite('GeicAssistantSwitcher', function() {
  let switcher: SettingsGeicAssistantSwitcherElement;
  let glicBrowserProxy: TestGlicBrowserProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;

  async function createSwitcher(
      targetSelection: GlicAssistantSelection =
          GlicAssistantSelection.CONSUMER) {
    switcher = document.createElement('settings-geic-assistant-switcher');
    switcher.targetSelection = targetSelection;
    document.body.appendChild(switcher);
    await microtasksFinished();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    glicBrowserProxy = new TestGlicBrowserProxy();
    GlicBrowserProxyImpl.setInstance(glicBrowserProxy);
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);
  });

  const testCases = [
    {
      selection: GlicAssistantSelection.CONSUMER,
      labelKey: 'geicDevicePolicyToggle',
      sublabelKey: 'geicDevicePolicyToggleSublabel',
      expectedBullets: [
        {
          icon: 'settings20:draft-spark',
          textKey: 'geicDevicePolicyConsider1',
        },
        {
          icon: 'settings20:shield-spark',
          textKey: 'geicDevicePolicyConsider2',
        },
        {
          icon: 'settings20:chat-spark',
          textKey: 'geicSwitchConsiderChatHistory',
        },
      ],
    },
    {
      selection: GlicAssistantSelection.ENTERPRISE,
      labelKey: 'glicSwitchToGeicToggle',
      sublabelKey: 'glicSwitchToGeicToggleSublabel',
      expectedBullets: [
        {
          icon: 'settings20:security',
          textKey: 'glicSwitchToGeicConsider1',
        },
        {
          icon: 'settings20:shield-spark',
          textKey: 'glicSwitchToGeicConsider2',
        },
        {
          icon: 'settings20:chat-spark',
          textKey: 'geicSwitchConsiderChatHistory',
        },
      ],
    },
  ];

  for (const config of testCases) {
    const label = GlicAssistantSelection[config.selection];

    test(`RendersLabelsAndConsiderBullets_${label}`, async () => {
      await createSwitcher(config.selection);

      assertTrue(isVisible(switcher.$.section));
      assertTrue(isVisible(switcher.$.row));
      assertTrue(isVisible(switcher.$.consider));
      assertEquals(
          loadTimeData.getString(config.labelKey),
          switcher.$.label.textContent.trim());
      assertEquals(
          loadTimeData.getString(config.sublabelKey),
          switcher.$.sublabel.textContent.trim());
      assertEquals(
          loadTimeData.getString('geicDevicePolicyButton'),
          switcher.$.switchButton.textContent.trim());
      assertEquals(
          'label sublabel',
          switcher.$.switchButton.getAttribute('aria-describedby'));

      const header = switcher.$.consider.querySelector('.description-header');
      assertTrue(!!header);
      assertEquals(
          loadTimeData.getString('columnHeadingConsider'),
          header.textContent.trim());

      const bullets =
          switcher.$.consider.querySelectorAll('ul.icon-bulleted-list > li');
      assertEquals(config.expectedBullets.length, bullets.length);
      for (let i = 0; i < config.expectedBullets.length; i++) {
        const expected = config.expectedBullets[i]!;
        const icon = bullets[i]!.querySelector('cr-icon');
        assertTrue(!!icon);
        assertEquals(expected.icon, icon.icon);
        const text = bullets[i]!.querySelector('.secondary');
        assertTrue(!!text);
        assertEquals(
            loadTimeData.getString(expected.textKey), text.textContent.trim());
      }
    });

    test(`SwitchesTo${label}AndRestarts`, async () => {
      await createSwitcher(config.selection);
      assertFalse(switcher.$.switchButton.disabled);

      // The selection is switched before the restart.
      const calls: string[] = [];
      const switched = glicBrowserProxy.whenCalled('switchAssistantSelection')
                           .then(value => {
                             calls.push('switchAssistantSelection');
                             return value;
                           });
      const restarted = lifetimeBrowserProxy.whenCalled('restart').then(
          () => calls.push('restart'));

      switcher.$.switchButton.click();
      await microtasksFinished();
      assertTrue(switcher.$.switchButton.disabled);
      // <if expr="not is_chromeos">
      await lifetimeBrowserProxy.whenCalled('shouldShowRelaunchDialog');
      // </if>
      const [recordedSelection] = await Promise.all([switched, restarted]);
      assertDeepEquals(['switchAssistantSelection', 'restart'], calls);
      assertEquals(0, lifetimeBrowserProxy.getCallCount('relaunch'));
      assertEquals(config.selection, recordedSelection);
      assertTrue(switcher.$.switchButton.disabled);
    });
  }

  // <if expr="not is_chromeos">
  test('ConfirmDialogRestarts', async () => {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
    await createSwitcher();

    switcher.$.switchButton.click();
    await lifetimeBrowserProxy.whenCalled('shouldShowRelaunchDialog');
    await microtasksFinished();

    const dialog =
        switcher.shadowRoot.querySelector('relaunch-confirmation-dialog');
    assertTrue(!!dialog);
    // Not switched until the restart is confirmed.
    assertEquals(0, glicBrowserProxy.getCallCount('switchAssistantSelection'));

    dialog.$.confirm.click();
    assertEquals(
        GlicAssistantSelection.CONSUMER,
        await glicBrowserProxy.whenCalled('switchAssistantSelection'));
    await lifetimeBrowserProxy.whenCalled('restart');
    assertEquals(0, lifetimeBrowserProxy.getCallCount('relaunch'));
    await microtasksFinished();
    assertTrue(switcher.$.switchButton.disabled);
  });

  test('CancelDialogKeepsSelection', async () => {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
    await createSwitcher();

    switcher.$.switchButton.click();
    await lifetimeBrowserProxy.whenCalled('shouldShowRelaunchDialog');
    await microtasksFinished();

    const dialog =
        switcher.shadowRoot.querySelector('relaunch-confirmation-dialog');
    assertTrue(!!dialog);
    dialog.$.cancel.click();
    await eventToPromise('close', dialog);
    await microtasksFinished();

    assertEquals(0, glicBrowserProxy.getCallCount('switchAssistantSelection'));
    assertEquals(0, lifetimeBrowserProxy.getCallCount('restart'));
    assertFalse(switcher.$.switchButton.disabled);
  });
  // </if>
});
