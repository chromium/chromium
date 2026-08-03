// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import type {OmniboxPopupContextualEntrypointElement, OmniboxPopupPageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {omniboxPopupBrowserProxyFactory, OmniboxPopupPageHandlerRemote, SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {InputType} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDefaultInputState, TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('OmniboxPopupContextualEntrypointTest', () => {
  let element: OmniboxPopupContextualEntrypointElement;
  let testProxy: TestSearchboxBrowserProxy;
  let popupHandler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;
  let popupCallbackRouter: OmniboxPopupPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      omniboxAimPopupEnabled: true,
      searchboxLayoutMode: 'TallBottomContext',
      hideClassicContextButton: false,
      composeboxShowContextMenuDescription: false,
      omniboxShowContextButtonSuggestionLabel: false,
      composeboxShowCurrentTabChip: true,
      composeboxShowLensSearchChip: true,
      composeboxShowLensIcon: true,
      contextualMenuUsePecApi: false,
      contextButtonHasBackground: false,
      contextButtonShapeIsOblong: false,
    });

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    popupHandler = TestMock.fromClass(OmniboxPopupPageHandlerRemote);
    const {instance, remote} =
        omniboxPopupBrowserProxyFactory.createForTest(popupHandler);
    popupCallbackRouter = remote;
    omniboxPopupBrowserProxyFactory.setInstance(instance);

    element = document.createElement('omnibox-popup-contextual-entrypoint');
    element.inputState = createDefaultInputState();
    element.isContentSharingEnabled = true;
    document.body.appendChild(element);
    testProxy.initVisibilityPrefs();
    await microtasksFinished();
  });

  test('LensSearchClickTriggersMojo', async () => {
    element.isLensSearchEligible = true;
    await microtasksFinished();

    const lensChip = $$<HTMLElement>(element, '#lensSearchChip');
    assertTrue(!!lensChip);

    lensChip.dispatchEvent(new CustomEvent('lens-search-click', {
      bubbles: true,
      composed: true,
    }));

    await testProxy.handler.whenCalled('openLensSearch');
    assertEquals(1, testProxy.handler.getCallCount('openLensSearch'));
  });

  test('AddTabContextTriggersMojo', async () => {
    element.isLensSearchEligible = true;
    testProxy.handler.setPromiseResolveFor<'getRecentTabs'>('getRecentTabs', {
      tabs: [{
        tabId: 123,
        title: 'Test Tab',
        url: 'https://example.com',
        showInCurrentTabChip: true,
      }],
    });

    popupCallbackRouter.onShow();
    await testProxy.handler.whenCalled('getRecentTabs');
    await microtasksFinished();

    const currentTabChip = $$<HTMLElement>(element, '#currentTabChip');
    assertTrue(!!currentTabChip);

    currentTabChip.dispatchEvent(new CustomEvent('add-tab-context', {
      detail: {id: 123, title: 'Test Tab', url: {url: 'https://example.com'}},
      bubbles: true,
      composed: true,
    }));

    assertEquals(1, testProxy.handler.getCallCount('addTabContext'));
    const [tabId, delayUpload] =
        await testProxy.handler.whenCalled('addTabContext');
    assertEquals(123, tabId);
    assertFalse(delayUpload);
  });

  test('OnShowRefreshesCurrentTab', async () => {
    testProxy.handler.setPromiseResolveFor<'getRecentTabs'>('getRecentTabs', {
      tabs: [{
        tabId: 456,
        title: 'Tab 456',
        url: 'https://google.com',
        showInCurrentTabChip: true,
      }],
    });

    popupCallbackRouter.onShow();
    await testProxy.handler.whenCalled('getRecentTabs');

    assertEquals(1, testProxy.handler.getCallCount('getRecentTabs'));
  });

  test('CompactModeHidesEntrypointButton', async () => {
    element.searchboxLayoutMode = 'Compact';
    await microtasksFinished();

    const entrypointButton = element.getContextEntrypointElement();
    assertFalse(!!entrypointButton);
  });

  test('CurrentTabChipHasPriorityOverLensChip', async () => {
    element.isLensSearchEligible = true;
    testProxy.handler.setPromiseResolveFor<'getRecentTabs'>('getRecentTabs', {
      tabs: [{
        tabId: 123,
        title: 'Test Tab',
        url: 'https://example.com',
        showInCurrentTabChip: true,
      }],
    });

    popupCallbackRouter.onShow();
    await testProxy.handler.whenCalled('getRecentTabs');
    await microtasksFinished();

    const currentTabChip = $$<HTMLElement>(element, '#currentTabChip');
    assertTrue(!!currentTabChip);

    const lensChip = $$<HTMLElement>(element, '#lensSearchChip');
    assertFalse(!!lensChip);
  });

  test('BackgroundAndShapeProperties', async () => {
    loadTimeData.overrideValues({
      contextButtonHasBackground: true,
      contextButtonShapeIsOblong: true,
    });

    const newElement =
        document.createElement('omnibox-popup-contextual-entrypoint');
    document.body.appendChild(newElement);
    await microtasksFinished();

    const button =
        $$<HTMLElement&
           {applyContextButtonBackground?: boolean, isOblongShape?: boolean}>(
            newElement, '#context');
    assertTrue(!!button);
    assertTrue(button.applyContextButtonBackground ?? false);
    assertTrue(button.isOblongShape ?? false);
    newElement.remove();
  });

  test('PecApiInputTypeFiltering', async () => {
    loadTimeData.overrideValues({
      contextualMenuUsePecApi: true,
    });

    const newElement =
        document.createElement('omnibox-popup-contextual-entrypoint');
    document.body.appendChild(newElement);
    testProxy.initVisibilityPrefs();
    newElement.isContentSharingEnabled = true;
    newElement.isLensSearchEligible = true;

    testProxy.handler.setPromiseResolveFor<'getRecentTabs'>('getRecentTabs', {
      tabs: [{
        tabId: 123,
        title: 'Test Tab',
        url: 'https://example.com',
        showInCurrentTabChip: true,
      }],
    });

    popupCallbackRouter.onShow();
    await testProxy.handler.whenCalled('getRecentTabs');

    // Without kBrowserTab allowed input type, tab chip should be hidden.
    newElement.inputState = {
      ...createDefaultInputState(),
      allowedInputTypes: [],
    };
    await microtasksFinished();

    let currentTabChip = $$<HTMLElement>(newElement, '#currentTabChip');
    assertFalse(!!currentTabChip);

    // With kBrowserTab allowed input type, tab chip should be shown.
    newElement.inputState = {
      ...createDefaultInputState(),
      allowedInputTypes: [InputType.kBrowserTab],
    };
    await microtasksFinished();

    currentTabChip = $$<HTMLElement>(newElement, '#currentTabChip');
    assertTrue(!!currentTabChip);
    newElement.remove();
  });
});
