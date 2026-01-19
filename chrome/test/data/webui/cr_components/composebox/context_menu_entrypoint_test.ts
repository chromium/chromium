// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/context_menu_entrypoint.js';

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ContextMenuEntrypointElement} from 'chrome://resources/cr_components/composebox/context_menu_entrypoint.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ContextMenuEntrypoint', () => {
  let entrypoint: ContextMenuEntrypointElement;

  let searchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;

  async function openContextMenuWithSuggestions(suggestions: TabInfo[]) {
    entrypoint.tabSuggestions = suggestions;
    $$(entrypoint, '#entrypoint')!.click();
    await microtasksFinished();
  }

  function createTabInfo(count: number): TabInfo[] {
    const tabs: TabInfo[] = [];
    for (let i = 1; i <= count; i++) {
      tabs.push({
        title: `Tab ${i}`,
        url: {url: `https://www.google.com/${i}`},
        tabId: i,
        showInCurrentTabChip: false,
        showInPreviousTabChip: true,
        lastActive: {internalValue: BigInt(i)},
      });
    }
    return tabs;
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      composeboxShowContextMenuTabPreviews: true,
      composeboxFileMaxCount: 10,
      composeboxShowPdfUpload: true,
    });

    searchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    const proxy = new ComposeboxProxyImpl(
        new PageHandlerRemote(), new PageCallbackRouter(),
        searchboxPageHandler as unknown as SearchboxPageHandlerRemote,
        new SearchboxPageCallbackRouter());
    ComposeboxProxyImpl.setInstance(proxy);

    entrypoint = document.createElement('cr-composebox-context-menu-entrypoint');
    document.body.appendChild(entrypoint);
    await microtasksFinished();
  });

  test('menu is hidden initially', async () => {
    await microtasksFinished();
    assertFalse(entrypoint.$.menu.open);
  });

  test('clicking entrypoint shows context menu', async () => {
    // Act.
    $$(entrypoint, '#entrypoint')!.click();
    await microtasksFinished();

    // Assert.
    assertTrue(entrypoint.$.menu.open);
  });

  test(
      'tab header is not displayed when there are no tab suggestions',
      async () => {
        // Arrange & Act.
        entrypoint.tabSuggestions = [];
        $$(entrypoint, '#entrypoint')!.click();
        await microtasksFinished();
        assertTrue(entrypoint.$.menu.open);

        // Assert.
        const tabHeader = $$(entrypoint, '#tabHeader');
        assertFalse(!!tabHeader);
        const items = entrypoint.$.menu.querySelectorAll('.dropdown-item');
        assertEquals(2, items.length);
        assertEquals('imageUpload', items[0]!.id);
        assertEquals('fileUpload', items[1]!.id);
      });

  test(
      'clicking entrypoint shows context menu with correct items', async () => {
        // Arrange.
        entrypoint.tabSuggestions = [
          {
            title: 'Tab 1',
            url: {url: 'https://www.google.com'},
            tabId: 1,
            showInCurrentTabChip: false,
            showInPreviousTabChip: true,
            lastActive: {internalValue: BigInt(1)},
          },
          {
            title: 'Tab 2',
            url: {url: 'https://www.google.com'},
            tabId: 2,
            showInCurrentTabChip: false,
            showInPreviousTabChip: true,
            lastActive: {internalValue: BigInt(2)},
          },
        ];
        $$(entrypoint, '#entrypoint')!.click();
        await microtasksFinished();
        assertTrue(entrypoint.$.menu.open);

        // Act & Assert.
        const tabHeader = $$(entrypoint, '#tabHeader');
        assertTrue(!!tabHeader);
        const items = entrypoint.$.menu.querySelectorAll('.dropdown-item');
        assertEquals(4, items.length);
        assertEquals('Tab 1', items[0]!.getAttribute('title'));
        assertEquals('Tab 2', items[1]!.getAttribute('title'));
        assertEquals(
            'Most recent tabs, Tab 1', items[0]!.getAttribute('aria-label'));
        assertEquals(
            'Most recent tabs, Tab 2', items[1]!.getAttribute('aria-label'));
        assertEquals('imageUpload', items[2]!.id);
        assertEquals('fileUpload', items[3]!.id);
      });

  test('disabled tabs cannot be added as context', async () => {
    // Arrange.
    $$(entrypoint, '#entrypoint')!.click();
    entrypoint.tabSuggestions = [
      {
        title: 'Tab 1',
        url: {url: 'https://www.google.com'},
        tabId: 1,
        showInCurrentTabChip: false,
        showInPreviousTabChip: true,
        lastActive: {internalValue: BigInt(1)},
      },
      {
        title: 'Tab 2',
        url: {url: 'https://www.google.com'},
        tabId: 2,
        showInCurrentTabChip: false,
        showInPreviousTabChip: true,
        lastActive: {internalValue: BigInt(2)},
      },
    ];
    entrypoint.disabledTabIds = new Map([[2, '2']]);
    await microtasksFinished();
    assertTrue(entrypoint.$.menu.open);

    // Assert.
    const items = entrypoint.$.menu.querySelectorAll('.dropdown-item');
    const tab1 = items[0]! as HTMLButtonElement;
    assertEquals('Tab 1', tab1.getAttribute('title'));
    assertFalse(tab1.disabled);
    const tab2 = items[1]! as HTMLButtonElement;
    assertEquals('Tab 2', tab2.getAttribute('title'));
    assertTrue(tab2.disabled);
  });

  ([
    ['#fileUpload', 'open-file-upload'],
    ['#imageUpload', 'open-image-upload'],
  ] as Array<[string, string]>)
      .forEach(([selector, eventName]) => {
        test(
            `clicking ${selector} propagates ${eventName} before closing menu`,
            async () => {
              // Arrange.
              $$(entrypoint, '#entrypoint')!.click();
              await microtasksFinished();
              assertTrue(entrypoint.$.menu.open);

              // Act.
              const eventFired = eventToPromise(eventName, entrypoint);
              const button = $$(entrypoint, selector);
              assertTrue(!!button);
              button.click();
              await eventFired;

              // Assert.
              assertTrue(!!eventFired);

              assertFalse(entrypoint.$.menu.open);
            });
      });

  test('tab thumbnail is shown on pointerenter', async () => {
    // Arrange.
    const previewUrl = 'data:image/png;base64,sometestdata';
    const tabPreviewPromise = eventToPromise('get-tab-preview', entrypoint);
    await openContextMenuWithSuggestions(createTabInfo(1));

    // Assert that thumbnail is not shown initially.
    let preview = $$<HTMLImageElement>(entrypoint, '.tab-preview');
    assertFalse(!!preview);

    // Act.
    const tabItem = $$<HTMLButtonElement>(
        entrypoint, '.suggestion-container .dropdown-item');
    assertTrue(!!tabItem);
    tabItem.dispatchEvent(new PointerEvent('pointerenter', {bubbles: true}));
    const e = await tabPreviewPromise;
    e.detail.onPreviewFetched(previewUrl);
    await microtasksFinished();

    // Assert that thumbnail is shown.
    preview = $$<HTMLImageElement>(entrypoint, '.tab-preview');
    assertTrue(!!preview);
    assertEquals(previewUrl, preview.src);
  });

  test('tab thumbnail is updated on pointerenter on another tab', async () => {
    // Arrange.
    const previewUrl1 = 'data:image/png;base64,sometestdata1';
    const previewUrl2 = 'data:image/png;base64,sometestdata2';
    await openContextMenuWithSuggestions(createTabInfo(2));
    assertTrue(entrypoint.$.menu.open);

    const tabItems = entrypoint.shadowRoot.querySelectorAll<HTMLButtonElement>(
        '.suggestion-container .dropdown-item');
    assertEquals(2, tabItems.length);

    // Act & Assert for first tab.
    const tabPreviewPromise1 = eventToPromise('get-tab-preview', entrypoint);
    tabItems[0]!.dispatchEvent(
        new PointerEvent('pointerenter', {bubbles: true}));
    const e1 = await tabPreviewPromise1;
    e1.detail.onPreviewFetched(previewUrl1);
    await microtasksFinished();

    let previews = entrypoint.shadowRoot.querySelectorAll<HTMLImageElement>(
        '.tab-preview');
    assertEquals(2, previews.length);
    assertEquals(previewUrl1, previews[0]!.src);
    assertEquals(previewUrl1, previews[1]!.src);

    // Act & Assert for second tab.
    const tabPreviewPromise2 = eventToPromise('get-tab-preview', entrypoint);
    tabItems[1]!.dispatchEvent(
        new PointerEvent('pointerenter', {bubbles: true}));
    const e2 = await tabPreviewPromise2;
    e2.detail.onPreviewFetched(previewUrl2);
    await microtasksFinished();

    previews = entrypoint.shadowRoot.querySelectorAll<HTMLImageElement>(
        '.tab-preview');
    assertEquals(2, previews.length);
    assertEquals(previewUrl2, previews[0]!.src);
    assertEquals(previewUrl2, previews[1]!.src);
  });

  test('tab thumbnail is not shown when feature is disabled', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      composeboxShowContextMenuTabPreviews: false,
    });
    // The element reads the loadTimeData in its constructor, so we need to
    // recreate it.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entrypoint = document.createElement('cr-composebox-context-menu-entrypoint');
    document.body.appendChild(entrypoint);
    await microtasksFinished();

    await openContextMenuWithSuggestions(createTabInfo(1));

    // Act.
    const tabItem = $$<HTMLButtonElement>(
        entrypoint, '.suggestion-container .dropdown-item');
    assertTrue(!!tabItem);
    tabItem.dispatchEvent(new PointerEvent('pointerenter', {bubbles: true}));
    await microtasksFinished();

    // Assert that thumbnail is not shown.
    const preview = $$<HTMLImageElement>(entrypoint, '.tab-preview');
    assertFalse(!!preview);
  });

  test('create image mode disables file upload and other tools', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      composeboxShowDeepSearchButton: true,
      composeboxShowCreateImageButton: true,
    });

    entrypoint.remove();
    entrypoint = document.createElement('cr-composebox-context-menu-entrypoint');
    document.body.appendChild(entrypoint);
    await microtasksFinished();

    await openContextMenuWithSuggestions([]);

    const fileUploadButton = $$<HTMLButtonElement>(entrypoint, '#fileUpload');
    const deepSearchButton = $$<HTMLButtonElement>(entrypoint, '#deepSearch');
    const createImageButton = $$<HTMLButtonElement>(entrypoint, '#createImage');
    assertTrue(!!fileUploadButton);
    assertTrue(!!deepSearchButton);
    assertTrue(!!createImageButton);

    // Assert buttons are enabled initially.
    assertFalse(fileUploadButton.disabled);
    assertFalse(deepSearchButton.disabled);
    assertFalse(createImageButton.disabled);

    // Set `inCreateImageMode` to true.
    entrypoint.inCreateImageMode = true;
    await entrypoint.updateComplete;

    // Assert buttons are disabled.
    assertTrue(fileUploadButton.disabled);
    assertTrue(deepSearchButton.disabled);
    assertTrue(createImageButton.disabled);

    // Set `inCreateImageMode` to false.
    entrypoint.inCreateImageMode = false;
    await entrypoint.updateComplete;

    // Assert buttons are enabled again.
    assertFalse(fileUploadButton.disabled);
    assertFalse(deepSearchButton.disabled);
    assertFalse(createImageButton.disabled);
  });

  test('deep search mode disables contextual inputs', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      composeboxShowDeepSearchButton: true,
    });
    entrypoint.remove();
    entrypoint = document.createElement('cr-composebox-context-menu-entrypoint');
    document.body.appendChild(entrypoint);
    // Simulate parent component behavior of listening for event and changing
    // property.
    entrypoint.addEventListener('deep-search-click', () => {
      entrypoint.inputsDisabled = !entrypoint.inputsDisabled;
    });
    await entrypoint.updateComplete;

    await openContextMenuWithSuggestions([]);

    // Assert entrypoint is enabled initially.
    const deepSearchButton = $$<HTMLButtonElement>(entrypoint, '#deepSearch');
    assertTrue(!!deepSearchButton);
    assertFalse(entrypoint.inputsDisabled);

    // Click deep search button.
    const eventFired = eventToPromise('deep-search-click', entrypoint);
    deepSearchButton.click();
    await eventFired;
    await entrypoint.updateComplete;

    // Assert menu is closed and entrypoint is disabled.
    assertFalse(entrypoint.$.menu.open);
    assertTrue(entrypoint.inputsDisabled);

    // Toggle deep search button.
    entrypoint['onDeepSearchClick_']();
    await entrypoint.updateComplete;

    // Assert entrypoint is enabled again.
    assertFalse(entrypoint.inputsDisabled);
  });

  test('image upload is disabled based on state', async () => {
    await openContextMenuWithSuggestions([]);
    const imageUploadButton = $$<HTMLButtonElement>(entrypoint, '#imageUpload');
    assertTrue(!!imageUploadButton);

    // Initially enabled.
    assertFalse(imageUploadButton.disabled);

    // Disabled when max files are hit.
    entrypoint.fileNum = 10;
    await microtasksFinished();
    assertTrue(imageUploadButton.disabled);

    // Re-enabled.
    entrypoint.fileNum = 0;
    await microtasksFinished();
    assertFalse(imageUploadButton.disabled);

    // Disabled in create image mode with image files.
    entrypoint.inCreateImageMode = true;
    entrypoint.hasImageFiles = true;
    await microtasksFinished();
    assertTrue(imageUploadButton.disabled);

    // Enabled in create image mode without image files.
    entrypoint.hasImageFiles = false;
    await microtasksFinished();
    assertFalse(imageUploadButton.disabled);
  });

  test('file upload is disabled based on state', async () => {
    await openContextMenuWithSuggestions([]);
    const fileUploadButton = $$<HTMLButtonElement>(entrypoint, '#fileUpload');
    assertTrue(!!fileUploadButton);

    // Initially enabled.
    assertFalse(fileUploadButton.disabled);

    // Disabled when max files are hit.
    entrypoint.fileNum = 10;
    await microtasksFinished();
    assertTrue(fileUploadButton.disabled);

    // Re-enabled.
    entrypoint.fileNum = 0;
    await microtasksFinished();
    assertFalse(fileUploadButton.disabled);

    // Disabled in create image mode.
    entrypoint.inCreateImageMode = true;
    await microtasksFinished();
    assertTrue(fileUploadButton.disabled);
  });

  test('deep search is disabled based on state', async () => {
    loadTimeData.overrideValues({
      composeboxShowDeepSearchButton: true,
    });
    entrypoint.remove();
    entrypoint = document.createElement('cr-composebox-context-menu-entrypoint');
    document.body.appendChild(entrypoint);
    await microtasksFinished();

    await openContextMenuWithSuggestions([]);
    const deepSearchButton = $$<HTMLButtonElement>(entrypoint, '#deepSearch');
    assertTrue(!!deepSearchButton);

    // Initially enabled.
    assertFalse(deepSearchButton.disabled);

    // Disabled in create image mode.
    entrypoint.inCreateImageMode = true;
    await microtasksFinished();
    assertTrue(deepSearchButton.disabled);
    entrypoint.inCreateImageMode = false;
    await microtasksFinished();
    assertFalse(deepSearchButton.disabled);

    // Disabled with 1 file.
    entrypoint.fileNum = 1;
    await microtasksFinished();
    assertTrue(deepSearchButton.disabled);
    entrypoint.fileNum = 0;
    await microtasksFinished();
    assertFalse(deepSearchButton.disabled);

    // Disabled with more than 1 file.
    entrypoint.fileNum = 2;
    await microtasksFinished();
    assertTrue(deepSearchButton.disabled);
  });

  test('create image is disabled based on state', async () => {
    loadTimeData.overrideValues({
      composeboxShowCreateImageButton: true,
    });
    entrypoint.remove();
    entrypoint = document.createElement('cr-composebox-context-menu-entrypoint');
    document.body.appendChild(entrypoint);
    await microtasksFinished();

    await openContextMenuWithSuggestions([]);
    const createImageButton = $$<HTMLButtonElement>(entrypoint, '#createImage');
    assertTrue(!!createImageButton);

    // Initially enabled.
    assertFalse(createImageButton.disabled);

    // Disabled with more than 1 file.
    entrypoint.fileNum = 2;
    await microtasksFinished();
    assertTrue(createImageButton.disabled);
    entrypoint.fileNum = 0;
    await microtasksFinished();
    assertFalse(createImageButton.disabled);

    // Disabled with 1 file and no image files.
    entrypoint.fileNum = 1;
    entrypoint.hasImageFiles = false;
    await microtasksFinished();
    assertTrue(createImageButton.disabled);

    // Enabled with 1 file and image files.
    entrypoint.hasImageFiles = true;
    await microtasksFinished();
    assertFalse(createImageButton.disabled);
    entrypoint.fileNum = 0;
    entrypoint.hasImageFiles = false;
    await microtasksFinished();

    // Disabled in create image mode.
    entrypoint.inCreateImageMode = true;
    await microtasksFinished();
    assertTrue(createImageButton.disabled);
  });

  test('tabs are disabled based on state', async () => {
    await openContextMenuWithSuggestions(createTabInfo(2));
    const tabItems = entrypoint.shadowRoot.querySelectorAll<HTMLButtonElement>(
        '.suggestion-container .dropdown-item');
    assertEquals(2, tabItems.length);
    const tab1 = tabItems[0]!;
    const tab2 = tabItems[1]!;

    // Initially enabled.
    assertFalse(tab1.disabled);
    assertFalse(tab2.disabled);

    // Disabled when max files are hit.
    entrypoint.fileNum = 10;
    await microtasksFinished();
    assertTrue(tab1.disabled);
    assertTrue(tab2.disabled);
    entrypoint.fileNum = 0;
    await microtasksFinished();
    assertFalse(tab1.disabled);
    assertFalse(tab2.disabled);

    // Disabled in create image mode.
    entrypoint.inCreateImageMode = true;
    await microtasksFinished();
    assertTrue(tab1.disabled);
    assertTrue(tab2.disabled);
    entrypoint.inCreateImageMode = false;
    await microtasksFinished();
    assertFalse(tab1.disabled);
    assertFalse(tab2.disabled);

    // Disabled via disabledTabIds.
    entrypoint.disabledTabIds = new Map([[1, '1']]);
    await microtasksFinished();
    assertTrue(tab1.disabled);
    assertFalse(tab2.disabled);
  });

  test(
      'multi-tab enabled allows selection/deselection of tabs', async () => {
        loadTimeData.overrideValues({
          composeboxContextMenuEnableMultiTabSelection: true,
        });
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        entrypoint = document.createElement(
            'cr-composebox-context-menu-entrypoint');
        document.body.appendChild(entrypoint);
        await microtasksFinished();

        await openContextMenuWithSuggestions(createTabInfo(2));
        let tabSelectors =
            entrypoint.shadowRoot.querySelectorAll<HTMLElement>(
                '.multi-tab-icon');
        assertEquals(2, tabSelectors.length);
        assertEquals('cr:add', tabSelectors[0]!.getAttribute('icon'));
        assertEquals('cr:add', tabSelectors[1]!.getAttribute('icon'));

        // Act by adding tab 1 as context.
        entrypoint.disabledTabIds = new Map([[1, '1']]);
        await microtasksFinished();

        // Assert tab 1 is selected and tab 2 is not.
        tabSelectors =
            entrypoint.shadowRoot.querySelectorAll<HTMLElement>(
                '.multi-tab-icon');
        assertEquals(2, tabSelectors.length);
        assertEquals('cr:check', tabSelectors[0]!.getAttribute('icon'));
        assertEquals('cr:add', tabSelectors[1]!.getAttribute('icon'));
      });

  test('multi-tab enabled does not close context menu', async () => {
    loadTimeData.overrideValues({
      composeboxContextMenuEnableMultiTabSelection: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entrypoint = document.createElement(
        'cr-composebox-context-menu-entrypoint');
    document.body.appendChild(entrypoint);
    await microtasksFinished();

    await openContextMenuWithSuggestions(createTabInfo(1));
    const tabItems = entrypoint.shadowRoot.querySelectorAll<HTMLButtonElement>(
        '.suggestion-container .dropdown-item');
    assertEquals(1, tabItems.length);
    const tab = tabItems[0]!;

    // Act by clicking on tab to initiate upload flow.
    const whenTabContextAdded = eventToPromise('add-tab-context', entrypoint);
    tab.click();
    const event = await whenTabContextAdded;
    assertEquals(TabUploadOrigin.CONTEXT_MENU, event.detail.origin);

    // Assert context menu is still open.
    assertTrue(entrypoint.$.menu.open);
  });

  test('multi-tab enabled deletes context on second click', async () => {
    loadTimeData.overrideValues({
      composeboxContextMenuEnableMultiTabSelection: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entrypoint = document.createElement(
        'cr-composebox-context-menu-entrypoint');
    document.body.appendChild(entrypoint);
    await microtasksFinished();

    await openContextMenuWithSuggestions(createTabInfo(1));
    const tabItems = entrypoint.shadowRoot.querySelectorAll<HTMLButtonElement>(
        '.suggestion-container .dropdown-item');
    assertEquals(1, tabItems.length);
    const tab = tabItems[0]!;

    // Add tab to disabled tabs to simulate it being added as context.
    entrypoint.disabledTabIds = new Map([[1, '1']]);
    await microtasksFinished();

    // Act by clicking on tab to initiate delete flow.
    const whenTabContextAdded =
        eventToPromise('delete-tab-context', entrypoint);
    tab.click();
    await whenTabContextAdded;

    // Assert context menu is still open.
    assertTrue(entrypoint.$.menu.open);
  });
});
