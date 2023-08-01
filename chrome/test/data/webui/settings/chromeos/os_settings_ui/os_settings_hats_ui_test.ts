// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the OS Settings ui for hats. */

import {CrSettingsPrefs, OsSettingsHatsBrowserProxyImpl, OsSettingsSearchBoxElement, OsSettingsUiElement, OsToolbarElement} from 'chrome://os-settings/os_settings.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestOsSettingsHatsBrowserProxy} from './test_os_settings_hats_browser_proxy.js';

suite('OSSettingsUiHats', function() {
  let browserProxy: TestOsSettingsHatsBrowserProxy|null = null;
  let field: CrToolbarSearchFieldElement|null;
  let searchBox: OsSettingsSearchBoxElement|null;
  let toolbar: OsToolbarElement|null;
  let ui: OsSettingsUiElement;

  suiteSetup(async function() {
    browserProxy = new TestOsSettingsHatsBrowserProxy();
    OsSettingsHatsBrowserProxyImpl.setInstanceForTesting(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    await CrSettingsPrefs.initialized;
    flush();
  });

  test(
      'sendSettingsHats is sent when user shifts focus off the Settings page',
      async () => {
        window.dispatchEvent(new Event('blur'));
        assert(browserProxy);
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
      toolbar = ui.shadowRoot!.querySelector('os-toolbar');
      assert(toolbar);
      searchBox = toolbar.shadowRoot!.querySelector('os-settings-search-box');
      assert(searchBox);
      field = searchBox.shadowRoot!.querySelector('cr-toolbar-search-field');
      assert(field);
    }

    teardown(async function() {
      await simulateSearch('');
    });

    test(
        'settingsUsedSearch is sent when user types into the searchbox',
        async () => {
          retrieveSearchBox();
          const searchQuery = 'query 1';
          await simulateSearch(searchQuery);
          assert(browserProxy);
          await browserProxy.whenCalled('settingsUsedSearch');
        });
  });
});
