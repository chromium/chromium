// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/top_toolbar.js';
import 'chrome://contextual-tasks/sources_menu.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {ToolbarBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_toolbar_browser_proxy.js';
import type {ContextualTasksFaviconGroupElement} from 'chrome://contextual-tasks/favicon_group.js';
import type {TopToolbarElement} from 'chrome://contextual-tasks/top_toolbar.js';
import type {UnboundedDialog} from 'chrome://contextual-tasks/utils.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertHTMLElement} from './contextual_tasks_test_utils.js';
import {TestContextualTasksBrowserProxy, TestToolbarBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

// clang-format off
// <if expr="not is_android">
import {LhsChipIdentifier} from 'chrome://contextual-tasks/toolbar_ui_api_data_model.mojom-webui.js';

import {createFakePermissionChip as createFakeChip} from './test_contextual_tasks_browser_proxy.js';
import type {TestContextualTasksToolbarUiService} from './test_contextual_tasks_browser_proxy.js';
// </if>
// clang-format on

suite('TopToolbarTest', () => {
  let topToolbar: TopToolbarElement;
  let proxy: TestContextualTasksBrowserProxy;
  let toolbarProxy: TestToolbarBrowserProxy;

  setup(() => {
    proxy = new TestContextualTasksBrowserProxy(
        'chrome://webui-test/contextual_tasks/test.html');
    BrowserProxyImpl.setInstance(proxy);
    toolbarProxy = new TestToolbarBrowserProxy();
    ToolbarBrowserProxyImpl.setInstance(toolbarProxy);
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: false,
      contextualTasksEnableSpatialModelToolbarLayout: false,
      contextualTasksUnboundedMenuEnabled: false,
      contextualTasksSidePanelRearchitectureEnabled: false,
    });
  });

  test('updates contextInfos when onContextUpdated fires', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    topToolbar = document.createElement('top-toolbar');
    document.body.appendChild(topToolbar);
    await microtasksFinished();

    toolbarProxy.callbackRouterRemote.onContextUpdated([{
      tab: {
        url: 'https://example.com',
        title: 'Example',
        tabId: 1,
        hasChromeTabData: false,
      },
    }]);
    await microtasksFinished();

    assertEquals(1, topToolbar.contextInfos.length);
    assertEquals('Example', topToolbar.contextInfos[0]!.tab!.title);
  });

  test(
      'updates expand button visibility when setExpandButtonEnabled fires',
      async () => {
        loadTimeData.overrideValues({expandButtonEnabled: false});
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        topToolbar = document.createElement('top-toolbar');
        document.body.appendChild(topToolbar);
        await microtasksFinished();

        assertFalse(
            !!topToolbar.shadowRoot.querySelector('#openInNewTabButton'));

        toolbarProxy.callbackRouterRemote.setExpandButtonEnabled(true);
        await microtasksFinished();

        assertTrue(
            !!topToolbar.shadowRoot.querySelector('#openInNewTabButton'));

        toolbarProxy.callbackRouterRemote.setExpandButtonEnabled(false);
        await microtasksFinished();

        assertFalse(
            !!topToolbar.shadowRoot.querySelector('#openInNewTabButton'));
      });

  (loadTimeData.getBoolean('isSmallDeviceFormFactor') ?
       suite.skip :
       suite)('Expand button enabled', () => {
    setup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;

      loadTimeData.overrideValues({expandButtonEnabled: true});

      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
    });

    test('shows correct logo', () => {
      // <if expr="_google_chrome">
      const logo = topToolbar.shadowRoot.querySelector<HTMLImageElement>(
          '.top-toolbar-logo');
      assertHTMLElement(logo);
      assertEquals(
          logo.src,
          'chrome://resources/cr_components/searchbox/icons/google_g_gradient.svg');
      // </if>
      // <if expr="not _google_chrome">
      const lightLogo = topToolbar.shadowRoot.querySelector<HTMLImageElement>(
          '.chrome-logo-light');
      assertHTMLElement(lightLogo);
      assertEquals(
          lightLogo.src,
          'chrome://resources/cr_components/searchbox/icons/chrome_product_cr23.svg');
      const darkLogo = topToolbar.shadowRoot.querySelector<HTMLImageElement>(
          '.chrome-logo-dark');
      assertHTMLElement(darkLogo);
      assertEquals(
          darkLogo.src, 'chrome://resources/images/chrome_logo_dark.svg');
      // </if>
    });

    test('handles new thread button click', async () => {
      const newThreadButton = topToolbar.$.newThreadButton;
      assertTrue(!!newThreadButton);
      const newThreadEvent = eventToPromise('new-thread-click', topToolbar);
      newThreadButton.click();
      await newThreadEvent;
    });

    test('handles thread history button click', async () => {
      topToolbar.isAiPage = true;
      await microtasksFinished();

      const historyButton = topToolbar.$.threadHistoryButton;
      assertTrue(!!historyButton);
      historyButton.click();
      await toolbarProxy.handler.whenCalled('showThreadHistory');
    });

    test('hides thread history button on SRP', async () => {
      const historyButton = topToolbar.$.threadHistoryButton;
      assertTrue(!!historyButton);

      topToolbar.isAiPage = false;
      await microtasksFinished();
      assertTrue(historyButton.hidden);

      topToolbar.isAiPage = true;
      await microtasksFinished();
      assertFalse(historyButton.hidden);
    });

    test('history button visibility with eligibility and signin', async () => {
      const historyButton = topToolbar.$.threadHistoryButton;
      assertTrue(!!historyButton);

      topToolbar.isAiPage = true;

      // Case 1: Signed In -> Visible
      topToolbar.isUserSignedIn = true;
      await microtasksFinished();
      assertFalse(historyButton.hidden);

      // Case 2: Signed Out -> Hidden
      topToolbar.isUserSignedIn = false;
      await microtasksFinished();
      assertTrue(historyButton.hidden);
    });

    test('handles close button click', async () => {
      const closeButton = topToolbar.$.closeButton;
      assertTrue(!!closeButton);
      closeButton.click();
      await toolbarProxy.handler.whenCalled('closeSidePanel');
    });

    test('toggles sources button visibility', async () => {
      const sourcesButton =
          topToolbar.shadowRoot
              .querySelector<ContextualTasksFaviconGroupElement>('#sources');
      assertTrue(!!sourcesButton);

      // Initially, there are no attached tabs, so the sources button should be
      // hidden and contain no items.
      assertTrue(sourcesButton.hidden);
      assertFalse(!!sourcesButton.shadowRoot.querySelector('.favicon-item'));

      topToolbar.contextInfos = [{
        tab: {
          title: 'Tab 1',
          url: 'https://example.com',
          hasChromeTabData: false,
          tabId: 1,
        },
      }];
      await microtasksFinished();

      // After attaching a tab, the sources button should be visible and contain
      // items.
      assertFalse(sourcesButton.hidden);
      assertTrue(!!sourcesButton.shadowRoot.querySelector('.favicon-item'));
    });


    test('handles tab sources menu interactions', async () => {
      const tab = {
        title: 'Sample Tab',
        url: 'https://example/sample.html',
        hasChromeTabData: false,
        tabId: 1,
      };
      topToolbar.contextInfos = [{tab: tab}];
      await microtasksFinished();

      const sourcesButton =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#sources');
      assertTrue(!!sourcesButton);
      sourcesButton.click();
      await microtasksFinished();

      const sourcesMenuElement = topToolbar.$.sourcesMenu.get();
      const crActionMenu =
          sourcesMenuElement.shadowRoot.querySelector('cr-action-menu');
      assertTrue(!!crActionMenu);
      assertTrue(crActionMenu.open);

      const headers = sourcesMenuElement.shadowRoot.querySelectorAll('.header');
      assertEquals(1, headers.length);

      // Click the first tab item.
      const tabItem = sourcesMenuElement.shadowRoot.querySelector<HTMLElement>(
          'cr-url-list-item.dropdown-item');
      assertTrue(!!tabItem);
      tabItem.click();

      const [tabId, url] =
          await toolbarProxy.handler.whenCalled('onTabClickedFromSourcesMenu');
      assertEquals(1, tabId);
      assertDeepEquals(url, tab.url);
    });

    test('handles file sources menu interactions', async () => {
      const file = {
        title: 'Sample Document',
        url: 'https://example/sample.pdf',
      };
      topToolbar.contextInfos = [{file: file}];
      await microtasksFinished();

      const sourcesButton =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#sources');
      assertTrue(!!sourcesButton);
      sourcesButton.click();
      await microtasksFinished();

      const sourcesMenuElement = topToolbar.$.sourcesMenu.get();
      const crActionMenu =
          sourcesMenuElement.shadowRoot.querySelector('cr-action-menu');
      assertTrue(!!crActionMenu);
      assertTrue(crActionMenu.open);

      const headers = sourcesMenuElement.shadowRoot.querySelectorAll('.header');
      assertEquals(1, headers.length);

      // Click the first file item.
      const fileItem = sourcesMenuElement.shadowRoot.querySelector<HTMLElement>(
          'cr-url-list-item.dropdown-item');
      assertTrue(!!fileItem);
      fileItem.click();

      const url =
          await toolbarProxy.handler.whenCalled('onFileClickedFromSourcesMenu');
      assertDeepEquals(url, file.url);
    });

    test('handles image sources menu interactions', async () => {
      const image = {
        title: 'Test Image',
        url: 'https://www.example.com/example.jpeg',
      };
      topToolbar.contextInfos = [{image: image}];
      await microtasksFinished();

      const sourcesButton =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#sources');
      assertTrue(!!sourcesButton);
      sourcesButton.click();
      await microtasksFinished();

      const sourcesMenuElement = topToolbar.$.sourcesMenu.get();

      const crActionMenu =
          sourcesMenuElement.shadowRoot.querySelector('cr-action-menu');
      assertTrue(!!crActionMenu);
      assertTrue(crActionMenu.open);

      const headers = sourcesMenuElement.shadowRoot.querySelectorAll('.header');
      assertEquals(1, headers.length);

      // Click the first image item.
      const imageItem =
          sourcesMenuElement.shadowRoot.querySelector<HTMLElement>(
              'cr-url-list-item.dropdown-item');
      assertTrue(!!imageItem);
      imageItem.click();

      const url = await toolbarProxy.handler.whenCalled(
          'onImageClickedFromSourcesMenu');
      assertDeepEquals(url, image.url);
    });

    test('handles open in new tab click', async () => {
      topToolbar.enableOpenInNewTabButton = true;
      await microtasksFinished();

      const openInNewTabButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#openInNewTabButton');
      assertTrue(!!openInNewTabButton);
      assertFalse(openInNewTabButton.disabled);
      openInNewTabButton.click();
      await toolbarProxy.handler.whenCalled('moveTaskUiToNewTab');

      topToolbar.enableOpenInNewTabButton = false;
      await microtasksFinished();
      assertTrue(openInNewTabButton.disabled);
      toolbarProxy.handler.reset();
      openInNewTabButton.click();
      assertEquals(0, toolbarProxy.handler.getCallCount('moveTaskUiToNewTab'));
    });

    test('shows 3 tab icons without number for 3 tabs', async () => {
      const sourcesButton =
          topToolbar.shadowRoot
              .querySelector<ContextualTasksFaviconGroupElement>('#sources');
      assertTrue(!!sourcesButton);

      topToolbar.contextInfos = [
        {
          tab: {
            title: 'Tab 1',
            url: 'https://example.com/1',
            hasChromeTabData: false,
            tabId: 1,
          },
        },
        {
          tab: {
            title: 'Tab 2',
            url: 'https://example.com/2',
            hasChromeTabData: false,
            tabId: 2,
          },
        },
        {
          tab: {
            title: 'Tab 3',
            url: 'https://example.com/3',
            hasChromeTabData: false,
            tabId: 3,
          },
        },
      ];
      await microtasksFinished();

      const faviconItems =
          sourcesButton.shadowRoot.querySelectorAll('.favicon-item');
      assertEquals(faviconItems.length, 3);
      assertFalse(!!sourcesButton.shadowRoot.querySelector('.more-items'));
    });

    test('shows 3 tab icons with number for 4 tabs', async () => {
      const sourcesButton =
          topToolbar.shadowRoot
              .querySelector<ContextualTasksFaviconGroupElement>('#sources');
      assertTrue(!!sourcesButton);

      topToolbar.contextInfos = [
        {
          tab: {
            title: 'Tab 1',
            url: 'https://example.com/1',
            hasChromeTabData: false,
            tabId: 1,
          },
        },
        {
          tab: {
            title: 'Tab 2',
            url: 'https://example.com/2',
            hasChromeTabData: false,
            tabId: 2,
          },
        },
        {
          tab: {
            title: 'Tab 3',
            url: 'https://example.com/3',
            hasChromeTabData: false,
            tabId: 3,
          },
        },
        {
          tab: {
            title: 'Tab 4',
            url: 'https://example.com/4',
            hasChromeTabData: false,
            tabId: 4,
          },
        },
      ];
      await microtasksFinished();

      const faviconItems = sourcesButton.shadowRoot.querySelectorAll(
          '.favicon-item:not(#more-items)');
      assertEquals(faviconItems.length, 3);
      const moreItems =
          sourcesButton.shadowRoot.querySelector<HTMLElement>('#more-items');
      assertTrue(!!moreItems);
    });

    test(
        'logo click does not trigger page info when flag disabled',
        async () => {
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          loadTimeData.overrideValues({
            contextualTasksSidePanelRearchitectureEnabled: false,
          });
          topToolbar = document.createElement('top-toolbar');
          document.body.appendChild(topToolbar);
          await microtasksFinished();

          const logo = topToolbar.shadowRoot.querySelector<HTMLElement>(
              '.top-toolbar-logo-button');
          assertHTMLElement(logo);
          assertFalse(logo.classList.contains('clickable'));

          logo.click();
          assertEquals(
              0, toolbarProxy.handler.getCallCount('showPageInfoBubble'));
        });

    test(
        'logo pointer click triggers page info when flag enabled', async () => {
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          loadTimeData.overrideValues({
            contextualTasksSidePanelRearchitectureEnabled: true,
          });
          topToolbar = document.createElement('top-toolbar');
          document.body.appendChild(topToolbar);
          await microtasksFinished();

          const logo = topToolbar.shadowRoot.querySelector<HTMLElement>(
              '.top-toolbar-logo-button');
          assertHTMLElement(logo);
          assertTrue(logo.classList.contains('clickable'));

          logo.dispatchEvent(new PointerEvent('click', {pointerType: 'mouse'}));
          const isPointer =
              await toolbarProxy.handler.whenCalled('showPageInfoBubble');
          assertTrue(isPointer);
        });

    test('logo enter key triggers page info when flag enabled', async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        contextualTasksSidePanelRearchitectureEnabled: true,
      });
      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();

      const logo = topToolbar.shadowRoot.querySelector<HTMLElement>(
          '.top-toolbar-logo-button');
      assertHTMLElement(logo);

      logo.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
      const isPointer =
          await toolbarProxy.handler.whenCalled('showPageInfoBubble');
      assertFalse(isPointer);
    });

    test('logo space key triggers page info when flag enabled', async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        contextualTasksSidePanelRearchitectureEnabled: true,
      });
      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();

      const logo = topToolbar.shadowRoot.querySelector<HTMLElement>(
          '.top-toolbar-logo-button');
      assertHTMLElement(logo);

      logo.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
      logo.dispatchEvent(new KeyboardEvent('keyup', {key: ' '}));
      const isPointer =
          await toolbarProxy.handler.whenCalled('showPageInfoBubble');
      assertFalse(isPointer);
    });

    test(
        'logo pointerdown triggers onLogoPointerDown when flag enabled',
        async () => {
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          loadTimeData.overrideValues({
            contextualTasksSidePanelRearchitectureEnabled: true,
          });
          topToolbar = document.createElement('top-toolbar');
          document.body.appendChild(topToolbar);
          await microtasksFinished();

          const logo = topToolbar.shadowRoot.querySelector<HTMLElement>(
              '.top-toolbar-logo-button');
          assertHTMLElement(logo);

          logo.dispatchEvent(new PointerEvent('pointerdown'));
          await toolbarProxy.handler.whenCalled('onLogoPointerDown');
        });

    test(
        'logo pointerdown does not trigger onLogoPointerDown when flag ' +
            'disabled',
        async () => {
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          loadTimeData.overrideValues({
            contextualTasksSidePanelRearchitectureEnabled: false,
          });
          topToolbar = document.createElement('top-toolbar');
          document.body.appendChild(topToolbar);
          await microtasksFinished();

          const logo = topToolbar.shadowRoot.querySelector<HTMLElement>(
              '.top-toolbar-logo-button');
          assertHTMLElement(logo);

          logo.dispatchEvent(new PointerEvent('pointerdown'));
          assertEquals(
              0, toolbarProxy.handler.getCallCount('onLogoPointerDown'));
        });

    test('logo container has inline-end margin when flag enabled', async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        contextualTasksSidePanelRearchitectureEnabled: true,
      });
      topToolbar = document.createElement('top-toolbar');
      topToolbar.title = 'Sample Thread Title';
      document.body.appendChild(topToolbar);
      await microtasksFinished();

      const logoContainer = topToolbar.shadowRoot.querySelector<HTMLElement>(
          '.top-toolbar-logo-container');
      assertHTMLElement(logoContainer);
      const computedStyle = getComputedStyle(logoContainer);
      assertEquals('12px', computedStyle.marginInlineEnd);
      assertEquals('12px', computedStyle.marginRight);
    });
  });

  suite('Pinning', () => {
    setup(async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        enablePinButton: true,
        isAiPage: true,
        isAimEligible: true,
        pinTooltip: 'Pin side panel',
        unpinTooltip: 'Unpin side panel',
      });
      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();
    });

    test('hides pin button when not on AI page', async () => {
      topToolbar.isAiPage = false;
      await microtasksFinished();

      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);
      moreButton.click();
      await microtasksFinished();

      const menu = topToolbar.$.overflowMenu.get();
      const pinButton =
          menu.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertFalse(!!pinButton);
    });

    test('hides pin button when pin button is not enabled', async () => {
      topToolbar.isPinButtonEnabled = false;
      await microtasksFinished();

      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);
      moreButton.click();
      await microtasksFinished();

      const menu = topToolbar.$.overflowMenu.get();
      const pinButton =
          menu.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertFalse(!!pinButton);
    });

    test('handles pin button click', async () => {
      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);
      moreButton.click();
      await microtasksFinished();

      const menu = topToolbar.$.overflowMenu.get();
      const pinButton =
          menu.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertTrue(!!pinButton);

      // Initially unpinned.
      assertEquals(
          pinButton.innerText.trim(), loadTimeData.getString('pinTooltip'));

      pinButton.click();
      await toolbarProxy.handler.whenCalled('pinSidePanel');
    });

    test('handles unpin button click', async () => {
      // Simulate pinned state.
      toolbarProxy.callbackRouterRemote.onSidePanelPinStateChanged(true);
      await toolbarProxy.callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);
      moreButton.click();
      await microtasksFinished();

      const menu = topToolbar.$.overflowMenu.get();
      const pinButton =
          menu.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertTrue(!!pinButton);

      // Now pinned.
      assertEquals(
          pinButton.innerText.trim(), loadTimeData.getString('unpinTooltip'));

      pinButton.click();
      await toolbarProxy.handler.whenCalled('unpinSidePanel');
    });

    test('updates pin state via Mojo', async () => {
      toolbarProxy.callbackRouterRemote.onSidePanelPinStateChanged(true);
      await toolbarProxy.callbackRouterRemote.$.flushForTesting();
      await topToolbar.updateComplete;

      // Note: isPinned property on topToolbar is protected. We check via unpin
      // button in menu.
      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);
      moreButton.click();
      await microtasksFinished();

      const menu = topToolbar.$.overflowMenu.get();
      const pinButton =
          menu.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertTrue(!!pinButton);
      assertEquals(
          pinButton.innerText.trim(), loadTimeData.getString('unpinTooltip'));

      // Toggling back to unpinned should update the button text.
      toolbarProxy.callbackRouterRemote.onSidePanelPinStateChanged(false);
      await toolbarProxy.callbackRouterRemote.$.flushForTesting();
      await topToolbar.updateComplete;
      await microtasksFinished();

      assertEquals(
          pinButton.innerText.trim(), loadTimeData.getString('pinTooltip'));
    });

    test('pin state listener is removed on disconnect', async () => {
      topToolbar.remove();

      toolbarProxy.callbackRouterRemote.onSidePanelPinStateChanged(true);
      await toolbarProxy.callbackRouterRemote.$.flushForTesting();
      await topToolbar.updateComplete;

      assertFalse((topToolbar as any).isPinned);
    });
  });

  suite('Expand button and menu for lens flows disabled', () => {
    setup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;

      loadTimeData.overrideValues({
        expandButtonEnabled: false,
        hideMenuOnAiPageEnabled: false,
        isAiPage: true,
        enablePinButton: false,
        contextualTasksEnableSpatialModelToolbarLayout: false,
      });

      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
    });

    const isPhone = loadTimeData.getBoolean('isSmallDeviceFormFactor');

    test('menu button visibility independent of ai page state', async () => {
      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);

      // Initially visible because hideMenuOnAiPageEnabled is false, even
      // though isAiPage is initialized to true.
      assertTrue(topToolbar.isAiPage);
      assertFalse(moreButton.hidden);

      topToolbar.isAiPage = false;
      await microtasksFinished();
      assertFalse(moreButton.hidden);
    });

    test('close button does not have rounded-corner attribute', () => {
      const closeButton = topToolbar.$.closeButton;
      assertTrue(!!closeButton);
      assertFalse(closeButton.hasAttribute('rounded-corner'));
    });

    (isPhone ? test.skip : test)(
        'handles open in new tab click in menu', async () => {
          topToolbar.enableOpenInNewTabButton = true;
          await microtasksFinished();

          const overflowMenuButton =
              topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
                  '#overflowMenuButton');
          assertTrue(!!overflowMenuButton);
          overflowMenuButton.click();
          await microtasksFinished();

          const menu = topToolbar.$.overflowMenu.get();
          assertTrue(menu.shadowRoot.querySelector('cr-action-menu')!.open);

          const buttons = menu.shadowRoot.querySelectorAll('button');
          const openInNewTabButton = buttons[0];
          assertTrue(!!openInNewTabButton);
          assertFalse(openInNewTabButton.disabled);
          openInNewTabButton.click();
          await toolbarProxy.handler.whenCalled('moveTaskUiToNewTab');

          topToolbar.enableOpenInNewTabButton = false;
          await microtasksFinished();
          assertTrue(openInNewTabButton.disabled);
        });

    test('handles my activity click', async () => {
      const overflowMenuButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!overflowMenuButton);
      overflowMenuButton.click();
      await microtasksFinished();

      const menu = topToolbar.$.overflowMenu.get();
      const buttons = menu.shadowRoot.querySelectorAll('button');
      const myActivityButton = buttons[1];
      assertTrue(!!myActivityButton);
      myActivityButton.click();
      await toolbarProxy.handler.whenCalled('openMyActivityUi');
    });

    test('handles help click', async () => {
      const overflowMenuButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!overflowMenuButton);
      overflowMenuButton.click();
      await microtasksFinished();

      const menu = topToolbar.$.overflowMenu.get();
      const buttons = menu.shadowRoot.querySelectorAll('button');
      const helpButton = buttons[2];
      assertTrue(!!helpButton);
      helpButton.click();
      await toolbarProxy.handler.whenCalled('openFeedbackUi');
    });

    test('calls maybeTriggerPinningPromo when AI page is shown', async () => {
      topToolbar.isAiPage = false;
      await microtasksFinished();
      toolbarProxy.handler.reset();

      topToolbar.isAiPage = true;
      await microtasksFinished();

      // <if expr="is_android">
      assertEquals(
          0, toolbarProxy.handler.getCallCount('maybeTriggerPinningPromo'));
      // </if>
      // <if expr="not is_android">
      await toolbarProxy.handler.whenCalled('maybeTriggerPinningPromo');
      // </if>
    });

    test(
        'does not call maybeTriggerPinningPromo when onboarding tooltip is showing',
        async () => {
          topToolbar.isAiPage = false;
          topToolbar.onboardingTooltipShowing = true;
          await microtasksFinished();
          toolbarProxy.handler.reset();

          topToolbar.isAiPage = true;
          await microtasksFinished();

          assertEquals(
              0, toolbarProxy.handler.getCallCount('maybeTriggerPinningPromo'));
        });
  });

  (loadTimeData.getBoolean('isSmallDeviceFormFactor') ?
       suite.skip :
       suite)('Menu for lens flows only', () => {
    setup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;

      loadTimeData.overrideValues({
        expandButtonEnabled: false,
        hideMenuOnAiPageEnabled: true,
        isAiPage: true,
        contextualTasksEnableSpatialModelToolbarLayout: false,
      });

      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
    });

    test('hides overflow menu button on ai page, shown for lens', async () => {
      const overflowMenuButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!overflowMenuButton);

      // Hidden initially because `isAiPage` is initialized to true via
      // loadTimeData.
      assertTrue(topToolbar.isAiPage);
      assertTrue(overflowMenuButton.hidden);

      topToolbar.isAiPage = false;
      await microtasksFinished();
      assertFalse(overflowMenuButton.hidden);

      topToolbar.isAiPage = true;
      await microtasksFinished();
      assertTrue(overflowMenuButton.hidden);
    });
  });

  test('shows mixed context types in favicon group', async () => {
    // topToolbar is already defined in the outer suite.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    topToolbar = document.createElement('top-toolbar');
    document.body.appendChild(topToolbar);

    const sourcesButton =
        topToolbar.shadowRoot.querySelector<ContextualTasksFaviconGroupElement>(
            '#sources');
    assertTrue(!!sourcesButton);

    topToolbar.contextInfos = [
      {
        tab: {
          title: 'Tab 1',
          url: 'https://example.com/1',
          hasChromeTabData: false,
          tabId: 1,
        },
      },
      {
        file: {
          title: 'Sample Document',
          url: 'https://example/sample.pdf',
        },
      },
      {
        image: {
          title: 'Test Image',
          url: 'https://www.example.com/example.jpeg',
        },
      },
      {
        tab: {
          title: 'Tab 2',
          url: 'https://example.com/2',
          hasChromeTabData: false,
          tabId: 2,
        },
      },
    ];
    await microtasksFinished();

    assertFalse(sourcesButton.hidden);
    const faviconItems =
        sourcesButton.shadowRoot.querySelectorAll('.favicon-item');
    assertEquals(faviconItems.length, 4);  // 3 items + more

    const items = sourcesButton.shadowRoot.querySelectorAll<HTMLElement>(
        '.favicon-item:not(#more-items)');
    assertEquals(items.length, 3);

    // Check item 1 (tab).
    const item1 = items[0];
    assertHTMLElement(item1);
    assertTrue(item1.classList.contains('favicon-item'));
    assertFalse(
        item1.classList.contains('file-icon'));  // Ensure it's not a file icon
    assertTrue(item1.tagName !== 'CR-ICON');

    // Check item 2 (file).
    const item2 = items[1];
    assertHTMLElement(item2);  // Assert first
    assertTrue(item2.classList.contains('favicon-item'));
    assertTrue(item2.classList.contains('file-icon'));
    assertEquals(item2.tagName, 'CR-ICON');
    assertEquals(
        (item2 as CrIconElement).icon,
        loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
            'contextual_tasks:drive-pdf-filled' :
            'contextual_tasks:pdf-old');

    // Check item 3 (image).
    const item3 = items[2];
    assertHTMLElement(item3);  // Assert first
    assertTrue(item3.classList.contains('favicon-item'));
    assertEquals(item3.tagName, 'CR-ICON');
    assertEquals(
        (item3 as CrIconElement).icon,
        loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
            'contextual_tasks:image' :
            'contextual_tasks:img_icon-old');

    const moreItems =
        sourcesButton.shadowRoot.querySelector<HTMLElement>('#more-items');
    assertTrue(!!moreItems);
    assertEquals(moreItems.innerText, '+1');
  });


  test('hides new thread button when isAimEligible is false', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({isAimEligible: false});

    topToolbar = document.createElement('top-toolbar');
    document.body.appendChild(topToolbar);
    await microtasksFinished();

    const newThreadButton = topToolbar.$.newThreadButton;
    assertTrue(!!newThreadButton);
    assertTrue(newThreadButton.hidden);
  });

  test('highlights overflow menu button when menu is open', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    topToolbar = document.createElement('top-toolbar');
    document.body.appendChild(topToolbar);
    await microtasksFinished();

    const overflowMenuButton =
        topToolbar.shadowRoot.querySelector<HTMLElement>('#overflowMenuButton');
    assertTrue(!!overflowMenuButton);
    assertFalse(overflowMenuButton.classList.contains('active'));

    const menu = topToolbar.$.overflowMenu.get();
    // Stubbed so the test never depends on real platform surface support.
    // The unbounded lifecycle itself is covered by unbounded_menu_test.ts.
    const dialogEl = menu.$.menu.getDialog() as UnboundedDialog;
    dialogEl.showUnboundedElement = () => Promise.resolve();
    dialogEl.hideUnboundedElement = () => Promise.resolve();

    // Open overflow menu
    overflowMenuButton.click();
    await microtasksFinished();

    assertTrue(overflowMenuButton.classList.contains('active'));

    // Close overflow menu
    menu.close();
    await microtasksFinished();

    assertFalse(overflowMenuButton.classList.contains('active'));
  });

  test('closes overflow menu when the side panel loses focus', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    topToolbar = document.createElement('top-toolbar');
    document.body.appendChild(topToolbar);
    await microtasksFinished();

    const overflowMenuButton =
        topToolbar.shadowRoot.querySelector<HTMLElement>('#overflowMenuButton');
    assertTrue(!!overflowMenuButton);

    overflowMenuButton.click();
    await microtasksFinished();
    assertTrue(topToolbar.$.overflowMenu.get().$.menu.open);

    // Clicking outside of the side panel (e.g. on the page contents, the Lens
    // crop frame, or a search result) blurs the side panel's window.
    window.dispatchEvent(new Event('blur'));
    await microtasksFinished();
    assertFalse(topToolbar.$.overflowMenu.get().$.menu.open);
  });

  // <if expr="not is_android">
  suite('Permission Dashboard Integration', () => {
    let toolbarUiService: TestContextualTasksToolbarUiService;

    setup(async () => {
      loadTimeData.overrideValues({
        contextualTasksSidePanelRearchitectureEnabled: true,
      });
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      toolbarUiService = toolbarProxy.toolbarUiService;
      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();
    });

    test(
        'hides and shows permission-dashboard, and hides and shows logo' +
            ' container based on state of request chip',
        async () => {
          const topRow = topToolbar.shadowRoot.querySelector('#top-row');
          assertTrue(!!topRow);
          const logoContainer =
              topRow.querySelector<HTMLElement>('.top-toolbar-logo-container');
          assertHTMLElement(logoContainer);

          // Initial state: `G` logo shows.
          assertFalse(!!topRow.querySelector('permission-dashboard'));
          assertFalse(logoContainer.hidden);

          // Create request chip.
          const fakeState = {
            indicatorChip: createFakeChip(),
            requestChip:
                createFakeChip({isVisible: true, iconName: 'kMicIcon'}),
            isDividerVisible: false,
          };

          // Verify `topToolbar` DOM (dashboard rendered, logo container hidden)
          // when request chip shows.
          topToolbar.permissionDashboardState = fakeState;
          await microtasksFinished();

          const dashboard = topRow.querySelector<HTMLElement&{delegate: any}>(
              'permission-dashboard');
          assertTrue(!!dashboard);
          assertTrue(!!dashboard.delegate);
          assertTrue(logoContainer.hidden);

          // Remove the visible chip, so that the permission dashboard no longer
          // shows and the `G` logo is restored.
          topToolbar.permissionDashboardState = {
            indicatorChip: createFakeChip(),
            requestChip: createFakeChip(),
            isDividerVisible: false,
          };
          await microtasksFinished();

          assertFalse(!!topRow.querySelector('permission-dashboard'));
          assertFalse(logoContainer.hidden);

          // Set state to dashboard to null. It should no longer show.
          topToolbar.permissionDashboardState = null;
          await microtasksFinished();

          assertFalse(!!topRow.querySelector('permission-dashboard'));
          assertFalse(logoContainer.hidden);
        });

    test(
        'hides and shows permission-dashboard, and hides and shows logo' +
            ' container based on state of indicator chip',
        async () => {
          const topRow = topToolbar.shadowRoot.querySelector('#top-row');
          assertTrue(!!topRow);
          const logoContainer =
              topRow.querySelector<HTMLElement>('.top-toolbar-logo-container');
          assertHTMLElement(logoContainer);

          // Initial state: `G` logo shows.
          assertFalse(!!topRow.querySelector('permission-dashboard'));
          assertFalse(logoContainer.hidden);

          // Create indicator chip.
          const fakeState = {
            indicatorChip:
                createFakeChip({isVisible: true, iconName: 'kCameraIcon'}),
            requestChip: createFakeChip(),
            isDividerVisible: false,
          };

          // Verify `topToolbar` DOM (dashboard rendered, logo container hidden)
          // when indicator chip shows.
          topToolbar.permissionDashboardState = fakeState;
          await microtasksFinished();

          const dashboard = topRow.querySelector<HTMLElement&{delegate: any}>(
              'permission-dashboard');
          assertTrue(!!dashboard);
          assertTrue(!!dashboard.delegate);
          assertTrue(logoContainer.hidden);

          // Remove the visible chip, so that the permission dashboard no longer
          // shows and the `G` logo is restored.
          topToolbar.permissionDashboardState = {
            indicatorChip: createFakeChip(),
            requestChip: createFakeChip(),
            isDividerVisible: false,
          };
          await microtasksFinished();

          assertFalse(!!topRow.querySelector('permission-dashboard'));
          assertFalse(logoContainer.hidden);
        });

    test(
        'loads initial state and updates on onPermissionDashboardStateChanged',
        async () => {
          await toolbarUiService.whenCalled('getInitialState');

          const topRow = topToolbar.shadowRoot.querySelector('#top-row');
          assertTrue(!!topRow);
          assertFalse(!!topRow.querySelector('permission-dashboard'));

          // Push an update over the observer callback router.
          const observerRemote = toolbarProxy.bindToolbarUiObserverRemote();
          const pushedState = {
            indicatorChip: createFakeChip(),
            requestChip:
                createFakeChip({isVisible: true, iconName: 'kMicIcon'}),
            isDividerVisible: false,
          };
          observerRemote.onPermissionDashboardStateChanged(pushedState);
          await observerRemote.$.flushForTesting();
          await microtasksFinished();

          assertDeepEquals(pushedState, topToolbar.permissionDashboardState);
          assertTrue(!!topRow.querySelector('permission-dashboard'));
        });

    test('forwards permission chip delegate calls to service', async () => {
      topToolbar.permissionDashboardState = {
        indicatorChip: createFakeChip(),
        requestChip: createFakeChip({isVisible: true, iconName: 'kMicIcon'}),
        isDividerVisible: false,
      };
      await microtasksFinished();

      const topRow = topToolbar.shadowRoot.querySelector('#top-row');
      assertTrue(!!topRow);
      const dashboard = topRow.querySelector<HTMLElement&{delegate: any}>(
          'permission-dashboard');
      assertTrue(!!dashboard);
      assertTrue(!!dashboard.delegate);

      dashboard.delegate.onChipClicked(
          LhsChipIdentifier.kPermissionRequest, true);
      assertDeepEquals(
          [LhsChipIdentifier.kPermissionRequest, true],
          await toolbarUiService.whenCalled('onChipClicked'));

      dashboard.delegate.onChipPointerEntered(
          LhsChipIdentifier.kPermissionRequest);
      assertEquals(
          LhsChipIdentifier.kPermissionRequest,
          await toolbarUiService.whenCalled('onChipPointerEntered'));

      dashboard.delegate.onChipPointerExited(
          LhsChipIdentifier.kPermissionRequest);
      assertEquals(
          LhsChipIdentifier.kPermissionRequest,
          await toolbarUiService.whenCalled('onChipPointerExited'));

      dashboard.delegate.onChipMousePressed(
          LhsChipIdentifier.kPermissionRequest);
      assertEquals(
          LhsChipIdentifier.kPermissionRequest,
          await toolbarUiService.whenCalled('onChipMousePressed'));

      dashboard.delegate.onChipExpandAnimationEnded(
          LhsChipIdentifier.kPermissionIndicator);
      assertEquals(
          LhsChipIdentifier.kPermissionIndicator,
          await toolbarUiService.whenCalled('onChipExpandAnimationEnded'));

      dashboard.delegate.onChipCollapseAnimationEnded(
          LhsChipIdentifier.kPermissionIndicator);
      assertEquals(
          LhsChipIdentifier.kPermissionIndicator,
          await toolbarUiService.whenCalled('onChipCollapseAnimationEnded'));
    });

    test(
        'registers super G button help bubble anchor only when logo is shown',
        () => {
          const anchorStatuses = topToolbar.getSortedAnchorStatusesForTesting();
          const gButtonStatus = anchorStatuses.find(
              ([id]: [string, boolean]) =>
                  id === 'kContextualTasksSuperGButtonElementId');
          assertTrue(!!gButtonStatus);
          assertTrue(gButtonStatus[1]);
        });
  });
  // </if>

  suite('Profile indicator', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        contextualTasksSidePanelRearchitectureEnabled: true,
      });
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();
    });

    test(
        'hides profile indicator when rearchitecture is disabled', async () => {
          loadTimeData.overrideValues({
            contextualTasksSidePanelRearchitectureEnabled: false,
          });
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          topToolbar = document.createElement('top-toolbar');
          topToolbar.profileAvatarUrl = 'data:image/png;base64,fakeImageData';
          document.body.appendChild(topToolbar);
          await microtasksFinished();

          const profileIndicator =
              topToolbar.shadowRoot.querySelector('#profileIndicator');
          assertEquals(null, profileIndicator);
        });

    test(
        'shows profile indicator with avatar image when avatar url is set',
        async () => {
          topToolbar.profileAvatarUrl = 'data:image/png;base64,fakeImageData';
          await microtasksFinished();

          const profileIndicator =
              topToolbar.shadowRoot.querySelector<HTMLElement>(
                  '#profileIndicator');
          assertTrue(!!profileIndicator);

          const avatarImg = profileIndicator.querySelector<HTMLImageElement>(
              '.profile-avatar-img');
          assertTrue(!!avatarImg);
          assertEquals('data:image/png;base64,fakeImageData', avatarImg.src);
        });

    test(
        'renders signed-out profile indicator with chrome placeholder avatar',
        async () => {
          topToolbar.profileAvatarUrl =
              'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
          await microtasksFinished();

          const profileIndicator =
              topToolbar.shadowRoot.querySelector<HTMLElement>(
                  '#profileIndicator');
          assertTrue(!!profileIndicator);

          const avatarImg = profileIndicator.querySelector<HTMLImageElement>(
              '.profile-avatar-img');
          assertTrue(!!avatarImg);
          assertEquals(
              'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE',
              avatarImg.src);
        });

    test('updates avatar url via mojo setProfileAvatarUrl', async () => {
      topToolbar.profileAvatarUrl =
          'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
      await microtasksFinished();

      toolbarProxy.callbackRouterRemote.setProfileAvatarUrl(
          'data:image/png;base64,pushedAvatar');
      await microtasksFinished();

      const profileIndicator =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#profileIndicator');
      assertTrue(!!profileIndicator);
      const avatarImg = profileIndicator.querySelector<HTMLImageElement>(
          '.profile-avatar-img');
      assertTrue(!!avatarImg);
      assertEquals('data:image/png;base64,pushedAvatar', avatarImg.src);
    });
  });
});
