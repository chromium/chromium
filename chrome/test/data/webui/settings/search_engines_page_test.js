// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_search_engines_page', function() {
  /**
   * @param {number} id
   * @param {string} name
   * @param {boolean} canBeDefault
   * @param {boolean} canBeEdited
   * @param {boolean} canBeRemoved
   * @return {!SearchEngine}
   */
  function createSampleSearchEngine(
      id, name, canBeDefault, canBeEdited, canBeRemoved) {
    return {
      canBeDefault: canBeDefault,
      canBeEdited: canBeEdited,
      canBeRemoved: canBeRemoved,
      default: false,
      displayName: name + ' displayName',
      iconURL: 'http://www.google.com/favicon.ico',
      id: id,
      isOmniboxExtension: false,
      keyword: name,
      modelIndex: 0,
      name: name,
      url: 'https://' + name + '.com/search?p=%s',
      urlLocked: false,
    };
  }

  /** @return {!SearchEngine} */
  function createSampleOmniboxExtension() {
    return {
      canBeDefault: false,
      canBeEdited: false,
      canBeRemoved: false,
      default: false,
      displayName: 'Omnibox extension displayName',
      extension: {
        icon: 'chrome://extension-icon/some-extension-icon',
        id: 'dummyextensionid',
        name: 'Omnibox extension'
      },
      id: 0,
      isOmniboxExtension: true,
      keyword: 'oe',
      modelIndex: 6,
      name: 'Omnibox extension',
      url: 'chrome-extension://dummyextensionid/?q=%s',
      urlLocked: false
    };
  }

  function registerDialogTests() {
    suite('AddSearchEngineDialogTests', function() {
      /** @type {?SettingsAddSearchEngineDialog} */
      let dialog = null;
      let browserProxy = null;

      setup(function() {
        browserProxy = new settings_search.TestSearchEnginesBrowserProxy();
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        dialog = document.createElement('settings-search-engine-dialog');
        document.body.appendChild(dialog);
      });

      teardown(function() {
        dialog.remove();
      });

      // Tests that the dialog calls 'searchEngineEditStarted' and
      // 'searchEngineEditCancelled' when closed from the 'cancel' button.
      test('DialogOpenAndCancel', function() {
        return browserProxy.whenCalled('searchEngineEditStarted')
            .then(function() {
              dialog.$.cancel.click();
              return browserProxy.whenCalled('searchEngineEditCancelled');
            });
      });

      // Tests the dialog to add a new search engine. Specifically
      //  - cr-input elements are empty initially.
      //  - action button initially disabled.
      //  - validation is triggered on 'input' event.
      //  - action button is enabled when all fields are valid.
      //  - action button triggers appropriate browser signal when tapped.
      test('DialogAddSearchEngine', function() {
        /**
         * Triggers an 'input' event on the cr-input element and checks that
         * validation is triggered.
         * @param {string} inputId
         * @return {!Promise}
         */
        const inputAndValidate = inputId => {
          const inputElement = dialog.$[inputId];
          browserProxy.resetResolver('validateSearchEngineInput');
          inputElement.fire('input');
          return inputElement.value != '' ?
              // Expecting validation only on non-empty values.
              browserProxy.whenCalled('validateSearchEngineInput') :
              Promise.resolve();
        };

        const actionButton = dialog.$.actionButton;

        return browserProxy.whenCalled('searchEngineEditStarted')
            .then(() => {
              assertEquals('', dialog.$.searchEngine.value);
              assertEquals('', dialog.$.keyword.value);
              assertEquals('', dialog.$.queryUrl.value);
              assertTrue(actionButton.disabled);
            })
            .then(() => inputAndValidate('searchEngine'))
            .then(() => inputAndValidate('keyword'))
            .then(() => inputAndValidate('queryUrl'))
            .then(() => {
              // Manually set the text to a non-empty string for all fields.
              dialog.$.searchEngine.value = 'foo';
              dialog.$.keyword.value = 'bar';
              dialog.$.queryUrl.value = 'baz';

              return inputAndValidate('searchEngine');
            })
            .then(() => {
              // Assert that the action button has been enabled now that all
              // input is valid and non-empty.
              assertFalse(actionButton.disabled);
              actionButton.click();
              return browserProxy.whenCalled('searchEngineEditCompleted');
            });
      });

      test('DialogCloseWhenEnginesChangedModelEngineNotFound', function() {
        dialog.set(
            'model', createSampleSearchEngine(0, 'G', false, false, false));
        cr.webUIListenerCallback('search-engines-changed', {
          defaults: [],
          others: [createSampleSearchEngine(1, 'H', false, false, false)],
          extensions: [],
        });
        return browserProxy.whenCalled('searchEngineEditCancelled');
      });

      test('DialogValidateInputsWhenEnginesChanged', function() {
        dialog.set(
            'model', createSampleSearchEngine(0, 'G', false, false, false));
        dialog.set('keyword_', 'G');
        cr.webUIListenerCallback('search-engines-changed', {
          defaults: [],
          others: [createSampleSearchEngine(0, 'G', false, false, false)],
          extensions: [],
        });
        return browserProxy.whenCalled('validateSearchEngineInput');
      });
    });
  }

  function registerSearchEngineEntryTests() {
    suite('SearchEngineEntryTests', function() {
      /** @type {?SettingsSearchEngineEntryElement} */
      let entry = null;

      /** @type {!settings_search.TestSearchEnginesBrowserProxy} */
      let browserProxy = null;

      /** @type {!SearchEngine} */
      const searchEngine = createSampleSearchEngine(0, 'G', true, true, true);

      setup(function() {
        browserProxy = new settings_search.TestSearchEnginesBrowserProxy();
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        entry = document.createElement('settings-search-engine-entry');
        entry.set('engine', searchEngine);
        document.body.appendChild(entry);
      });

      teardown(function() {
        entry.remove();
      });

      // Test that the <search-engine-entry> is populated according to its
      // underlying SearchEngine model.
      test('Initialization', function() {
        assertEquals(
            searchEngine.displayName,
            entry.root.querySelector('#name-column').textContent.trim());
        assertEquals(
            searchEngine.keyword,
            entry.root.querySelector('#keyword-column').textContent);
        assertEquals(
            searchEngine.url,
            entry.root.querySelector('#url-column').textContent);
      });

      test('Remove_Enabled', function() {
        // Open action menu.
        entry.$$('cr-icon-button').click();
        const menu = entry.$$('cr-action-menu');
        assertTrue(menu.open);

        const deleteButton = entry.$.delete;
        assertTrue(!!deleteButton);
        assertFalse(deleteButton.hidden);
        deleteButton.click();
        return browserProxy.whenCalled('removeSearchEngine')
            .then(function(modelIndex) {
              assertFalse(menu.open);
              assertEquals(entry.engine.modelIndex, modelIndex);
            });
      });

      test('MakeDefault_Enabled', function() {
        // Open action menu.
        entry.$$('cr-icon-button').click();
        const menu = entry.$$('cr-action-menu');
        assertTrue(menu.open);

        const makeDefaultButton = entry.$.makeDefault;
        assertTrue(!!makeDefaultButton);
        makeDefaultButton.click();
        return browserProxy.whenCalled('setDefaultSearchEngine')
            .then(function(modelIndex) {
              assertFalse(menu.open);
              assertEquals(entry.engine.modelIndex, modelIndex);
            });
      });

      // Test that clicking the "edit" fires edit event.
      test('Edit_Enabled', function() {
        // Open action menu.
        entry.$$('cr-icon-button').click();
        const menu = entry.$$('cr-action-menu');
        assertTrue(menu.open);

        const engine = entry.engine;
        const editButton = entry.$.edit;
        assertTrue(!!editButton);
        assertFalse(editButton.hidden);

        const promise =
            test_util.eventToPromise('edit-search-engine', entry).then(e => {
              assertEquals(engine, e.detail.engine);
              assertEquals(entry.$$('cr-icon-button'), e.detail.anchorElement);
            });
        editButton.click();
        return promise;
      });

      /**
       * Checks that the given button is disabled (by being hidden), for the
       * given search engine.
       * @param {!SearchEngine} searchEngine
       * @param {string} buttonId
       */
      function testButtonDisabled(searchEngine, buttonId) {
        entry.engine = searchEngine;
        const button = entry.$[buttonId];
        assertTrue(!!button);
        assertTrue(button.hidden);
      }

      test('Remove_Disabled', function() {
        testButtonDisabled(
            createSampleSearchEngine(0, 'G', true, true, false), 'delete');
      });

      test('MakeDefault_Disabled', function() {
        testButtonDisabled(
            createSampleSearchEngine(0, 'G', false, true, true), 'makeDefault');
      });

      test('Edit_Disabled', function() {
        testButtonDisabled(
            createSampleSearchEngine(0, 'G', true, false, true), 'edit');
      });

      test('All_Disabled', function() {
        entry.engine = createSampleSearchEngine(0, 'G', true, false, false);
        Polymer.dom.flush();
        assertTrue(entry.hasAttribute('show-dots_'));

        entry.engine = createSampleSearchEngine(1, 'G', false, false, false);
        Polymer.dom.flush();
        assertFalse(entry.hasAttribute('show-dots_'));
      });
    });
  }

  function registerPageTests() {
    suite('SearchEnginePageTests', function() {
      /** @type {?SettingsSearchEnginesPageElement} */
      let page = null;

      let browserProxy = null;

      /** @type {!SearchEnginesInfo} */
      const searchEnginesInfo = {
        defaults: [createSampleSearchEngine(
            0, 'search_engine_G', false, false, false)],
        others: [
          createSampleSearchEngine(1, 'search_engine_B', false, false, false),
          createSampleSearchEngine(2, 'search_engine_A', false, false, false),
        ],
        extensions: [createSampleOmniboxExtension()],
      };

      setup(function() {
        browserProxy = new settings_search.TestSearchEnginesBrowserProxy();

        // Purposefully pass a clone of |searchEnginesInfo| to avoid any
        // mutations on ground truth data.
        browserProxy.setSearchEnginesInfo({
          defaults: searchEnginesInfo.defaults.slice(),
          others: searchEnginesInfo.others.slice(),
          extensions: searchEnginesInfo.extensions.slice(),
        });
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-search-engines-page');
        document.body.appendChild(page);
        return browserProxy.whenCalled('getSearchEnginesList');
      });

      teardown(function() {
        page.remove();
      });

      // Tests that the page is querying and displaying search engine info on
      // startup.
      test('Initialization', function() {
        const searchEnginesLists =
            page.shadowRoot.querySelectorAll('settings-search-engines-list');
        assertEquals(2, searchEnginesLists.length);

        // Note: iron-list may create hidden children, so test the length
        // if IronList.items instead of the child nodes.
        Polymer.dom.flush();
        const defaultsList = searchEnginesLists[0];
        const defaultsEntries =
            defaultsList.shadowRoot.querySelector('iron-list').items;
        assertEquals(searchEnginesInfo.defaults.length, defaultsEntries.length);

        const othersList = searchEnginesLists[1];
        const othersEntries =
            othersList.shadowRoot.querySelector('iron-list').items;
        assertEquals(searchEnginesInfo.others.length, othersEntries.length);

        // Ensure that the search engines have reverse alphabetical order in the
        // model.
        assertGT(
            searchEnginesInfo.others[0].name, searchEnginesInfo.others[1].name);

        // Ensure that they are displayed in alphabetical order.
        assertEquals(searchEnginesInfo.others[1].name, othersEntries[0].name);
        assertEquals(searchEnginesInfo.others[0].name, othersEntries[1].name);

        const extensionEntries =
            page.shadowRoot.querySelector('iron-list').items;
        assertEquals(
            searchEnginesInfo.extensions.length, extensionEntries.length);
      });

      // Test that the "no other search engines" message is shown/hidden as
      // expected.
      test('NoOtherSearchEnginesMessage', function() {
        cr.webUIListenerCallback('search-engines-changed', {
          defaults: [],
          others: [],
          extensions: [],
        });

        const message = page.root.querySelector('#noOtherEngines');
        assertTrue(!!message);
        assertFalse(message.hasAttribute('hidden'));

        cr.webUIListenerCallback('search-engines-changed', {
          defaults: [],
          others: [createSampleSearchEngine(0, 'G', false, false, false)],
          extensions: [],
        });
        assertTrue(message.hasAttribute('hidden'));
      });

      // Tests that the add search engine dialog opens when the corresponding
      // button is tapped.
      test('AddSearchEngineDialog', function() {
        assertFalse(!!page.$$('settings-search-engine-dialog'));
        const addSearchEngineButton = page.$.addSearchEngine;
        assertTrue(!!addSearchEngineButton);

        addSearchEngineButton.click();
        Polymer.dom.flush();
        assertTrue(!!page.$$('settings-search-engine-dialog'));
      });

      test('EditSearchEngineDialog', function() {
        const engine = searchEnginesInfo.others[0];
        page.fire(
            'edit-search-engine',
            {engine, anchorElement: page.$.addSearchEngine});
        return browserProxy.whenCalled('searchEngineEditStarted')
            .then(modelIndex => {
              assertEquals(engine.modelIndex, modelIndex);
              const dialog = page.$$('settings-search-engine-dialog');
              assertTrue(!!dialog);

              // Check that the cr-input fields are pre-populated.
              assertEquals(engine.name, dialog.$.searchEngine.value);
              assertEquals(engine.keyword, dialog.$.keyword.value);
              assertEquals(engine.url, dialog.$.queryUrl.value);

              assertFalse(dialog.$.actionButton.disabled);
            });
      });

      // Tests that filtering the three search engines lists works, and that the
      // "no search results" message is shown as expected.
      test('FilterSearchEngines', function() {
        Polymer.dom.flush();

        function getListItems(listIndex) {
          const ironList = listIndex == 2 /* extensions */ ?
              page.shadowRoot.querySelector('iron-list') :
              page.shadowRoot
                  .querySelectorAll('settings-search-engines-list')[listIndex]
                  .shadowRoot.querySelector('iron-list');

          return ironList.items;
        }

        function getDefaultEntries() {
          return getListItems(0);
        }
        function getOtherEntries() {
          return getListItems(1);
        }

        function assertSearchResults(
            defaultsCount, othersCount, extensionsCount) {
          assertEquals(defaultsCount, getListItems(0).length);
          assertEquals(othersCount, getListItems(1).length);
          assertEquals(extensionsCount, getListItems(2).length);

          const noResultsElements = Array.from(
              page.shadowRoot.querySelectorAll('.no-search-results'));
          assertEquals(defaultsCount > 0, noResultsElements[0].hidden);
          assertEquals(othersCount > 0, noResultsElements[1].hidden);
          assertEquals(extensionsCount > 0, noResultsElements[2].hidden);
        }

        assertSearchResults(1, 2, 1);

        // Search by name
        page.filter = searchEnginesInfo.defaults[0].name;
        Polymer.dom.flush();
        assertSearchResults(1, 0, 0);

        // Search by displayName
        page.filter = searchEnginesInfo.others[0].displayName;
        Polymer.dom.flush();
        assertSearchResults(0, 1, 0);

        // Search by keyword
        page.filter = searchEnginesInfo.others[1].keyword;
        Polymer.dom.flush();
        assertSearchResults(0, 1, 0);

        // Search by URL
        page.filter = 'search?';
        Polymer.dom.flush();
        assertSearchResults(1, 2, 0);

        // Test case where none of the sublists have results.
        page.filter = 'does not exist';
        Polymer.dom.flush();
        assertSearchResults(0, 0, 0);

        // Test case where an 'extension' search engine matches.
        page.filter = 'extension';
        Polymer.dom.flush();
        assertSearchResults(0, 0, 1);
      });
    });
  }

  function registerOmniboxExtensionEntryTests() {
    suite('OmniboxExtensionEntryTests', function() {
      /** @type {?SettingsOmniboxExtensionEntryElement} */
      let entry = null;

      let browserProxy = null;

      setup(function() {
        browserProxy = new TestExtensionControlBrowserProxy();
        settings.ExtensionControlBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        entry = document.createElement('settings-omnibox-extension-entry');
        entry.set('engine', createSampleOmniboxExtension());
        document.body.appendChild(entry);

        // Open action menu.
        entry.$$('cr-icon-button').click();
      });

      teardown(function() {
        entry.remove();
      });

      test('Manage', function() {
        const manageButton = entry.$.manage;
        assertTrue(!!manageButton);
        manageButton.click();
        return browserProxy.whenCalled('manageExtension')
            .then(function(extensionId) {
              assertEquals(entry.engine.extension.id, extensionId);
            });
      });

      test('Disable', function() {
        const disableButton = entry.$.disable;
        assertTrue(!!disableButton);
        disableButton.click();
        return browserProxy.whenCalled('disableExtension')
            .then(function(extensionId) {
              assertEquals(entry.engine.extension.id, extensionId);
            });
      });
    });
  }

  registerDialogTests();
  registerSearchEngineEntryTests();
  registerOmniboxExtensionEntryTests();
  registerPageTests();
});
