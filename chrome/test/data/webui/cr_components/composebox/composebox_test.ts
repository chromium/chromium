// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox.js';
import 'chrome://contextual-tasks/strings.m.js';

import {ComposeboxFile, TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
// <if expr="not is_android">
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

// </if>

import {installMock} from './composebox_test_utils.js';

interface TestComposeboxElement extends ComposeboxElement {
  composeboxSource: string;
  keepMenuOpenForMultiSelection(): Promise<void>;
}

// LINT.IfChange
suite('ComposeboxTest', () => {
  let composebox: ComposeboxElement;
  let searchboxHandler: SearchboxPageHandlerRemote&TestMock<SearchboxPageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      composeboxSource: 'NTP',
      composeboxShowZps: true,
      composeboxFileMaxCount: 1,
      composeboxFileMaxSize: 1024,
      composeboxAttachmentFileTypes: '.pdf',
      composeboxImageFileTypes: 'image/png',
      composeboxSmartTabSharingVisible: false,
    });

    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));

    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    // <if expr="not is_android">
    searchboxHandler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));
    // </if>
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));

    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await composebox.updateComplete;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  // <if expr="not is_android">
  test(
      'connectedCallback calls getSmartTabSharingActive when' +
          ' smartTabSharingVisible pre-set to true',
      async () => {
        searchboxHandler.setResultMapperFor(
            'getSmartTabSharingActive', () => Promise.resolve({active: true}));

        const newComposebox = document.createElement('cr-composebox');
        newComposebox.smartTabSharingVisible = true;
        document.body.appendChild(newComposebox);
        await searchboxHandler.whenCalled('getSmartTabSharingActive');
        await microtasksFinished();
        await newComposebox.updateComplete;

        assertTrue(
            searchboxHandler.getCallCount('getSmartTabSharingActive') === 1);
        assertTrue(newComposebox.smartTabSharingActive);
      });

  test(
      'connectedCallback does NOT call getSmartTabSharingActive when' +
          ' smartTabSharingVisible is false',
      async () => {
        const newComposebox = document.createElement('cr-composebox');
        newComposebox.smartTabSharingVisible = false;
        document.body.appendChild(newComposebox);
        await newComposebox.updateComplete;

        assertTrue(
            searchboxHandler.getCallCount('getSmartTabSharingActive') === 0);
        assertFalse(newComposebox.smartTabSharingActive);
      });

  test(
      'host template .prop binding triggers getSmartTabSharingActive' +
          ' at child mount',
      async () => {
        searchboxHandler.setResultMapperFor(
            'getSmartTabSharingActive', () => Promise.resolve({active: true}));

        document.body.innerHTML = getTrustedHtml(`
      <cr-composebox smart-tab-sharing-visible></cr-composebox>
    `);

        const newComposebox =
            document.body.querySelector<ComposeboxElement>('cr-composebox');
        assertTrue(!!newComposebox);

        await searchboxHandler.whenCalled('getSmartTabSharingActive');
        await microtasksFinished();
        await newComposebox.updateComplete;

        assertTrue(
            searchboxHandler.getCallCount('getSmartTabSharingActive') === 1);
        assertTrue(newComposebox.smartTabSharingActive);
      });
  // </if>

  test('UpdateAutoSuggestedTabContext_NullDoesNotDeleteIfConditionsMet', () => {
    loadTimeData.overrideValues({webUIOmniboxAskGAboutThisPageEnabled: true});
    composebox.isSidePanel = true;
    const token = {high: 0n, low: 1n} as any;
    const mockFile =
        new ComposeboxFile(token, 'Auto Tab', 'tab', InputType.kBrowserTab, {
          isDeletable: true,
          tabId: 1,
          url: 'http://example.com',
        });
    composebox.setAutomaticActiveTabForTesting(mockFile);

    let deleteFileCalled = false;
    const originalDeleteFile = composebox.deleteFile;
    composebox.deleteFile = (_uuid) => {
      deleteFileCalled = true;
      return null;
    };

    // Call with null, should NOT delete.
    composebox.updateAutoSuggestedTabContextForTesting(
        null, 'OmniboxPageAction');
    assertFalse(deleteFileCalled);

    // Call with different URL, SHOULD delete.
    const differentTab: TabInfo = {
      tabId: 2,
      title: 'Different Tab',
      url: 'http://different.com',
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(1)} as any,
    };
    composebox.updateAutoSuggestedTabContextForTesting(
        differentTab, 'OmniboxPageAction');
    assertTrue(deleteFileCalled);

    // Restore
    composebox.isSidePanel = false;
    composebox.deleteFile = originalDeleteFile;
  });

  test('UpdateAutoSuggestedTabContext_NullDeletesIfFeatureDisabled', () => {
    loadTimeData.overrideValues({webUIOmniboxAskGAboutThisPageEnabled: false});
    const token = {high: 0n, low: 1n} as any;
    const mockFile =
        new ComposeboxFile(token, 'Auto Tab', 'tab', InputType.kBrowserTab, {
          isDeletable: true,
          tabId: 1,
          url: 'http://example.com',
        });
    composebox.setAutomaticActiveTabForTesting(mockFile);

    let deleteFileCalled = false;
    const originalDeleteFile = composebox.deleteFile;
    composebox.deleteFile = (_uuid) => {
      deleteFileCalled = true;
      return null;
    };

    // Call with null, should delete because feature is disabled.
    composebox.updateAutoSuggestedTabContextForTesting(null);
    assertTrue(deleteFileCalled);

    // Restore
    composebox.deleteFile = originalDeleteFile;
  });

  test(
      'UpdateAutoSuggestedTabContext_NullDeletesIfFeatureDisabledEvenIfSidePanel',
      () => {
        loadTimeData.overrideValues(
            {webUIOmniboxAskGAboutThisPageEnabled: false});
        composebox.isSidePanel = true;
        const token = {high: 0n, low: 1n} as any;
        const mockFile = new ComposeboxFile(
            token, 'Auto Tab', 'tab', InputType.kBrowserTab, {
              isDeletable: true,
              tabId: 1,
              url: 'http://example.com',
            });
        composebox.setAutomaticActiveTabForTesting(mockFile);

        let deleteFileCalled = false;
        const originalDeleteFile = composebox.deleteFile;
        composebox.deleteFile = (_uuid) => {
          deleteFileCalled = true;
          return null;
        };

        // Call with null, should delete because feature is disabled.
        composebox.updateAutoSuggestedTabContextForTesting(
            null, 'OmniboxPageAction');
        assertTrue(deleteFileCalled);

        // Restore
        composebox.isSidePanel = false;
        composebox.deleteFile = originalDeleteFile;
      });

  test(
      'keepMenuOpenForMultiSelection is gated' +
          ' by keepMenuOpenOnTabSelectForRealbox',
      async () => {
        let openMenuCalled = false;
        composebox.getContextEntrypointElement = () => {
          return {
            openMenuForMultiSelection: () => {
              openMenuCalled = true;
            },
          } as unknown as ContextualEntrypointAndMenuElement;
        };

        const testElement = composebox as TestComposeboxElement;
        testElement.contextManagementInComposeboxEnabled = true;

        // Omnibox source: always returns early
        testElement.composeboxSource = 'Omnibox';
        await testElement.keepMenuOpenForMultiSelection();
        assertFalse(openMenuCalled);

        // NewTabPage source, flag off: returns early
        testElement.composeboxSource = 'NewTabPage';
        loadTimeData.overrideValues({keepMenuOpenOnTabSelectForRealbox: false});
        await testElement.keepMenuOpenForMultiSelection();
        assertFalse(openMenuCalled);

        // NewTabPage source, flag on: calls openMenuForMultiSelection
        testElement.composeboxSource = 'NewTabPage';
        loadTimeData.overrideValues({keepMenuOpenOnTabSelectForRealbox: true});
        await testElement.keepMenuOpenForMultiSelection();
        assertTrue(openMenuCalled);
      });

  test(
      'keepMenuOpenForMultiSelection called on add/delete tab context',
      async () => {
        let keepMenuOpenCalled = false;
        const testElement = composebox as TestComposeboxElement;
        testElement.keepMenuOpenForMultiSelection = () => {
          keepMenuOpenCalled = true;
          return Promise.resolve();
        };

        await composebox.onAddTabContext(new CustomEvent('add-tab-context', {
          detail: {
            id: 1,
            title: 'Test',
            url: 'about:blank',  // Mojo converts obj to str.
            delayUpload: false,
            origin: TabUploadOrigin.CONTEXT_MENU,
          },
        }));
        assertTrue(keepMenuOpenCalled);

        keepMenuOpenCalled = false;
        await composebox.onDeleteTabContext(
            new CustomEvent('delete-tab-context', {
              detail: {
                tabId: 1,
              },
            }));
        assertTrue(keepMenuOpenCalled);
      });

  test('onContextMenuClosed sets shareTabsFlyoutOpen to false', async () => {
    composebox.shareTabsFlyoutOpen = true;
    await composebox.onContextMenuClosed();
    assertFalse(composebox.shareTabsFlyoutOpen);
  });
});


suite('composeboxSharedMountAutoRepositionDefault', () => {
  let composebox: ComposeboxElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      composeboxSource: 'NTP',
      composeboxShowZps: true,
      composeboxFileMaxCount: 1,
      composeboxFileMaxSize: 1024,
      composeboxAttachmentFileTypes: '.pdf',
      composeboxImageFileTypes: 'image/png',
      composeboxShowContextMenu: true,
      composeboxContextMenuEnableMultiTabSelection: false,
      composeboxShowContextMenuTabPreviews: false,
      ShowContextMenuHeaders: false,
      menu: 'menu',
      addContextTitle: 'Add context',
      addContext: 'Add context',
    });

    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    const searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    // <if expr="not is_android">
    searchboxHandler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));
    // </if>
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NTP_REALBOX'}));

    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await composebox.updateComplete;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('onContextMenuClosed sets shareTabsFlyoutOpen to false', async () => {
    composebox.shareTabsFlyoutOpen = true;
    await composebox.onContextMenuClosed();
    assertFalse(composebox.shareTabsFlyoutOpen);
  });
});
// LINT.ThenChange(//ui/webui/resources/cr_components/composebox/Componentization.md)
