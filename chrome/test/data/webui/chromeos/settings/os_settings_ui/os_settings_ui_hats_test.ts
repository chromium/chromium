// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the OS Settings ui for hats. */

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, CrToolbarSearchFieldElement, OsSettingsHatsBrowserProxyImpl, OsSettingsSearchBoxElement, OsSettingsUiElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';
import {clearBody} from '../utils.js';

import {TestOsSettingsHatsBrowserProxy} from './test_os_settings_hats_browser_proxy.js';

suite('<os-settings-ui> HaTS', () => {
  let browserProxy: TestOsSettingsHatsBrowserProxy;
  let field: CrToolbarSearchFieldElement|null;
  let searchBox: OsSettingsSearchBoxElement|null;
  let ui: OsSettingsUiElement;
  let testAccountManagerBrowserProxy: TestAccountManagerBrowserProxy;

  suiteSetup(async () => {
    browserProxy = new TestOsSettingsHatsBrowserProxy();
    OsSettingsHatsBrowserProxyImpl.setInstanceForTesting(browserProxy);

    // Setup fake accounts. There must be a device account available for the
    // Accounts menu item in <os-settings-menu>.
    testAccountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(
        testAccountManagerBrowserProxy);

    clearBody();
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    await CrSettingsPrefs.initialized;
    flush();
  });

  suiteTeardown(() => {
    ui.remove();
  });

  teardown(() => {
    browserProxy.reset();
    testAccountManagerBrowserProxy.reset();
  });

  test(
      'sendSettingsHats is sent when user shifts focus off the Settings page',
      async () => {
        window.dispatchEvent(new Event('blur'));
        await browserProxy.whenCalled('sendSettingsHats');
      });

  suite('with search', () => {
    async function simulateSearch(term: string) {
      assert(field);
      field.$.searchInput.value = term;
      field.onSearchTermInput();
      field.onSearchTermSearch();
      if (term) {
        // search-results-fetched only fires on a non-empty search term.
        await waitForResultsFetched();
      }
      flush();
    }

    async function waitForResultsFetched(): Promise<void> {
      // Wait for search results to be fetched.
      assert(searchBox);
      await eventToPromise('search-results-fetched', searchBox);
      flush();
    }

    function retrieveSearchBox(): void {
      const toolbar = ui.shadowRoot!.querySelector('settings-toolbar');
      assert(toolbar);
      searchBox = toolbar.shadowRoot!.querySelector('os-settings-search-box');
      assert(searchBox);
      field = searchBox.shadowRoot!.querySelector('cr-toolbar-search-field');
      assert(field);
    }

    teardown(async () => {
      await simulateSearch('');
    });

    test(
        'settingsUsedSearch is sent when user types into the searchbox',
        async () => {
          retrieveSearchBox();
          const searchQuery = 'query 1';
          await simulateSearch(searchQuery);
          await browserProxy.whenCalled('settingsUsedSearch');
        });
  });
});
