// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox.js';
import 'chrome://contextual-tasks/strings.m.js';

import {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from './composebox_test_utils.js';

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
            mock, new SearchboxPageHandlerRemote(),
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
});
// LINT.ThenChange(//ui/webui/resources/cr_components/composebox/Componentization.md)
