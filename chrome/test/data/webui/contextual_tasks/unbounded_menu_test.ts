// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/top_toolbar.js';
import 'chrome://contextual-tasks/sources_menu.js';
import 'chrome://contextual-tasks/overflow_menu.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {SourcesMenuElement} from 'chrome://contextual-tasks/sources_menu.js';
import type {TopToolbarElement} from 'chrome://contextual-tasks/top_toolbar.js';
import type {UnboundedDialog} from 'chrome://contextual-tasks/utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('UnboundedMenuTest', () => {
  let topToolbar: TopToolbarElement;
  let proxy: TestContextualTasksBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    proxy = new TestContextualTasksBrowserProxy(
        'chrome://webui-test/contextual_tasks/test.html');
    BrowserProxyImpl.setInstance(proxy);

    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: false,
      contextualTasksEnableSpatialModelToolbarLayout: false,
      contextualTasksUnboundedMenuEnabled: true,
      isSmallDeviceFormFactor: false,
    });
  });

  suite('SourcesMenu', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        expandButtonEnabled: true,
      });
      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();
    });

    test('handles sources menu interactions', async () => {
      const tab = {
        title: 'Tab 1',
        url: 'https://example.com',
        hasChromeTabData: false,
        tabId: 1,
      };
      topToolbar.contextInfos = [{tab: tab}];
      await microtasksFinished();

      const sourcesMenuElement: SourcesMenuElement =
          topToolbar.$.sourcesMenu.get();
      let showUnboundedCalled = false;
      let hideUnboundedCalled = false;
      const dialogEl = sourcesMenuElement.$.menu.getDialog() as UnboundedDialog;
      dialogEl.showUnboundedElement = () => {
        showUnboundedCalled = true;
        return Promise.resolve();
      };
      dialogEl.hideUnboundedElement = () => {
        hideUnboundedCalled = true;
        return Promise.resolve();
      };

      const sourcesButton =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#sources');
      assertTrue(!!sourcesButton);
      sourcesButton.click();
      await microtasksFinished();

      assertTrue(showUnboundedCalled, 'showUnboundedCalled should be true');
      assertTrue(
          dialogEl.hasAttribute('unbounded'),
          'dialogEl should have unbounded attribute');

      const crActionMenu =
          sourcesMenuElement.shadowRoot.querySelector<CrActionMenuElement>(
              'cr-action-menu');
      assertTrue(!!crActionMenu);
      assertTrue(crActionMenu.open);

      // Click the first tab item.
      const tabItem = sourcesMenuElement.shadowRoot.querySelector<HTMLElement>(
          'cr-url-list-item.dropdown-item');
      assertTrue(!!tabItem);
      tabItem.click();
      await microtasksFinished();

      assertTrue(hideUnboundedCalled, 'hideUnboundedCalled should be true');
      assertFalse(
          dialogEl.hasAttribute('unbounded'),
          'dialogEl should not have unbounded attribute');

      const [tabId, url] =
          await proxy.handler.whenCalled('onTabClickedFromSourcesMenu');
      assertEquals(tabId, 1);
      assertDeepEquals(url, tab.url);
    });

    test('does not use unbounded menu if flag is disabled', async () => {
      loadTimeData.overrideValues({contextualTasksUnboundedMenuEnabled: false});

      const menu = topToolbar.$.overflowMenu.get();
      let showUnboundedCalled = false;
      const dialogEl = menu.$.menu.getDialog() as UnboundedDialog;
      dialogEl.showUnboundedElement = () => {
        showUnboundedCalled = true;
        return Promise.resolve();
      };

      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);
      moreButton.click();
      await microtasksFinished();

      assertFalse(showUnboundedCalled);
      assertFalse(dialogEl.hasAttribute('unbounded'));
    });
  });

  suite('OverflowMenu', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        expandButtonEnabled: false,
        hideMenuOnAiPageEnabled: false,
        isAiPage: true,
        enablePinButton: false,
      });

      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();
    });

    test('handles more menu interactions', async () => {
      const menu = topToolbar.$.overflowMenu.get();
      let showUnboundedCalled = false;
      let hideUnboundedCalled = false;
      const dialogEl = menu.$.menu.getDialog() as UnboundedDialog;
      dialogEl.showUnboundedElement = () => {
        showUnboundedCalled = true;
        return Promise.resolve();
      };
      dialogEl.hideUnboundedElement = () => {
        hideUnboundedCalled = true;
        return Promise.resolve();
      };

      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);
      moreButton.click();
      await microtasksFinished();

      assertTrue(showUnboundedCalled);
      assertTrue(dialogEl.hasAttribute('unbounded'));
      assertTrue(menu.shadowRoot.querySelector('cr-action-menu')!.open);

      menu.close();
      await microtasksFinished();
      assertTrue(hideUnboundedCalled);
      assertFalse(dialogEl.hasAttribute('unbounded'));
    });
  });
});
