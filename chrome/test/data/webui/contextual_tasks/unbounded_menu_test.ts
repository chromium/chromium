// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/top_toolbar.js';
import 'chrome://contextual-tasks/sources_menu.js';
import 'chrome://contextual-tasks/overflow_menu.js';

import {ToolbarBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_toolbar_browser_proxy.js';
import type {SourcesMenuElement} from 'chrome://contextual-tasks/sources_menu.js';
import type {TopToolbarElement} from 'chrome://contextual-tasks/top_toolbar.js';
import type {UnboundedDialog, UnboundedElement} from 'chrome://contextual-tasks/utils.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestToolbarBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('UnboundedMenuTest', () => {
  let topToolbar: TopToolbarElement;
  let toolbarProxy: TestToolbarBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbarProxy = new TestToolbarBrowserProxy();
    ToolbarBrowserProxyImpl.setInstance(toolbarProxy);

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
      // The `unbounded` attribute stays in sync with the menu's unbounded mode,
      // which remains enabled after close. hideUnboundedElement() tears down
      // the native surface.
      assertTrue(
          dialogEl.hasAttribute('unbounded'),
          'dialogEl should have unbounded attribute');

      const [tabId, url] =
          await toolbarProxy.handler.whenCalled('onTabClickedFromSourcesMenu');
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
      // The `unbounded` attribute stays in sync with the menu's unbounded mode,
      // which remains enabled after close. hideUnboundedElement() tears down
      // the native surface.
      assertTrue(dialogEl.hasAttribute('unbounded'));
    });

    test('stays usable when unbounded API is unsupported', async () => {
      const original =
          (HTMLElement.prototype as unknown as
           Record<string, unknown>)['showUnboundedElement'];
      delete (
          HTMLElement.prototype as unknown as
          Record<string, unknown>)['showUnboundedElement'];

      try {
        const moreButton =
            topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
                '#overflowMenuButton');
        assertTrue(!!moreButton);
        moreButton.click();
        await microtasksFinished();

        const menu = topToolbar.$.overflowMenu.get();
        const dialogEl = menu.$.menu.getDialog();
        // Regression test for b/560101585. If the platform does not support
        // unbounded elements (e.g. Android < U), showAt() must open the menu
        // as a normal bounded menu without setting 'unbounded'.
        assertFalse(
            dialogEl.hasAttribute('unbounded'),
            'unbounded attribute should not be set when API is unsupported');
        const crActionMenu = menu.shadowRoot.querySelector<CrActionMenuElement>(
            'cr-action-menu');
        assertTrue(!!crActionMenu);
        assertTrue(crActionMenu.open, 'menu should remain open');
        assertTrue(
            dialogEl.checkVisibility({visibilityProperty: true}),
            'menu should still be visible');
        menu.close();
      } finally {
        if (original !== undefined) {
          (HTMLElement.prototype as unknown as
           Record<string, unknown>)['showUnboundedElement'] = original;
        }
      }
    });
  });

  suite('AccountMenu', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        contextualTasksSidePanelRearchitectureEnabled: true,
      });
      topToolbar = document.createElement('top-toolbar');
      topToolbar.profileAvatarUrl = 'data:image/png;base64,fakeImageData';
      document.body.appendChild(topToolbar);
      await microtasksFinished();
    });

    test('opens account menu as an unbounded menu', async () => {
      const menu = topToolbar.$.accountMenu.get();
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

      const profileIndicator =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#profileIndicator');
      assertTrue(!!profileIndicator);
      profileIndicator.click();
      await microtasksFinished();

      assertTrue(showUnboundedCalled);
      assertTrue(dialogEl.hasAttribute('unbounded'));
      assertTrue(menu.$.menu.open);

      menu.close();
      await microtasksFinished();
      assertTrue(hideUnboundedCalled);
    });
  });

  suite('ProfileTooltip', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        contextualTasksSidePanelRearchitectureEnabled: true,
        googleAccount: 'Google Account',
        profileAvatarUrl: 'data:image/png;base64,fakeImageData',
      });

      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();
    });

    test('shows and hides unbounded profile tooltip on hover', async () => {
      const profileIndicator =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#profileIndicator');
      assertTrue(!!profileIndicator);
      const tooltipEl =
          topToolbar.shadowRoot.querySelector('#profileTooltip')
              ?.shadowRoot?.querySelector<UnboundedElement>('#tooltip');
      assertTrue(!!tooltipEl);

      let showUnboundedCalled = false;
      let hideUnboundedCalled = false;
      tooltipEl.showUnboundedElement = () => {
        showUnboundedCalled = true;
        return Promise.resolve();
      };
      tooltipEl.hideUnboundedElement = () => {
        hideUnboundedCalled = true;
        return Promise.resolve();
      };

      const mockTimer = new MockTimer();
      mockTimer.install();
      profileIndicator.dispatchEvent(new PointerEvent('pointerenter'));
      mockTimer.tick(500);
      mockTimer.uninstall();
      await microtasksFinished();

      assertTrue(showUnboundedCalled);
      assertTrue(tooltipEl.hasAttribute('unbounded'));
      assertTrue(tooltipEl.hasAttribute('open'));

      mockTimer.install();
      profileIndicator.dispatchEvent(new PointerEvent('pointerleave'));
      mockTimer.tick(100);
      mockTimer.uninstall();
      await microtasksFinished();

      assertTrue(hideUnboundedCalled);
      assertFalse(tooltipEl.hasAttribute('open'));
    });

    test('does not use unbounded tooltip if flag is disabled', async () => {
      loadTimeData.overrideValues({contextualTasksUnboundedMenuEnabled: false});

      const profileIndicator =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#profileIndicator');
      assertTrue(!!profileIndicator);
      const tooltipEl =
          topToolbar.shadowRoot.querySelector('#profileTooltip')
              ?.shadowRoot?.querySelector<UnboundedElement>('#tooltip');
      assertTrue(!!tooltipEl);

      let showUnboundedCalled = false;
      tooltipEl.showUnboundedElement = () => {
        showUnboundedCalled = true;
        return Promise.resolve();
      };

      const mockTimer = new MockTimer();
      mockTimer.install();
      profileIndicator.dispatchEvent(new PointerEvent('pointerenter'));
      mockTimer.tick(500);
      mockTimer.uninstall();
      await microtasksFinished();

      assertFalse(showUnboundedCalled);
      assertFalse(tooltipEl.hasAttribute('unbounded'));
      assertTrue(tooltipEl.hasAttribute('open'));
    });
  });
});
