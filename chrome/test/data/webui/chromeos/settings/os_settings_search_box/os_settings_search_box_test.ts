// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrToolbarSearchFieldElement, IronDropdownElement, IronListElement, OpenWindowProxyImpl, OsSettingsSearchBoxBrowserProxyImpl, OsSettingsSearchBoxElement, personalizationSearchMojom, Router, routes, routesMojom, searchMojom, searchResultIconMojom, setPersonalizationSearchHandlerForTesting, setSettingsSearchHandlerForTesting, settingMojom, setUserActionRecorderForTesting, ToolbarElement} from 'chrome://os-settings/os_settings.js';
import {mojoString16ToString, stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';
import {FakePersonalizationSearchHandler} from '../fake_personalization_search_handler.js';
import {FakeSettingsSearchHandler} from '../fake_settings_search_handler.js';
import {FakeUserActionRecorder} from '../fake_user_action_recorder.js';
import {clearBody} from '../utils.js';

import {TestOpenWindowProxy} from './test_open_window_proxy.js';
import {TestOsSettingsSearchBoxBrowserProxy} from './test_os_settings_search_box_browser_proxy.js';

suite('<os-settings-search-box>', () => {
  const DEFAULT_RELEVANCE_SCORE: number = 0.5;
  const DEFAULT_PAGE_HIERARCHY: String16[] = [];
  let toolbar: ToolbarElement;
  let searchBox: OsSettingsSearchBoxElement;
  let field: CrToolbarSearchFieldElement;
  let dropDown: IronDropdownElement;
  let resultList: IronListElement;
  let personalizationSearchHandler: FakePersonalizationSearchHandler;
  let settingsSearchHandler: FakeSettingsSearchHandler;
  let userActionRecorder: FakeUserActionRecorder;
  let noResultsSection: HTMLElement;
  let openWindowProxy: TestOpenWindowProxy;

  function isTextSelected(): boolean {
    const input = field.$.searchInput;
    return input.selectionStart === 0 &&
        input.selectionEnd === input.value.length;
  }

  async function simulateSearch(term: string): Promise<void> {
    field.$.searchInput.value = term;
    field.onSearchTermInput();
    field.onSearchTermSearch();
    if (term && term.trim().length !== 0) {
      // search-results-fetched only fires on a non-empty search term.
      await waitForResultsFetched();
    }
    flush();
  }

  async function waitForListUpdate(): Promise<void> {
    // Wait for iron-list to complete resizing.
    await eventToPromise('iron-resize', resultList);
    flush();
  }

  async function waitForResultsFetched(): Promise<void> {
    // Wait for search results to be fetched.
    await eventToPromise('search-results-fetched', searchBox);
    flush();
  }

  function fakeSettingsResult(
      text: string, urlPathWithParameters: string = '',
      icon?: searchResultIconMojom.SearchResultIcon,
      wasGeneratedFromTextMatch?: boolean,
      relevanceScore?: number): searchMojom.SearchResult {
    return {
      text: stringToMojoString16(text),
      canonicalText: stringToMojoString16(text),
      urlPathWithParameters,
      icon: icon ? icon : searchResultIconMojom.SearchResultIcon.MIN_VALUE,
      wasGeneratedFromTextMatch: wasGeneratedFromTextMatch === undefined ?
          true :
          wasGeneratedFromTextMatch,
      id: {
        section: routesMojom.Section.MIN_VALUE,
        subpage: routesMojom.Subpage.MIN_VALUE,
        setting: settingMojom.Setting.MIN_VALUE,
      },
      type: searchMojom.SearchResultType.MIN_VALUE,
      relevanceScore: typeof relevanceScore === 'number' ?
          relevanceScore :
          DEFAULT_RELEVANCE_SCORE,
      settingsPageHierarchy: DEFAULT_PAGE_HIERARCHY,
      defaultRank: 0,
    };
  }

  function fakePersonalizationResult(
      text: string, relativeUrl: string = '',
      relevanceScore: number =
          DEFAULT_RELEVANCE_SCORE): personalizationSearchMojom.SearchResult {
    return ({
      searchConceptId: personalizationSearchMojom.SearchConceptId.MIN_VALUE,
      text: stringToMojoString16(text),
      relativeUrl,
      relevanceScore,
    });
  }

  function setupSearchBox(): void {
    chrome.metricsPrivate = new FakeMetricsPrivate();

    personalizationSearchHandler = new FakePersonalizationSearchHandler();
    setPersonalizationSearchHandlerForTesting(personalizationSearchHandler);

    settingsSearchHandler = new FakeSettingsSearchHandler();
    setSettingsSearchHandlerForTesting(settingsSearchHandler);

    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);

    toolbar = document.createElement('settings-toolbar');
    assertTrue(!!toolbar);
    document.body.appendChild(toolbar);
    flush();

    const element = toolbar.shadowRoot!.querySelector('os-settings-search-box');
    assertTrue(!!element);
    searchBox = element;
    const searchField =
        searchBox.shadowRoot!.querySelector('cr-toolbar-search-field');
    assertTrue(!!searchField);
    field = searchField;
    const ironDropDown = searchBox.shadowRoot!.querySelector('iron-dropdown');
    assertTrue(!!ironDropDown);
    dropDown = ironDropDown;
    const ironList = searchBox.shadowRoot!.querySelector('iron-list');
    assertTrue(!!ironList);
    resultList = ironList;
    const noSearchResultsContainer =
        searchBox.shadowRoot!.querySelector<HTMLElement>(
            '#noSearchResultsContainer');
    assertTrue(!!noSearchResultsContainer);
    noResultsSection = noSearchResultsContainer;
  }

  suite('AllBuilds', () => {
    setup(() => {
      Router.getInstance().navigateTo(routes.BASIC);

      openWindowProxy = new TestOpenWindowProxy();
      OpenWindowProxyImpl.setInstance(openWindowProxy);
    });

    teardown(async () => {
      // Clear search field for next test.
      await simulateSearch('');
      toolbar.remove();
      openWindowProxy.reset();
      Router.getInstance().resetRouteForTesting();
    });

    test('Search availability changed', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults([fakeSettingsResult('result')]);
      await simulateSearch('test query');
      assertTrue(dropDown.opened);
      assertEquals(1, searchBox.get('searchResults_').length);

      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('1'), fakeSettingsResult('2')]);
      assertTrue(dropDown.opened);
      assertEquals(1, searchBox.get('searchResults_').length);

      // Check that the list updates when the dropdown is open, and the dropdown
      // remains open.
      settingsSearchHandler.simulateSearchResultsChanged();
      await waitForResultsFetched();
      assertTrue(dropDown.opened);
      assertEquals(2, searchBox.get('searchResults_').length);

      // Personalization search results should also result in a new search.
      personalizationSearchHandler.setFakeResults(
          [fakePersonalizationResult('personalization')]);
      assertTrue(dropDown.opened);
      assertEquals(2, searchBox.get('searchResults_').length);

      personalizationSearchHandler.simulateSearchResultsChanged();
      await waitForResultsFetched();
      assertTrue(dropDown.opened);
      assertEquals(3, searchBox.get('searchResults_').length);

      // User clicks outside the search box, closing the dropdown.
      searchBox.blur();
      assertFalse(dropDown.opened);

      settingsSearchHandler.setFakeResults([fakeSettingsResult('result')]);
      assertFalse(dropDown.opened);
      assertEquals(3, searchBox.get('searchResults_').length);

      // Check that the list updates when the dropdown is closed, and the
      // dropdown remains closed.
      settingsSearchHandler.simulateSearchResultsChanged();
      await waitForResultsFetched();
      assertFalse(dropDown.opened);
      assertEquals(2, searchBox.get('searchResults_').length);

      // The first item should be selected immediately when the search results
      // change even if the change occurred while the dropdown was closed.
      field.$.searchInput.focus();
      await waitForListUpdate();
      assertTrue(dropDown.opened);
      assertEquals(
          searchBox.get('selectedItem_').resultText,
          searchBox['getSelectedOsSearchResultRow_']().searchResult.resultText);
    });

    test('User action search event', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults([]);

      assertEquals(0, userActionRecorder.searchCount);
      await simulateSearch('query');
      assertEquals(1, userActionRecorder.searchCount);
    });

    test(
        'Clicking magnifying glass shows dropdown and selects all text',
        async () => {
          setupSearchBox();
          settingsSearchHandler.setFakeResults([fakeSettingsResult('a')]);
          await simulateSearch('query');
          await waitForListUpdate();
          assertTrue(dropDown.opened);
          searchBox.blur();

          assertFalse(dropDown.opened);
          assertFalse(isTextSelected());
          const icon =
              field.shadowRoot!.querySelector<HTMLButtonElement>('#icon');
          assertTrue(!!icon);
          icon.click();
          assertTrue(isTextSelected());
          assertTrue(dropDown.opened);
        });

    test('Dropdown opens correctly when results are fetched', async () => {
      setupSearchBox();
      // Show no results in dropdown if no results are returned.
      settingsSearchHandler.setFakeResults([]);
      personalizationSearchHandler.setFakeResults([]);
      assertFalse(dropDown.opened);
      await simulateSearch('query 1');
      assertTrue(dropDown.opened);
      assertEquals(0, searchBox.get('searchResults_').length);
      assertFalse(noResultsSection.hidden);

      assertEquals(1, userActionRecorder.searchCount);

      // Show result list if results are returned, and hide no results div.
      settingsSearchHandler.setFakeResults([fakeSettingsResult('result')]);
      personalizationSearchHandler.setFakeResults([]);
      await simulateSearch('query 2');
      assertNotEquals(0, searchBox.get('searchResults_').length);
      assertTrue(noResultsSection.hidden);

      // Show result list if personalization search results are returned, and
      // hide no results div.
      settingsSearchHandler.setFakeResults([]);
      personalizationSearchHandler.setFakeResults(
          [fakePersonalizationResult('personalization')]);
      await simulateSearch('query 3');
      assertNotEquals(0, searchBox.get('searchResults_').length);
      assertTrue(noResultsSection.hidden);
    });

    test('Restore previous existing search results', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults([fakeSettingsResult('result 1')]);
      personalizationSearchHandler.setFakeResults(
          [fakePersonalizationResult('personalization')]);
      await simulateSearch('query');
      assertTrue(dropDown.opened);
      assertEquals(2, resultList.items!.length);
      const [firstResult, secondResult] = resultList.items!;

      // Child blur elements except field should not trigger closing of
      // dropdown.
      resultList.blur();
      assertTrue(dropDown.opened);
      dropDown.blur();
      assertTrue(dropDown.opened);

      // User clicks outside the search box, closing the dropdown.
      searchBox.blur();
      assertFalse(dropDown.opened);

      // User clicks on input, restoring old results and opening dropdown.
      field.$.searchInput.focus();
      assertEquals('query', field.$.searchInput.value);
      assertTrue(dropDown.opened);

      // The same result rows exist.
      assertEquals(firstResult, resultList.items![0]);
      assertEquals(secondResult, resultList.items![1]);

      // Search field is blurred, closing the dropdown.
      field.$.searchInput.blur();
      assertFalse(dropDown.opened);

      // User clicks on input, restoring old results and opening dropdown.
      field.$.searchInput.focus();
      assertEquals('query', field.$.searchInput.value);
      assertTrue(dropDown.opened);

      // The same result rows exist.
      assertEquals(firstResult, resultList.items![0]);
      assertEquals(secondResult, resultList.items![1]);
    });

    test('Search result rows are selected correctly', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults([fakeSettingsResult('a')]);
      personalizationSearchHandler.setFakeResults(
          [fakePersonalizationResult('b')]);
      await simulateSearch('query');
      await waitForListUpdate();

      assertTrue(dropDown.opened);
      assertEquals(2, resultList.items!.length);

      // The first row should be selected when results are fetched.
      assertEquals(resultList.selectedItem, resultList.items![0]);

      // Test ArrowUp and ArrowDown interaction with selecting.
      const arrowUpEvent = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'ArrowUp', keyCode: 38});
      const arrowDownEvent = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'ArrowDown', keyCode: 40});

      // ArrowDown event should select next row.
      searchBox.dispatchEvent(arrowDownEvent);
      assertEquals(resultList.selectedItem, resultList.items![1]);

      // If last row selected, ArrowDown brings select back to first row.
      searchBox.dispatchEvent(arrowDownEvent);
      assertEquals(resultList.selectedItem, resultList.items![0]);

      // If first row selected, ArrowUp brings select back to last row.
      searchBox.dispatchEvent(arrowUpEvent);
      assertEquals(resultList.selectedItem, resultList.items![1]);

      // ArrowUp should bring select previous row.
      searchBox.dispatchEvent(arrowUpEvent);
      assertEquals(resultList.selectedItem, resultList.items![0]);

      // Test that ArrowLeft and ArrowRight do nothing.
      const arrowLeftEvent = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'ArrowLeft', keyCode: 37});
      const arrowRightEvent = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'ArrowRight', keyCode: 39});

      // No change on ArrowLeft
      searchBox.dispatchEvent(arrowLeftEvent);
      assertEquals(resultList.selectedItem, resultList.items![0]);

      // No change on ArrowRight
      searchBox.dispatchEvent(arrowRightEvent);
      assertEquals(resultList.selectedItem, resultList.items![0]);
    });

    test('Keydown Enter on search box can cause route change', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('WiFi Settings', 'networks?type=WiFi')]);
      await simulateSearch('fake query');
      await waitForListUpdate();

      const enterEvent = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'Enter', keyCode: 13});

      // Keydown with Enter key on the searchBox causes navigation to selected
      // row's route.
      searchBox.dispatchEvent(enterEvent);
      flush();
      assertFalse(dropDown.opened);
      const router = Router.getInstance();
      assertEquals('fake query', router.getQueryParameters().get('search'));
      assertEquals('/networks', router.currentRoute.path);
      assertEquals('WiFi', router.getQueryParameters().get('type'));
    });

    test(
        'Keypress Enter on personalization result opens personalization hub',
        async () => {
          setupSearchBox();
          personalizationSearchHandler.setFakeResults(
              [fakePersonalizationResult('result', 'test')]);
          settingsSearchHandler.setFakeResults([]);
          await simulateSearch('fake query 1');
          await waitForListUpdate();

          const selectedOsRow = searchBox['getSelectedOsSearchResultRow_']();
          assertTrue(!!selectedOsRow);
          assertEquals('cr:open-in-new', selectedOsRow.getActionTypeIcon_());

          // Keypress with Enter key on any row specifically causes navigation
          // to selected row's route. This can't happen unless the row is
          // focused.
          const enterEvent = new KeyboardEvent(
              'keypress', {cancelable: true, key: 'Enter', keyCode: 13});
          selectedOsRow.$.searchResultContainer.dispatchEvent(enterEvent);

          assertEquals(
              'chrome://personalization/test',
              await openWindowProxy.whenCalled('openUrl'));
        });

    test(
        'Clicking on personalization result opens personalization hub',
        async () => {
          setupSearchBox();
          personalizationSearchHandler.setFakeResults(
              [fakePersonalizationResult('Wallpaper', 'test')]);
          await simulateSearch('fake query 1');
          await waitForListUpdate();

          const selectedOsRow = searchBox['getSelectedOsSearchResultRow_']();
          assertTrue(!!selectedOsRow);
          assertEquals('cr:open-in-new', selectedOsRow.getActionTypeIcon_());

          // Clicking on the searchResultContainer of the row opens a new
          // window.
          selectedOsRow.$.searchResultContainer.click();

          assertEquals(
              'chrome://personalization/test',
              await openWindowProxy.whenCalled('openUrl'));
        });

    test('Keypress Enter on row causes route change', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('WiFi Settings', 'networks?type=WiFi')]);
      await simulateSearch('fake query 1');
      await waitForListUpdate();

      const selectedOsRow = searchBox['getSelectedOsSearchResultRow_']();
      assertTrue(!!selectedOsRow);

      // Keypress with Enter key on any row specifically causes navigation to
      // selected row's route. This can't happen unless the row is focused.
      const enterEvent = new KeyboardEvent(
          'keypress', {cancelable: true, key: 'Enter', keyCode: 13});
      selectedOsRow.$.searchResultContainer.dispatchEvent(enterEvent);
      assertFalse(dropDown.opened);
      const router = Router.getInstance();
      assertEquals('fake query 1', router.getQueryParameters().get('search'));
      assertEquals('/networks', router.currentRoute.path);
      assertEquals('WiFi', router.getQueryParameters().get('type'));
    });

    test('Route change when result row is clicked', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('WiFi Settings', 'networks?type=WiFi')]);
      await simulateSearch('fake query 2');
      await waitForListUpdate();

      const searchResultRow = searchBox['getSelectedOsSearchResultRow_']();

      // Clicking on the searchResultContainer of the row correctly changes the
      // route and dropdown to close.
      searchResultRow.$.searchResultContainer.click();

      assertFalse(dropDown.opened);
      const router = Router.getInstance();
      assertEquals('fake query 2', router.getQueryParameters().get('search'));
      assertEquals('/networks', router.currentRoute.path);
      assertEquals('WiFi', router.getQueryParameters().get('type'));
    });

    test('Selecting result a second time does not deselect it.', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('WiFi Settings', 'networks?type=WiFi')]);
      await simulateSearch('query');
      await waitForListUpdate();

      // Clicking a selected item does not deselect it.
      const searchResultRow = searchBox['getSelectedOsSearchResultRow_']();
      searchResultRow.$.searchResultContainer.click();
      assertEquals(resultList.selectedItem, resultList.items![0]);
      assertFalse(dropDown.opened);

      // Open search drop down again.
      field.$.searchInput.focus();
      assertTrue(dropDown.opened);

      // Clicking again does not deslect the row.
      searchResultRow.$.searchResultContainer.click();
      assertEquals(resultList.selectedItem, resultList.items![0]);
    });

    test('Test no bolding if not generated from text match', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults([fakeSettingsResult(
          'Search and Assistant', undefined, undefined,
          /*wasGeneratedFromTextMatch=*/ false)]);
      await simulateSearch('Search');
      await waitForListUpdate();
      assertEquals(
          'Search and Assistant',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
    });

    test('Tokenize and match result text to query text', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('Search and Assistant')]);
      await simulateSearch('Assistant Search');
      await waitForListUpdate();
      assertEquals(
          '<b>Search</b> and <b>Assistant</b>',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
    });

    test('Bold result text to matching query', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('Search and Assistant')]);
      await simulateSearch('a');
      await waitForListUpdate();
      assertEquals(
          'Se<b>a</b>rch <b>a</b>nd <b>A</b>ssist<b>a</b>nt',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
    });

    test('Bold result including ignored characters', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('Turn on Wi-Fi')]);
      await simulateSearch('wif');
      await waitForListUpdate();
      assertEquals(
          'Turn on <b>Wi-F</b>i',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
      await simulateSearch('wi f');
      assertEquals(
          'Turn on <b>Wi-F</b>i',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
      await simulateSearch('wi-f');
      assertEquals(
          'Turn on <b>Wi-F</b>i',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);

      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('Touchpad tap-to-click')]);
      await simulateSearch('tap to cli');
      assertEquals(
          'Touchpad <b>tap-to-cli</b>ck',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);

      await simulateSearch('taptocli');
      assertEquals(
          'Touchpad <b>tap-to-cli</b>ck',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
      await simulateSearch('tap-to-cli');
      assertEquals(
          'Touchpad <b>tap-to-cli</b>ck',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);

      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('Touchpad tap-to-click')]);
      await simulateSearch('tap top cli');
      assertEquals(
          'Touchpad <b>tap-to-cli</b>ck',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);

      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('wxyz Tap-To-Click')]);
      await simulateSearch('tap toxy cli');
      assertEquals(
          'w<b>xy</b>z <b>Tap-To</b>-Click',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);

      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('Tap-to-click Tips Title')]);
      await simulateSearch('tap ti');
      assertEquals(
          '<b>Tap</b>-to-click <b>Ti</b>ps <b>Ti</b>tle',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
    });

    test('Test query longer than result blocks', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('Turn on Wi-Fi')]);
      await simulateSearch('onwifi');
      await waitForListUpdate();
      assertEquals(
          'Turn <b>on</b> <b>Wi-Fi</b>',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
    });

    test('Test bolding of accented characters', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('Crème Brûlée')]);
      await simulateSearch('E U');
      await waitForListUpdate();
      assertEquals(
          'Cr<b>è</b>me Br<b>û</b>l<b>é</b>e',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
    });

    test(
        'Test no spaces nor characters that have upper/lower case',
        async () => {
          setupSearchBox();
          settingsSearchHandler.setFakeResults(
              [fakeSettingsResult('キーボード設定---')]);
          await simulateSearch('キー設');
          await waitForListUpdate();
          assertEquals(
              '<b>キ</b><b>ー</b>ボ<b>ー</b>ド<b>設</b>定---',
              searchBox['getSelectedOsSearchResultRow_']()
                  .$.resultText.innerHTML);
        });

    test('Test blankspace types in result maintained', async () => {
      setupSearchBox();
      const resultText = 'Turn\xa0on  \xa0Wi-Fi ';

      settingsSearchHandler.setFakeResults([fakeSettingsResult(resultText)]);
      await simulateSearch('wif');
      await waitForListUpdate();
      assertEquals(
          'Turn&nbsp;on  &nbsp;<b>Wi-F</b>i ',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
    });

    test('Test longest common substring for mispellings', async () => {
      setupSearchBox();
      settingsSearchHandler.setFakeResults([fakeSettingsResult('Linux')]);
      await simulateSearch('Linuux');
      await waitForListUpdate();
      assertEquals(
          '<b>Linu</b>x',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);

      settingsSearchHandler.setFakeResults([fakeSettingsResult('Linux')]);
      await simulateSearch('Llinuc');
      assertEquals(
          '<b>Linu</b>x',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);

      settingsSearchHandler.setFakeResults([fakeSettingsResult('Display')]);
      await simulateSearch('Dispplay');
      assertEquals(
          '<b>Disp</b>lay',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);

      settingsSearchHandler.setFakeResults(
          [fakeSettingsResult('ABCDEF GHIJK LMNO')]);
      await simulateSearch('MCDEMMM LM EF CDEABCDEFADBCDABDCEF');
      assertEquals(
          '<b>ABCDEF</b> GHIJK <b>LM</b>NO',
          searchBox['getSelectedOsSearchResultRow_']().$.resultText.innerHTML);
    });

    test('Focus search input behavior on attached', () => {
      clearBody();
      Router.getInstance().navigateTo(routes.BASIC);
      setupSearchBox();
      assertEquals(field.$.searchInput, field.shadowRoot!.activeElement);

      clearBody();
      Router.getInstance().navigateTo(routes.KEYBOARD);
      assertNull(field.shadowRoot!.activeElement);
    });

    test('search results sorted descending', async () => {
      setupSearchBox();
      personalizationSearchHandler.setFakeResults([
        fakePersonalizationResult(
            'one', /*relativeUrl=*/ '', /*relevanceScore=*/ 0.99),
        fakePersonalizationResult(
            'three', /*relativeUrl=*/ '', /*relevanceScore=*/ 0.75),
        fakePersonalizationResult(
            'cut', /*relativeUrl=*/ '', /*relevanceScore=*/ 0.2),
      ]);
      settingsSearchHandler.setFakeResults([
        fakeSettingsResult(
            'two', /*urlPathWithParameters=*/ '',
            /*icon=*/ searchResultIconMojom.SearchResultIcon.MIN_VALUE,
            /*wasGeneratedFromTextMatch=*/ true, /*relevanceScore=*/ 0.85),
        fakeSettingsResult(
            'four', /*urlPathWithParameters=*/ '',
            /*icon=*/ searchResultIconMojom.SearchResultIcon.MIN_VALUE,
            /*wasGeneratedFromTextMatch=*/ true, /*relevanceScore=*/ 0.55),
        fakeSettingsResult(
            'five', /*urlPathWithParameters=*/ '',
            /*icon=*/ searchResultIconMojom.SearchResultIcon.MIN_VALUE,
            /*wasGeneratedFromTextMatch=*/ true, /*relevanceScore=*/ 0.35),
      ]);

      await simulateSearch('fake query');
      await waitForListUpdate();

      assertEquals(5, resultList.items!.length, 'results cut to show top 5');

      assertDeepEquals(
          [
            {text: 'one', relevanceScore: 0.99},
            {text: 'two', relevanceScore: 0.85},
            {text: 'three', relevanceScore: 0.75},
            {text: 'four', relevanceScore: 0.55},
            {text: 'five', relevanceScore: 0.35},
          ],
          resultList.items!.map((item: searchMojom.SearchResult) => {
            return {
              text: mojoString16ToString(item.text),
              relevanceScore: item.relevanceScore,
            };
          }),
          'search results sorted in expected order');
    });
  });

  suite('OfficialBuild', () => {
    let browserProxy: TestOsSettingsSearchBoxBrowserProxy;

    setup(() => {
      browserProxy = new TestOsSettingsSearchBoxBrowserProxy();
      OsSettingsSearchBoxBrowserProxyImpl.setInstanceForTesting(browserProxy);

      setupSearchBox();
    });

    teardown(() => {
      toolbar.remove();
      browserProxy.reset();
    });

    test(
        'feedback button does not appear when search result exists',
        async () => {
          settingsSearchHandler.setFakeResults(
              [fakeSettingsResult('assistant')]);
          await simulateSearch('a');
          assertTrue(dropDown.opened);
          assertEquals(1, searchBox.get('searchResults_').length);
          assertTrue(noResultsSection.hidden);
          const feedbackReportResults =
              searchBox.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#reportSearchResultButton');
          assertTrue(!!feedbackReportResults);
          assertTrue(feedbackReportResults.hidden);
        });

    test(
        'feedback button appears when search result does not exist',
        async () => {
          settingsSearchHandler.setFakeResults([]);
          await simulateSearch('query 1');
          assertTrue(dropDown.opened);
          assertEquals(0, searchBox.get('searchResults_').length);
          assertFalse(noResultsSection.hidden);
          // feedback button appears when no search results have been found
          const feedbackReportResults =
              searchBox.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#reportSearchResultButton');
          assertTrue(!!feedbackReportResults);
          assertFalse(feedbackReportResults.hidden);
        });

    test('clicking the button opens feedback dialog', async () => {
      settingsSearchHandler.setFakeResults([]);
      const searchQuery = 'query 1';
      await simulateSearch(searchQuery);
      const feedbackReportResults =
          searchBox.shadowRoot!.querySelector<HTMLButtonElement>(
              '#reportSearchResultButton');
      assertTrue(!!feedbackReportResults);
      feedbackReportResults.click();
      const descriptionTemplate =
          searchBox
              .i18nAdvanced('searchFeedbackDescriptionTemplate', {
                substitutions: [searchQuery],
              })
              .toString();
      await browserProxy.methodCalled(
          'openSearchFeedbackDialog', descriptionTemplate);
    });

    test(
        'feedback button does not appear when searching for only whitespace',
        async () => {
          settingsSearchHandler.setFakeResults([fakeSettingsResult('')]);
          await simulateSearch('    ');
          const noSearchResult =
              searchBox.shadowRoot!.querySelector('#noSearchResultsContainer');
          assertTrue(!!noSearchResult);
          const feedbackReportResults =
              searchBox.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#reportSearchResultButton');
          assertTrue(!!feedbackReportResults);
          assertTrue(feedbackReportResults.hidden);
        });
  });
});
