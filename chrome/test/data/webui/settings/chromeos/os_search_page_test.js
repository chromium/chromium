// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('os_settings_search_page', function() {
  function generateSearchEngineInfo() {
    const searchEngines0 =
        settings_search.createSampleSearchEngine(true, false, false);
    searchEngines0.default = true;
    const searchEngines1 =
        settings_search.createSampleSearchEngine(true, false, false);
    searchEngines1.default = false;
    const searchEngines2 =
        settings_search.createSampleSearchEngine(true, false, false);
    searchEngines2.default = false;

    return {
      defaults: [searchEngines0, searchEngines1, searchEngines2],
      others: [],
      extensions: [],
    };
  }

  suite('OSSearchPageTests', function() {
    /** @type {?SettingsSearchPageElement} */
    let page = null;

    let browserProxy = null;

    setup(function() {
      browserProxy = new settings_search.TestSearchEnginesBrowserProxy();
      browserProxy.setSearchEnginesInfo(generateSearchEngineInfo());
      settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      page = document.createElement('os-settings-search-page');
      page.prefs = {
        default_search_provider_data: {
          template_url_data: {},
        },
      };
      document.body.appendChild(page);
    });

    teardown(function() {
      page.remove();
    });

    // Tests that the page is querying and displaying search engine info on
    // startup.
    test('Initialization', async () => {
      const selectElement = page.$$('select');
      assertTrue(!!selectElement);
      assertTrue(!!page.$$('#help-icon'));

      await browserProxy.whenCalled('getSearchEnginesList');
      Polymer.dom.flush();
      assertEquals(0, selectElement.selectedIndex);

      // Simulate a user initiated change of the default search engine.
      selectElement.selectedIndex = 1;
      selectElement.dispatchEvent(new CustomEvent('change'));
      await browserProxy.whenCalled('setDefaultSearchEngine');
      assertEquals(1, selectElement.selectedIndex);

      // Simulate a change that happened in browser settings.
      const searchEnginesInfo = generateSearchEngineInfo();
      searchEnginesInfo.defaults[0].default = false;
      searchEnginesInfo.defaults[1].default = false;
      searchEnginesInfo.defaults[2].default = true;

      browserProxy.resetResolver('setDefaultSearchEngine');
      cr.webUIListenerCallback('search-engines-changed', searchEnginesInfo);
      Polymer.dom.flush();
      assertEquals(2, selectElement.selectedIndex);

      browserProxy.whenCalled('setDefaultSearchEngine').then(function() {
        // Since the change happened in a different tab, there should be
        // no new call to |setDefaultSearchEngine|.
        assertNotReached('Should not call setDefaultSearchEngine again');
      });
    });

    test('ControlledByExtension', async () => {
      await browserProxy.whenCalled('getSearchEnginesList');
      const selectElement = page.$$('select');
      assertFalse(selectElement.disabled);
      assertFalse(!!page.$$('extension-controlled-indicator'));

      page.set('prefs.default_search_provider_data.template_url_data', {
        controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
        controlledByName: 'fake extension name',
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        extensionId: 'fake extension id',
        extensionCanBeDisabled: true,
        value: {},
      });
      Polymer.dom.flush();

      assertTrue(selectElement.disabled);
      assertTrue(!!page.$$('extension-controlled-indicator'));
      assertFalse(!!page.$$('cr-policy-pref-indicator'));
    });

    test('ControlledByPolicy', async () => {
      await browserProxy.whenCalled('getSearchEnginesList');
      const selectElement = page.$$('select');
      assertFalse(selectElement.disabled);
      assertFalse(!!page.$$('extension-controlled-indicator'));

      page.set('prefs.default_search_provider_data.template_url_data', {
        controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        value: {},
      });
      Polymer.dom.flush();

      assertTrue(selectElement.disabled);
      assertFalse(!!page.$$('extension-controlled-indicator'));
      assertTrue(!!page.$$('cr-policy-pref-indicator'));
    });
  });
});
