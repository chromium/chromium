// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/top_toolbar.js';
import 'chrome://contextual-tasks/sources_menu.js';

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ContextualTasksFaviconGroupElement} from 'chrome://contextual-tasks/favicon_group.js';
import type {SourcesMenuElement} from 'chrome://contextual-tasks/sources_menu.js';
import type {TopToolbarElement} from 'chrome://contextual-tasks/top_toolbar.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {assertHTMLElement} from './test_utils.js';

suite('TopToolbarTest', () => {
  let topToolbar: TopToolbarElement;
  let proxy: TestContextualTasksBrowserProxy;

  setup(() => {
    proxy = new TestContextualTasksBrowserProxy(
        'chrome://webui-test/contextual_tasks/test.html');
    BrowserProxyImpl.setInstance(proxy);
    loadTimeData.overrideValues({contextManagementInComposeboxEnabled: false});
  });

  suite('Expand button enabled', () => {
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
          'chrome://resources/cr_components/searchbox/icons/chrome_product.svg');
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
      await proxy.handler.whenCalled('showThreadHistory');
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

    test('handles close button click', async () => {
      const closeButton = topToolbar.$.closeButton;
      assertTrue(!!closeButton);
      closeButton.click();
      await proxy.handler.whenCalled('closeSidePanel');
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
          tabId: 1,
        },
      }];
      await microtasksFinished();

      // After attaching a tab, the sources button should be visible and contain
      // items.
      assertFalse(sourcesButton.hidden);
      assertTrue(!!sourcesButton.shadowRoot.querySelector('.favicon-item'));
    });

    test('handles sources menu interactions', async () => {
      const tab = {
        title: 'Tab 1',
        url: 'https://example.com',
        tabId: 1,
      };
      topToolbar.contextInfos = [{tab: tab}];
      await microtasksFinished();

      const sourcesButton =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#sources');
      assertTrue(!!sourcesButton);
      sourcesButton.click();
      await microtasksFinished();

      const sourcesMenuElement: SourcesMenuElement =
          topToolbar.$.sourcesMenu.get();
      const crActionMenu =
          sourcesMenuElement.shadowRoot.querySelector<CrActionMenuElement>(
              'cr-action-menu');
      assertTrue(!!crActionMenu);
      assertTrue(crActionMenu.open);

      // The header is "Shared tabs and files".
      const headers = sourcesMenuElement.shadowRoot.querySelectorAll('.header');
      assertEquals(1, headers.length);

      // Click the first tab item.
      const tabItem = sourcesMenuElement.shadowRoot.querySelector<HTMLElement>(
          'cr-url-list-item.dropdown-item');
      assertTrue(!!tabItem);
      tabItem.click();

      const [tabId, url] =
          await proxy.handler.whenCalled('onTabClickedFromSourcesMenu');
      assertEquals(tabId, 1);
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
          await proxy.handler.whenCalled('onFileClickedFromSourcesMenu');
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

      const url =
          await proxy.handler.whenCalled('onImageClickedFromSourcesMenu');
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
      await proxy.handler.whenCalled('moveTaskUiToNewTab');

      topToolbar.enableOpenInNewTabButton = false;
      await microtasksFinished();
      assertTrue(openInNewTabButton.disabled);
      proxy.handler.reset();
      openInNewTabButton.click();
      assertEquals(0, proxy.handler.getCallCount('moveTaskUiToNewTab'));
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
            tabId: 1,
          },
        },
        {
          tab: {
            title: 'Tab 2',
            url: 'https://example.com/2',
            tabId: 2,
          },
        },
        {
          tab: {
            title: 'Tab 3',
            url: 'https://example.com/3',
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
            tabId: 1,
          },
        },
        {
          tab: {
            title: 'Tab 2',
            url: 'https://example.com/2',
            tabId: 2,
          },
        },
        {
          tab: {
            title: 'Tab 3',
            url: 'https://example.com/3',
            tabId: 3,
          },
        },
        {
          tab: {
            title: 'Tab 4',
            url: 'https://example.com/4',
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
  });

  suite('Pinning', () => {
    setup(async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        enablePinButton: true,
        isAiPage: true,
        pinTooltip: 'Pin side panel',
        unpinTooltip: 'Unpin side panel',
      });
      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
      await microtasksFinished();
    });

    test('handles pin button click', async () => {
      const pinButton =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertTrue(!!pinButton);

      // Initially unpinned.
      assertEquals(pinButton.title, 'Pin side panel');

      pinButton.click();
      await proxy.handler.whenCalled('pinSidePanel');
    });

    test('handles unpin button click', async () => {
      // Simulate pinned state.
      proxy.callbackRouterRemote.onSidePanelPinStateChanged(true);
      await microtasksFinished();

      const pinButton =
          topToolbar.shadowRoot.querySelector<HTMLElement>('#pinButton');
      assertTrue(!!pinButton);

      // Now pinned.
      assertEquals(pinButton.title, 'Unpin side panel');

      pinButton.click();
      await proxy.handler.whenCalled('unpinSidePanel');
    });
  });

  suite('Expand button and menu for lens flows disabled', () => {
    setup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;

      loadTimeData.overrideValues({
        expandButtonEnabled: false,
        hideMenuOnAiPageEnabled: false,
        isAiPage: true,
      });

      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
    });

    test('handles more menu interactions', async () => {
      const moreButton =
          topToolbar.shadowRoot.querySelector<CrIconButtonElement>(
              '#overflowMenuButton');
      assertTrue(!!moreButton);
      moreButton.click();
      await microtasksFinished();

      const menu = topToolbar.$.overflowMenu.get();
      assertTrue(menu.shadowRoot.querySelector('cr-action-menu')!.open);

      const buttons = menu.shadowRoot.querySelectorAll('button');
      assertEquals(3, buttons.length);
    });

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

    test('handles open in new tab click in menu', async () => {
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
      await proxy.handler.whenCalled('moveTaskUiToNewTab');

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
      await proxy.handler.whenCalled('openMyActivityUi');
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
      await proxy.handler.whenCalled('openFeedbackUi');
    });
  });

  suite('Menu for lens flows only', () => {
    setup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;

      loadTimeData.overrideValues({
        expandButtonEnabled: false,
        hideMenuOnAiPageEnabled: true,
        isAiPage: true,
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
    assertEquals((item2 as CrIconElement).icon, 'contextual_tasks:pdf');

    // Check item 3 (image).
    const item3 = items[2];
    assertHTMLElement(item3);  // Assert first
    assertTrue(item3.classList.contains('favicon-item'));
    assertEquals(item3.tagName, 'CR-ICON');
    assertEquals((item3 as CrIconElement).icon, 'contextual_tasks:img_icon');

    const moreItems =
        sourcesButton.shadowRoot.querySelector<HTMLElement>('#more-items');
    assertTrue(!!moreItems);
    assertEquals(moreItems.innerText, '+1');
  });

  suite('Reopen Tabs', () => {
    setup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      topToolbar = document.createElement('top-toolbar');
      document.body.appendChild(topToolbar);
    });

    test('shows reopen tabs section', async () => {
      // Initially not in the DOM.
      let reopenTabs =
          topToolbar.shadowRoot.querySelector<HTMLElement>('reopen-tabs');
      assertFalse(!!reopenTabs);

      // Show via mojo.
      proxy.callbackRouterRemote.setShowReopenTabs(true);
      await microtasksFinished();
      reopenTabs =
          topToolbar.shadowRoot.querySelector<HTMLElement>('reopen-tabs');
      assertHTMLElement(reopenTabs);
    });

    test('handles reopen tabs click', async () => {
      proxy.callbackRouterRemote.setShowReopenTabs(true);
      await microtasksFinished();

      const reopenTabs =
          topToolbar.shadowRoot.querySelector<HTMLElement>('reopen-tabs');
      assertHTMLElement(reopenTabs);

      const reopenButton =
          reopenTabs.shadowRoot!.querySelector<HTMLElement>('cr-button');
      assertHTMLElement(reopenButton);
      reopenButton.click();

      await proxy.handler.whenCalled('reopenTabs');
    });

    test('handles reopen tabs dismiss click', async () => {
      proxy.callbackRouterRemote.setShowReopenTabs(true);
      await microtasksFinished();

      let reopenTabs =
          topToolbar.shadowRoot.querySelector<HTMLElement>('reopen-tabs');
      assertHTMLElement(reopenTabs);

      const dismissButton =
          reopenTabs.shadowRoot!.querySelector<HTMLElement>('#reopenDismiss');
      assertHTMLElement(dismissButton);
      dismissButton.click();

      await microtasksFinished();
      reopenTabs =
          topToolbar.shadowRoot.querySelector<HTMLElement>('reopen-tabs');
      assertFalse(!!reopenTabs);
    });
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
});
