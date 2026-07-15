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
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

import {installMock} from './composebox_test_utils.js';

interface TestComposeboxElement extends ComposeboxElement {
  composeboxSource: string;
  keepMenuOpenForMultiSelection(): Promise<void>;
}

// LINT.IfChange
suite('ComposeboxTest', () => {
  let composebox: ComposeboxElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;
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
    });

    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    handler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));

    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
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

  test('connectedCallback calls getSmartTabSharingActive when' +
        ' smartTabSharingVisible pre-set to true', async () => {
    handler.setResultMapperFor(
        'getSmartTabSharingActive',
        () => Promise.resolve({active: true}));

    const newComposebox = document.createElement('cr-composebox');
    newComposebox.smartTabSharingVisible = true;
    document.body.appendChild(newComposebox);
    await handler.whenCalled('getSmartTabSharingActive');
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('getSmartTabSharingActive'));
    assertTrue(newComposebox.smartTabSharingActive);
  });

  test('connectedCallback does NOT call getSmartTabSharingActive when' +
        ' smartTabSharingVisible is false', () => {
    const newComposebox = document.createElement('cr-composebox');
    newComposebox.smartTabSharingVisible = false;
    document.body.appendChild(newComposebox);

    assertEquals(0, handler.getCallCount('getSmartTabSharingActive'));
    assertFalse(newComposebox.smartTabSharingActive);
  });

  test('host template .prop binding triggers getSmartTabSharingActive' +
        ' at child mount', async () => {
    handler.setResultMapperFor(
        'getSmartTabSharingActive',
        () => Promise.resolve({active: true}));

    document.body.innerHTML = getTrustedHtml(`
      <cr-composebox smart-tab-sharing-visible></cr-composebox>
    `);

    const newComposebox =
        document.body.querySelector<ComposeboxElement>('cr-composebox');
    assertTrue(!!newComposebox);

    await handler.whenCalled('getSmartTabSharingActive');
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('getSmartTabSharingActive'));
    assertTrue(newComposebox.smartTabSharingActive);
  });

  test('UpdateAutoSuggestedTabContext_NullDoesNotDelete', () => {
    loadTimeData.overrideValues({webUIOmniboxAskGAboutThisPageEnabled: true});
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
    composebox.updateAutoSuggestedTabContextForTesting(null);
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
    composebox.updateAutoSuggestedTabContextForTesting(differentTab);
    assertTrue(deleteFileCalled);

    // Restore
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

  test('UpdateAutoSuggestedTabContext_NullDoesNotDeleteIfSidePanel', () => {
    loadTimeData.overrideValues({webUIOmniboxAskGAboutThisPageEnabled: false});
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

    // Call with null, should NOT delete because isSidePanel is true.
    composebox.updateAutoSuggestedTabContextForTesting(null);
    assertFalse(deleteFileCalled);

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
// LINT.ThenChange(//ui/webui/resources/cr_components/composebox/Componentization.md)
