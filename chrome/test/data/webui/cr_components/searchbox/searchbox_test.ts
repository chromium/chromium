// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {SearchboxElement, SearchboxIconElement, SearchboxMatchElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$, BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {NavigationPredictor} from 'chrome://resources/cr_components/searchbox/omnibox.mojom-webui.js';
import type {AutocompleteMatch} from 'chrome://resources/cr_components/searchbox/searchbox.mojom-webui.js';
import {RenderType, SideType} from 'chrome://resources/cr_components/searchbox/searchbox.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {mojoString16ToString, stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertStyle, createAutocompleteMatch} from './searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

enum Attributes {
  SELECTED = 'selected',
}

function createClipboardEvent(name: string): ClipboardEvent {
  return new ClipboardEvent(
      name, {cancelable: true, clipboardData: new DataTransfer()});
}

function createUrlMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  return Object.assign(
      createAutocompleteMatch(), {
        swapContentsAndDescription: true,
        contents: stringToMojoString16('helloworld.com'),
        contentsClass: [{offset: 0, style: 1}],
        destinationUrl: {url: 'https://helloworld.com/'},
        fillIntoEdit: stringToMojoString16('https://helloworld.com'),
        type: 'url-what-you-typed',
      },
      modifiers);
}

function createSearchMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  return Object.assign(
      createAutocompleteMatch(), {
        isSearchType: true,
        contents: stringToMojoString16('hello world'),
        contentsClass: [{offset: 0, style: 0}],
        description: stringToMojoString16('Google search'),
        descriptionClass: [{offset: 0, style: 4}],
        destinationUrl: {url: 'https://www.google.com/search?q=hello+world'},
        fillIntoEdit: stringToMojoString16('hello world'),
        type: 'search-what-you-typed',
      },
      modifiers);
}

function createCalculatorMatch(modifiers: Partial<AutocompleteMatch>):
    AutocompleteMatch {
  return Object.assign(
      createAutocompleteMatch(), {
        isSearchType: true,
        contents: stringToMojoString16('2 + 3'),
        contentsClass: [{offset: 0, style: 0}],
        description: stringToMojoString16('5'),
        descriptionClass: [{offset: 0, style: 0}],
        destinationUrl: {url: 'https://www.google.com/search?q=2+%2B+3'},
        fillIntoEdit: stringToMojoString16('5'),
        type: 'search-calculator-answer',
        iconUrl: 'calculator.svg',
      },
      modifiers);
}

/** Verifies the autocomplete match is showing. */
function verifyMatch(match: AutocompleteMatch, matchEl: SearchboxMatchElement) {
  assertEquals('option', matchEl.getAttribute('role'));
  const matchContents = mojoString16ToString(
      match.answer ? match.answer.firstLine : match.contents);
  const matchDescription = mojoString16ToString(
      match.answer ? match.answer.secondLine : match.description);
  const separatorText =
      matchDescription ? loadTimeData.getString('searchboxSeparator') : '';
  const contents = matchEl.$['contents'].textContent!.trim();
  const separator = matchEl.$['separator'].textContent!.trim();
  const description = matchEl.$['description'].textContent!.trim();
  const text = (contents + ' ' + separator + ' ' + description).trim();
  assertEquals(
      match.swapContentsAndDescription ?
          matchDescription + separatorText + matchContents :
          matchContents + separatorText + matchDescription,
      text);
}

suite('NewTabPageRealboxTest', () => {
  let realbox: SearchboxElement;

  let testProxy: TestSearchboxBrowserProxy;

  const testMetricsReporterProxy = TestMock.fromClass(BrowserProxyImpl);

  suiteSetup(() => {
    loadTimeData.overrideValues({
      searchboxSeparator: ' - ',
      searchboxVoiceSearch: true,
    });
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Set up Realbox's browser proxy.
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    // Set up MetricsReporter's browser proxy.
    testMetricsReporterProxy.reset();
    const metricsReporterCallbackRouter = new PageMetricsCallbackRouter();
    testMetricsReporterProxy.setResultFor(
        'getCallbackRouter', metricsReporterCallbackRouter);
    testMetricsReporterProxy.setResultFor('getMark', Promise.resolve(null));
    BrowserProxyImpl.setInstance(testMetricsReporterProxy);
    MetricsReporterImpl.setInstanceForTest(new MetricsReporterImpl());

    realbox = document.createElement('cr-searchbox');
    document.body.appendChild(realbox);
  });

  // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
  /*
    function assertFavicon(
        iconElement: SearchboxIconElement, destinationUrl: string) {
      assertStyle(
          iconElement.$.icon, 'background-image',

          // Resolution units are converted from `x` (shorthand for `dppx`) to
          // `dppx` (the canonical unit for the resolution type) because
          // assertStyle is using computed values instead of specified ones, and
          // the computed values have to return the canonical unit for the type.
          getFaviconForPageURL(destinationUrl, false, '', 16, true)
              .replace(' 1x', ' 1dppx')
              .replace(' 2x', ' 2dppx'));
      assertStyle(iconElement.$.icon, '-webkit-mask-image', 'none');
    }
  */

  function assertIconMaskImageUrl(
      iconElement: SearchboxIconElement, url: string) {
    assertStyle(
        iconElement.$.icon, '-webkit-mask-image',
        `url("chrome://new-tab-page/${url}")`);
    assertStyle(iconElement.$.icon, 'background-image', 'none');
  }

  async function areMatchesShowing(): Promise<boolean> {
    // Force a synchronous render.
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(realbox);
    return window.getComputedStyle(realbox.$.matches).display !== 'none';
  }

  test('when created is not focused and matches are not showing', async () => {
    assertEquals(0, testProxy.handler.getCallCount('onFocusChanged'));
    assertFalse(realbox.hidden);
    assertNotEquals(realbox, getDeepActiveElement());
    assertFalse(await areMatchesShowing());
  });

  test('Voice search button is present by default', async () => {
    // Arrange.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('cr-searchbox');
    document.body.appendChild(realbox);
    await waitAfterNextRender(realbox);

    // Assert
    const voiceSearchButton =
        realbox.shadowRoot!.querySelector<HTMLElement>('#voiceSearchButton');
    assertTrue(!!voiceSearchButton);
  });

  test('Voice search button is not present when not enabled', async () => {
    // Arrange.
    loadTimeData.overrideValues({searchboxVoiceSearch: false});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('cr-searchbox');
    document.body.appendChild(realbox);
    await waitAfterNextRender(realbox);

    // Assert
    const voiceSearchButton =
        realbox.shadowRoot!.querySelector<HTMLElement>('#voiceSearchButton');
    assertFalse(!!voiceSearchButton);

    // Restore
    loadTimeData.overrideValues({searchboxVoiceSearch: true});
  });

  test('clicking voice search button send voice search event', async () => {
    // Arrange.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('cr-searchbox');
    document.body.appendChild(realbox);
    await waitAfterNextRender(realbox);

    const whenOpenVoiceSearch = eventToPromise('open-voice-search', realbox);

    // Act.
    const voiceSearchButton =
        realbox.shadowRoot!.querySelector<HTMLElement>('#voiceSearchButton');
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();

    // Assert.
    await whenOpenVoiceSearch;
  });

  test('realbox default loupe icon', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      searchboxDefaultIcon: 'search.svg',
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('cr-searchbox');
    document.body.appendChild(realbox);

    // Assert.
    assertIconMaskImageUrl(realbox.$.icon, 'search.svg');
  });

  test('realbox default Google G icon', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      searchboxDefaultIcon:
          '//resources/cr_components/searchbox/icons/google_g.svg',
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('cr-searchbox');
    document.body.appendChild(realbox);

    // Assert.
    assertStyle(
        realbox.$.icon.$.icon, 'background-image',
        `url("chrome://resources/cr_components/searchbox/icons/google_g.svg")`);
    assertStyle(realbox.$.icon.$.icon, '-webkit-mask-image', 'none');

    // Restore.
    loadTimeData.overrideValues({
      searchboxDefaultIcon: 'search.svg',
    });
  });

  test('Color source baseline search icon has background image', async () => {
    // Arrange.
    loadTimeData.overrideValues({searchboxCr23Theming: true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('cr-searchbox');
    realbox.colorSourceIsBaseline = true;
    document.body.appendChild(realbox);
    await waitAfterNextRender(realbox);

    // Assert.
    const voiceSearchButton =
        realbox.shadowRoot!.querySelector<HTMLElement>('#voiceSearchButton');
    assertTrue(!!voiceSearchButton);
    assertStyle(
        voiceSearchButton, 'background-image',
        'url("chrome://resources/cr_components/searchbox/icons/mic.svg")');

    // Restore.
    loadTimeData.overrideValues({searchboxCr23Theming: false});
  });

  test('Color source not baseline search icon has mask image', async () => {
    // Arrange.
    loadTimeData.overrideValues({searchboxCr23Theming: true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('cr-searchbox');
    realbox.colorSourceIsBaseline = false;
    document.body.appendChild(realbox);
    await waitAfterNextRender(realbox);

    // Assert.
    const voiceSearchButton =
        realbox.shadowRoot!.querySelector<HTMLElement>('#voiceSearchButton');
    assertTrue(!!voiceSearchButton);
    assertStyle(
        voiceSearchButton, '-webkit-mask-image',
        'url("chrome://resources/cr_components/searchbox/icons/mic.svg")');

    // Restore.
    loadTimeData.overrideValues({searchboxCr23Theming: false});
  });

  //============================================================================
  // Test Querying Autocomplete
  //============================================================================

  test('left-clicking the input queries autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.value = '';
    // Left click queries autocomplete when matches are not showing.
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));

    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(''),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Left click does not query autocomplete when matches are showing.
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Right click does not query autocomplete.
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 1}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(2, testProxy.handler.getCallCount('onFocusChanged'));

    // Left click queries autocomplete when input is non-empty.
    realbox.$.input.value = '   ';
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('focusing the input does not query autocomplete', async () => {
    assertEquals(0, testProxy.handler.getCallCount('onFocusChanged'));
    realbox.$.input.value = '';
    realbox.$.input.focus();
    assertEquals(realbox.$.input, getDeepActiveElement());
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));
  });

  test('tabbing into empty input queries autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(''),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Tabbing into input does not query autocomplete when matches are
    // showing.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      key: 'Tab',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Tabbing into empty input queries autocomplete.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      key: 'Tab',
    }));
    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Tabbing into non-empty input does not query autocomplete.
    realbox.$.input.value = '   ';
    realbox.$.input.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      key: 'Tab',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('arrow up/down keys in empty input query autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(''),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Arrow up/down keys do not query autocomplete when matches are showing.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowUp',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Arrow up/down keys query autocomplete.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowDown',
    }));
    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('arrow up/down keys in non-empty input query autocomplete', async () => {
    // Query matches.
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show matches.
    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16('hello'),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Arrow up/down keys do not query autocomplete when matches are showing.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowUp',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Arrow up/down keys query autocomplete.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowDown',
    }));
    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('empty input does not query autocomplete', async () => {
    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('typing space does not query autocomplete', async () => {
    realbox.$.input.value = ' ';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('typing queries autocomplete', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Deleting text from input prevents inline autocompletion.
    realbox.$.input.value = 'h';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Pasting text into the input prevents inline autocompletion.
    const pasteEvent = createClipboardEvent('paste');
    realbox.$.input.dispatchEvent(pasteEvent);
    assertFalse(pasteEvent.defaultPrevented);
    realbox.$.input.value = 'hel';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    realbox.$.input.value = 'hell';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // If caret isn't at the end of the text inline autocompletion is prevented.
    realbox.$.input.value = 'hello';
    realbox.$.input.setSelectionRange(0, 0);  // Move caret to beginning.
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // If text is being composed with an IME inline autocompletion is prevented.
    realbox.$.input.value = 'hello 간';
    const inputEvent = new InputEvent('input', {isComposing: true});
    realbox.$.input.dispatchEvent(inputEvent);

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();
  });

  test('clearing the input stops autocomplete', async () => {
    realbox.$.input.value = 'h';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    args = await testProxy.handler.whenCalled('stopAutocomplete');
    assertTrue(args.clearResult);
  });

  test(
      'autocomplete triggers on focus on non-empty input with thumbnail',
      async () => {
        testProxy.callbackRouterRemote.setThumbnail('foo.png');
        await waitAfterNextRender(realbox);
        const thumbnail = realbox.$.inputWrapper.querySelector('#thumbnail');
        assertTrue(thumbnail !== null);
        realbox.$.input.value = 'hi';
        realbox.$.input.dispatchEvent(new InputEvent('input'));
        // Make sure realbox is not focused and matches aren't showing.
        realbox.$.input.blur();
        assertFalse(await areMatchesShowing());

        // Click on realbox.
        realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));

        // Check that autocomplete gets queried with last input on click with
        // non empty input when thumbnail is showing.
        let args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(mojoString16ToString(args.input), realbox.$.input.value);

        // Make sure realbox focus is not focused and matches aren't showing.
        realbox.$.input.blur();
        assertFalse(await areMatchesShowing());

        // Tabbing into realbox.
        realbox.$.input.dispatchEvent(new KeyboardEvent('keyup', {
          bubbles: true,
          cancelable: true,
          key: 'Tab',
        }));

        // Check that autocomplete gets queried with last input on keyup with
        // non empty input when thumbnail is showing.
        args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
      });

  //============================================================================
  // Test Autocomplete Response
  //============================================================================

  test('autocomplete response', async () => {
    realbox.$.input.value = '      hello world';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    assertEquals('listbox', realbox.$.matches.getAttribute('role'));
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);
    verifyMatch(matches[0]!, matchEls[0]!);
    verifyMatch(matches[1]!, matchEls[1]!);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    assertEquals('      hello world', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('', realbox.$.input.value.substring(start, end));
  });

  test('autocomplete response with inline autocompletion', async () => {
    realbox.$.input.value = 'hello ';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), realbox.$.input.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    const matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: stringToMojoString16('world'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(1, matchEls.length);
    verifyMatch(matches[0]!, matchEls[0]!);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    assertEquals('hello world', realbox.$.input.value);
    let start = realbox.$.input.selectionStart!;
    let end = realbox.$.input.selectionEnd!;
    assertEquals('world', realbox.$.input.value.substring(start, end));

    // Define a new |value| property on the input to see whether it gets set.
    let inputValueChanged = false;
    const originalValueProperty =
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value')!;
    Object.defineProperty(realbox.$.input, 'value', {
      get: originalValueProperty.get,
      set: (value) => {
        inputValueChanged = true;
        originalValueProperty.set!.call(realbox.$.input, value);
      },
    });

    // If the user types the next character of the inline autocompletion, the
    // keydown event is stopped, inline autocompletion is moved forward and
    // autocomplete is queried with the non inline autocompletion part of input.
    const keyEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'w',
    });
    realbox.$.input.dispatchEvent(keyEvent);
    assertTrue(keyEvent.defaultPrevented);

    assertFalse(inputValueChanged);
    assertEquals('hello world', realbox.$.input.value);
    start = realbox.$.input.selectionStart!;
    end = realbox.$.input.selectionEnd!;
    assertEquals('orld', realbox.$.input.value.substring(start, end));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(mojoString16ToString(args.input), 'hello w');
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('autocomplete response perserves cursor position', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.selectionStart = 0;
    realbox.$.input.selectionEnd = 4;
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      contents: stringToMojoString16('hello'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    assertEquals('hello', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('hell', realbox.$.input.value.substring(start, end));
  });

  test('stale autocomplete response is ignored', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16('h'),  // Simulate stale response.
      matches,
      suggestionGroupsMap: {},
    });
    assertFalse(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(0, matchEls.length);
  });

  test('autocomplete response changes', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    let matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    realbox.$.input.value += 'll';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches: [],
      suggestionGroupsMap: {},
    });
    assertFalse(await areMatchesShowing());

    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(0, matchEls.length);

    realbox.$.input.value += 'o';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);
  });

  //============================================================================
  // Test Cut/Copy
  //============================================================================

  test('Copying or cutting empty input fails', async () => {
    realbox.$.input.value = '';

    const copyEvent = createClipboardEvent('copy');
    realbox.$.input.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    realbox.$.input.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);
  });

  test('Copying or cutting search match fails', async () => {
    realbox.$.input.value = 'hello ';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: stringToMojoString16('world'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    assertEquals('hello world', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('world', realbox.$.input.value.substring(start, end));

    // Select the entire input.
    realbox.$.input.setSelectionRange(0, realbox.$.input.value.length);

    const copyEvent = createClipboardEvent('copy');
    realbox.$.input.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    realbox.$.input.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);
  });

  test('Copying or cutting URL match succeeds', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createUrlMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: stringToMojoString16('world.com'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    assertEquals('helloworld.com', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('world.com', realbox.$.input.value.substring(start, end));

    const copyEvent = createClipboardEvent('copy');
    realbox.$.input.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    realbox.$.input.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);

    // Select the entire input.
    realbox.$.input.setSelectionRange(0, realbox.$.input.value.length);

    realbox.$.input.dispatchEvent(copyEvent);
    assertTrue(copyEvent.defaultPrevented);
    assertEquals(
        'https://helloworld.com/',
        copyEvent.clipboardData!.getData('text/plain'));

    realbox.$.input.dispatchEvent(cutEvent);
    assertTrue(cutEvent.defaultPrevented);
    assertEquals(
        'https://helloworld.com/',
        cutEvent.clipboardData!.getData('text/plain'));

    // Cut should close the dropdown.
    assertFalse(await areMatchesShowing());
  });

  //============================================================================
  // Test Navigation
  //============================================================================

  test('pressing Enter on input navigates to the selected match', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = 'hello ';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
        inlineAutocompletion: stringToMojoString16('world'),
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // Before navigation, input should be inline autocompleted.
    assertEquals('hello world', realbox.$.input.value);
    let start = realbox.$.input.selectionStart!;
    let end = realbox.$.input.selectionEnd!;
    assertEquals('world', realbox.$.input.value.substring(start, end));

    // Pressing enter...
    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Navigates to the first match.
    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl.url, args.url.url);
    assertTrue(args.areMatchesShowing);
    assertTrue(args.shiftKey);
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));

    // Navigation should close the dropdown.
    assertFalse(await areMatchesShowing());

    // After navigation, the inline autocompletion should be applied to the text
    // shown in the input and there should be no visible selection.
    assertEquals('hello world', realbox.$.input.value);
    start = realbox.$.input.selectionStart!;
    end = realbox.$.input.selectionEnd!;
    assertEquals('', realbox.$.input.value.substring(start, end));
  });

  test(
      'pressing Enter on input navigates to *hidden* selected match',
      async () => {
        // Input is expected to have been focused before any navigation.
        realbox.$.input.dispatchEvent(new Event('focus'));

        realbox.$.input.value = '  hello  ';
        realbox.$.input.dispatchEvent(new InputEvent('input'));

        const matches =
            [createSearchMatch({iconUrl: 'clock.svg'}), createUrlMatch()];
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: stringToMojoString16(realbox.$.input.value.trimStart()),
          matches,
          suggestionGroupsMap: {},
        });
        assertTrue(await areMatchesShowing());

        let matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
        assertEquals(2, matchEls.length);

        // Select the first match.
        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');

        // Hide the matches by focusing out.
        matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          relatedTarget: document.body,
        }));

        // Matches are hidden.
        assertFalse(await areMatchesShowing());

        // First match is still selected.
        matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
        assertEquals(2, matchEls.length);
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is not cleared.
        assertEquals('hello world', realbox.$.input.value);
        // Icon is not cleared.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');

        const shiftEnter = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          key: 'Enter',
          composed: true,  // So it propagates across shadow DOM boundary.
          shiftKey: true,
        });
        realbox.$.input.dispatchEvent(shiftEnter);
        assertTrue(shiftEnter.defaultPrevented);

        // Navigates to the first match.
        const args =
            await testProxy.handler.whenCalled('openAutocompleteMatch');
        assertEquals(0, args.line);
        assertEquals(matches[0]!.destinationUrl.url, args.url.url);
        assertFalse(args.areMatchesShowing);
        assertTrue(args.shiftKey);
        assertEquals(
            1, testProxy.handler.getCallCount('openAutocompleteMatch'));
      });

  test('pressing Enter on input is ignored if no selected match', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftEnter);
    assertFalse(shiftEnter.defaultPrevented);

    // Did not navigate to the first match since it's not selected.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test(
      'pressing Enter on input is ignored if no *hidden* selected match',
      async () => {
        realbox.$.input.value = '';
        realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));

        const matches =
            [createSearchMatch({iconUrl: 'clock.svg'}), createUrlMatch()];
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: stringToMojoString16(realbox.$.input.value.trimStart()),
          matches,
          suggestionGroupsMap: {},
        });
        assertTrue(await areMatchesShowing());

        let matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
        assertEquals(2, matchEls.length);

        // Select the first match.
        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');

        // Hide the matches by focusing out.
        matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          relatedTarget: document.body,
        }));

        // Matches are hidden.
        assertFalse(await areMatchesShowing());

        // Matches are cleared.
        matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
        assertEquals(0, matchEls.length);
        // Input is cleared (zero-prefix case).
        assertEquals('', realbox.$.input.value);
        // Icon is restored (zero-prefix case).
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');

        const shiftEnter = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Enter',
          shiftKey: true,
        });
        realbox.$.input.dispatchEvent(shiftEnter);
        assertFalse(shiftEnter.defaultPrevented);

        // Did not navigate to the first match since it's not selected.
        assertEquals(
            0, testProxy.handler.getCallCount('openAutocompleteMatch'));
      });

  test('pressing Enter on input too quickly', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // User types some more and presses Enter before the results update.
    realbox.$.input.value = 'hello world';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('queryAutocomplete');

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Did not navigate to the first match since it's stale.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));

    // New matches arrive.
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    // As soon as the new matches arrive, the pending enter triggers a
    // navigation, which closes the dropdown.
    assertFalse(await areMatchesShowing());

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // Navigates to the first match immediately without further user action.
    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl.url, args.url.url);
    assertTrue(args.areMatchesShowing);
    assertTrue(args.shiftKey);
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('pressing Enter on the selected match navigates to it', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    matchEls[0]!.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Navigates to the first match is selected.
    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl.url, args.url.url);
    assertTrue(args.areMatchesShowing);
    assertTrue(args.shiftKey);
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  //============================================================================
  // Test Deletion
  //============================================================================

  test('Remove button is visible if the match supports deletion', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches =
        [createSearchMatch(), createUrlMatch({supportsDeletion: true})];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.remove).display, 'none');

    // Match must be focused/selected for remove button to be shown/
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));
    assertNotEquals(
        window.getComputedStyle(matchEls[1]!.$.remove).display, 'none');
  });

  test('Can remove selected match using keyboard shortcut', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch({supportsDeletion: true}),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);
    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // First match does not support deletion.
    const deleteEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Delete',
    });
    realbox.$.input.dispatchEvent(deleteEvent);
    assertFalse(deleteEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    // Second match is selected.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));

    // Unmodified 'Delete' key does not delete matches.
    realbox.$.input.dispatchEvent(deleteEvent);
    assertFalse(deleteEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    const shiftDeleteEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Delete',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftDeleteEvent);
    assertTrue(shiftDeleteEvent.defaultPrevented);
    const args = await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(1, args.line);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
  });

  test('Selection is restored after selected match is removed', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    let matches = [
      createSearchMatch({
        supportsDeletion: true,
      }),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    let matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(1, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // Remove the first match.
    matchEls[0]!.$.remove.click();
    let args = await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    testProxy.handler.reset();

    matches = [createUrlMatch({supportsDeletion: true})];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16('hello'),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(1, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.value);

    // Remove the first match.
    const shiftDeleteEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Delete',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftDeleteEvent);
    assertTrue(shiftDeleteEvent.defaultPrevented);
    args = await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    matches = [createSearchMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16('hello'),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(1, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
  });

  //============================================================================
  // Test Selection
  //============================================================================

  test('pressing Escape selects the first match / hides matches', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    let matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Select the second match.
    matchEls[1]!.focus();
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.value);
    assertEquals(matchEls[1], realbox.$.matches.shadowRoot!.activeElement);

    let escapeEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Escape',
    });
    realbox.$.input.dispatchEvent(escapeEvent);
    assertTrue(escapeEvent.defaultPrevented);

    // First match gets selected and also gets the focus.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
    assertEquals(matchEls[0], realbox.$.matches.shadowRoot!.activeElement);

    escapeEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Escape',
    });
    realbox.$.input.dispatchEvent(escapeEvent);
    assertTrue(escapeEvent.defaultPrevented);

    // Matches are hidden.
    assertFalse(await areMatchesShowing());

    // Matches are cleared.
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(0, matchEls.length);
    // Input is cleared.
    assertEquals('', realbox.$.input.value);

    // Show zero-prefix matches.
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(''),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Pressing 'Escape' when no matches are selected closes the dropdown.
    escapeEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Escape',
    });
    realbox.$.input.dispatchEvent(escapeEvent);
    assertTrue(escapeEvent.defaultPrevented);

    // Matches are hidden.
    assertFalse(await areMatchesShowing());

    // Matches are cleared.
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(0, matchEls.length);
  });

  test('arrow up/down moves selection / focus', async () => {
    realbox.$.input.focus();
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(2, matchEls.length);

    let arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    // First match is selected but does not get focus while focus is in the
    // input.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
    assertEquals(realbox.$.input, realbox.shadowRoot!.activeElement);

    // If text is being composed with an IME composition selection is prevented.
    arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      isComposing: true,
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertFalse(arrowDownEvent.defaultPrevented);

    // First match remains selected and does not get focus while focus is in the
    // input.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
    assertEquals(realbox.$.input, realbox.shadowRoot!.activeElement);

    arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    // Second match gets selected but does not get focus while focus is in the
    // input.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.value);
    assertEquals(realbox.$.input, realbox.shadowRoot!.activeElement);

    // Move the focus to the second match.
    matchEls[1]!.focus();
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));

    // Second match is selected and has focus.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.value);
    assertEquals(matchEls[1], realbox.$.matches.shadowRoot!.activeElement);

    const arrowUpEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowUp',
    });
    matchEls[1]!.dispatchEvent(arrowUpEvent);
    assertTrue(arrowUpEvent.defaultPrevented);

    // First match gets selected and gets focus while focus is in the matches.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
    assertEquals(matchEls[0], realbox.$.matches.shadowRoot!.activeElement);

    // Changing match selection doesn't result in another onFocusChanged call
    // because focus is for the whole realbox (including input container).
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));
  });

  test('focus indicator', async () => {
    realbox.$.input.focus();
    realbox.$.input.value = 'clear browsing history';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch({
      actions: [{
        a11yLabel: stringToMojoString16(''),
        hint: stringToMojoString16('Clear Browsing History'),
        suggestionContents: stringToMojoString16(''),
        iconUrl: 'chrome://theme/current-channel-logo',
      }],
      fillIntoEdit: stringToMojoString16('clear browsing history'),
      supportsDeletion: true,
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');

    const focusIndicator = matchEls[0]!.$['focus-indicator'];

    // Select the first match
    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('clear browsing history', realbox.$.input.value);
    assertTrue(isVisible(focusIndicator));

    // Give focus to the action button
    const action = $$<HTMLElement>(matchEls[0]!, '#action')!;
    action.focus();

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals(action, matchEls[0]!.shadowRoot!.activeElement);
    assertFalse(isVisible(focusIndicator));

    // Give focus to remove button
    const removeButton = matchEls[0]!.$.remove;
    removeButton.focus();

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals(removeButton, matchEls[0]!.shadowRoot!.activeElement);
    assertFalse(isVisible(focusIndicator));
  });

  //============================================================================
  // Test Responsiveness Metrics
  //============================================================================

  test('responsiveness metrics are being recorded', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    // The responsiveness metrics are not recorded until the results are
    // painted.
    assertEquals(0, testMetricsReporterProxy.getCallCount('umaReportTime'));

    let matches = [createSearchMatch()];
    MetricsReporterImpl.getInstance().mark('ResultChanged');  // Marked in C++.
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    // The responsiveness metrics are recorded once the results are painted.
    await testMetricsReporterProxy.whenCalled('umaReportTime');
    assertEquals(2, testMetricsReporterProxy.getCallCount('umaReportTime'));
    await testMetricsReporterProxy.whenCalled('clearMark');

    // Delete the last character.
    realbox.$.input.value = 'h';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: stringToMojoString16('ello'),
    })];
    MetricsReporterImpl.getInstance().mark('ResultChanged');  // Marked in C++.
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    // Only one responsiveness metric is recorded when characters are deleted.
    await testMetricsReporterProxy.whenCalled('umaReportTime');
    assertEquals(3, testMetricsReporterProxy.getCallCount('umaReportTime'));
    await testMetricsReporterProxy.whenCalled('clearMark');

    assertEquals('hello', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('ello', realbox.$.input.value.substring(start, end));

    // Type the next character of the inline autocompletion.
    const keyEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'e',
    });
    realbox.$.input.dispatchEvent(keyEvent);
    assertTrue(keyEvent.defaultPrevented);

    matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: stringToMojoString16('llo'),
    })];
    MetricsReporterImpl.getInstance().mark('ResultChanged');  // Marked in C++.
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16('he'),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    // The responsiveness metrics are recorded when the default match has
    // inline autocompletion.
    await testMetricsReporterProxy.whenCalled('umaReportTime');
    assertEquals(5, testMetricsReporterProxy.getCallCount('umaReportTime'));
    await testMetricsReporterProxy.whenCalled('clearMark');
  });

  //============================================================================
  // Test favicons / entity images
  //============================================================================

  test(
      'match and realbox icons are updated when favicon becomes available',
      async () => {
        realbox.$.input.value = 'hello';
        realbox.$.input.dispatchEvent(new InputEvent('input'));

        const matches = [
          createSearchMatch({iconUrl: 'clock.svg'}),
          createUrlMatch({iconUrl: 'page.svg'}),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: stringToMojoString16(realbox.$.input.value.trimStart()),
          matches,
          suggestionGroupsMap: {},
        });
        assertTrue(await areMatchesShowing());

        const matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
        assertEquals(2, matchEls.length);
        assertIconMaskImageUrl(matchEls[0]!.$.icon, 'clock.svg');
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(matchEls[1]!.$.icon, matches[1]!.destinationUrl.url);
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.

        // Select the first match.
        let arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });
        realbox.$.input.dispatchEvent(arrowDownEvent);
        assertTrue(arrowDownEvent.defaultPrevented);

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');

        // Select the second match.
        arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });
        realbox.$.input.dispatchEvent(arrowDownEvent);
        assertTrue(arrowDownEvent.defaultPrevented);

        // Second match is selected.
        assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('https://helloworld.com', realbox.$.input.value);
        // Realbox icon is updated.
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(realbox.$.icon, matches[1]!.destinationUrl.url);

        // Select the first match by pressing 'Escape'.
        const escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');
      });

  test(
      'realbox icons is updated when url match is cut from realbox',
      async () => {
        realbox.$.input.value = 'www.test.com';
        realbox.$.input.dispatchEvent(new InputEvent('input'));

        const matches = [createUrlMatch(
            {allowedToBeDefaultMatch: true, iconUrl: 'page.svg'})];

        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: stringToMojoString16(realbox.$.input.value.trimStart()),
          matches,
          suggestionGroupsMap: {},
        });
        assertTrue(await areMatchesShowing());

        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(realbox.$.icon, matches[0]!.destinationUrl.url);
        // Select the entire input.
        realbox.$.input.setSelectionRange(0, realbox.$.input.value.length);

        const cutEvent = createClipboardEvent('cut');
        realbox.$.input.dispatchEvent(cutEvent);
        assertTrue(cutEvent.defaultPrevented);

        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');
      });

  test(
      'match icons are updated when entity images become available',
      async () => {
        realbox.$.input.value = 'hello';
        realbox.$.input.dispatchEvent(new InputEvent('input'));

        const matches = [
          createUrlMatch({iconUrl: 'page.svg'}),
          createSearchMatch({
            iconUrl: 'clock.svg',
            imageUrl: 'https://gstatic.com/',
            imageDominantColor: '#757575',
            isRichSuggestion: true,
          }),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: stringToMojoString16(realbox.$.input.value.trimStart()),
          matches,
          suggestionGroupsMap: {},
        });
        assertTrue(await areMatchesShowing());

        const matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
        assertEquals(2, matchEls.length);
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(matchEls[0]!.$.icon, matches[0]!.destinationUrl.url);
        assertIconMaskImageUrl(matchEls[1]!.$.icon, 'clock.svg');
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.

        // Select the first match.
        let arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });
        realbox.$.input.dispatchEvent(arrowDownEvent);
        assertTrue(arrowDownEvent.defaultPrevented);

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('https://helloworld.com', realbox.$.input.value);
        // Realbox icon is updated.
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(matchEls[0]!.$.icon, matches[0]!.destinationUrl.url);

        // Select the second match.
        arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });
        realbox.$.input.dispatchEvent(arrowDownEvent);
        assertTrue(arrowDownEvent.defaultPrevented);

        // Second match is selected.
        assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Second match shows a placeholder color until the image loads.
        const containerEl = matchEls[1]!.$.icon.$.container;
        assertStyle(
            containerEl, 'background-color', 'rgba(117, 117, 117, 0.25)');
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.

        assertEquals(
            matchEls[1]!.$.icon.$.image.getAttribute('src'),
            `//image?staticEncode=true&encodeType=webp&url=${
                matches[1]!.imageUrl}`);

        // Mock image finishing loading, which should remove the temporary
        // background color.
        matchEls[1]!.$.icon.$.image.dispatchEvent(new Event('load'));
        assertStyle(containerEl, 'background-color', 'rgba(0, 0, 0, 0)');
        // Realbox icon is not updated as the input does not feature images.
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.
        assertTrue(window.getComputedStyle(realbox.$.icon).display !== 'none');

        // Select the first match by pressing 'Escape'.
        const escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('https://helloworld.com', realbox.$.input.value);
        // Realbox icon is updated.
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(realbox.$.icon, matches[0]!.destinationUrl.url);
      });

  //============================================================================
  // Test suggestion groups
  //============================================================================

  test('matches in a suggestion group can be made hidden/visible', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches =
        [createSearchMatch(), createUrlMatch({suggestionGroupId: 100})];
    const suggestionGroupsMap = {
      100: {
        header: stringToMojoString16('Recommended for you'),
        hideGroupA11yLabel: stringToMojoString16(''),
        showGroupA11yLabel: stringToMojoString16(''),
        hidden: true,
        renderType: RenderType.kDefaultVertical,
        sideType: SideType.kDefaultPrimary,
      },
      101: {
        header: stringToMojoString16('Not recommended for you'),
        hideGroupA11yLabel: stringToMojoString16(''),
        showGroupA11yLabel: stringToMojoString16(''),
        hidden: false,
        renderType: RenderType.kDefaultVertical,
        sideType: SideType.kDefaultPrimary,
      },
    };
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap,
    });
    assertTrue(await areMatchesShowing());

    // The first match is showing. The second match is initially hidden.
    let matchEls = realbox.$.matches.selectableMatchElements;
    assertEquals(1, matchEls.length);

    // The suggestion group header and the toggle button are visible.
    const headerEl =
        realbox.$.matches.shadowRoot!.querySelectorAll<HTMLElement>(
            '.header')[0]!;
    assertTrue(window.getComputedStyle(headerEl).display !== 'none');
    assertEquals('Recommended for you', headerEl.textContent!.trim());
    const toggleButtonEl =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-icon-button')[0]!;
    assertTrue(window.getComputedStyle(toggleButtonEl).display !== 'none');

    // Make the second match visible by pressing 'Space' on the toggle button.
    toggleButtonEl.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: ' ',
    }));
    toggleButtonEl.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: ' ',
    }));

    let args =
        await testProxy.handler.whenCalled('toggleSuggestionGroupIdVisibility');
    assertEquals(100, args.suggestionGroupId);
    assertEquals(
        1, testProxy.handler.getCallCount('toggleSuggestionGroupIdVisibility'));

    testProxy.handler.reset();

    // Second match is visible.
    matchEls = realbox.$.matches.selectableMatchElements;
    assertEquals(2, matchEls.length);

    // Hide the second match by clicking the toggle button.
    toggleButtonEl.click();

    args =
        await testProxy.handler.whenCalled('toggleSuggestionGroupIdVisibility');
    assertEquals(100, args.suggestionGroupId);
    assertEquals(
        1, testProxy.handler.getCallCount('toggleSuggestionGroupIdVisibility'));

    // Second match is hidden.
    matchEls = realbox.$.matches.selectableMatchElements;
    assertEquals(1, matchEls.length);

    testProxy.handler.reset();

    // Show the second match by clicking the header.
    headerEl.click();
    args =
        await testProxy.handler.whenCalled('toggleSuggestionGroupIdVisibility');
    assertEquals(100, args.suggestionGroupId);
    assertEquals(
        1, testProxy.handler.getCallCount('toggleSuggestionGroupIdVisibility'));
    // Second match is visible again.
    matchEls = realbox.$.matches.selectableMatchElements;
    assertEquals(2, matchEls.length);
  });

  test('HidesDropdownIfNoPrimaryMatches', async () => {
    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));

    const matches = [createUrlMatch({suggestionGroupId: 100})];
    const suggestionGroupsMap = {
      100: {
        header: stringToMojoString16('People also search for'),
        hideGroupA11yLabel: stringToMojoString16(''),
        showGroupA11yLabel: stringToMojoString16(''),
        hidden: false,
        renderType: RenderType.kDefaultVertical,
        sideType: SideType.kSecondary,
      },
    };
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(''),
      matches,
      suggestionGroupsMap,
    });
    assertFalse(await areMatchesShowing());

    // Verify updating the suggestion group to be a primary group makes the
    // realbox dropdown show.
    suggestionGroupsMap[100].sideType = SideType.kDefaultPrimary;
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(''),
      matches,
      suggestionGroupsMap,
    });
    assertTrue(await areMatchesShowing());
  });

  test(
      'focusing suggestion group header resets selection and input text',
      async () => {
        realbox.$.input.value = '';
        realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));

        const matches =
            [createSearchMatch(), createUrlMatch({suggestionGroupId: 100})];
        const suggestionGroupsMap = {
          100: {
            header: stringToMojoString16('Recommended for you'),
            hideGroupA11yLabel: stringToMojoString16(''),
            showGroupA11yLabel: stringToMojoString16(''),
            hidden: false,
            renderType: RenderType.kDefaultVertical,
            sideType: SideType.kDefaultPrimary,
          },
        };
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: stringToMojoString16(realbox.$.input.value.trimStart()),
          matches,
          suggestionGroupsMap,
        });
        assertTrue(await areMatchesShowing());

        const matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
        assertEquals(2, matchEls.length);

        // Select the first match.
        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);

        // Focus the suggestion group header.
        const headerEl =
            realbox.$.matches.shadowRoot!.querySelectorAll<HTMLElement>(
                '.header')[0]!;
        headerEl.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));

        // First match is no longer selected.
        assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is cleared.
        assertEquals('', realbox.$.input.value);
      });

  //============================================================================
  // Test calculator answer type
  //============================================================================

  test('match calculator answer type', async () => {
    const matches = [createCalculatorMatch({isRichSuggestion: true})];

    realbox.$.input.value = '2 + 3';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    assertEquals(1, matchEls.length);

    verifyMatch(matches[0]!, matchEls[0]!);
    assertIconMaskImageUrl(matchEls[0]!.$.icon, 'calculator.svg');
    assertIconMaskImageUrl(realbox.$.icon, 'search.svg');

    // Separator is not displayed
    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.separator).display, 'none');

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('5', realbox.$.input.value);

    assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default Icon
  });

  //============================================================================
  // Test suggestion answer
  //============================================================================

  test('Test Rich Suggestion Answer for Verbatim Question', async () => {
    realbox.$.input.value = 'When is Christmas Day';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    const matches = [createSearchMatch({
      answer: {
        firstLine: stringToMojoString16('When is Christmas Day'),
        secondLine: stringToMojoString16('Saturday, December 25, 2021'),
      },
      isRichSuggestion: true,
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    verifyMatch(matches[0]!, matchEls[0]!);

    // Separator is not displayed
    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.separator).display, 'none');

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default Icon
  });

  //============================================================================
  // Test pedals
  //============================================================================

  test('Test Actions for Verbatim Query', async () => {
    realbox.$.input.value = 'Clear Browsing History';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    const matches = [createSearchMatch({
      actions: [{
        a11yLabel: stringToMojoString16(''),
        hint: stringToMojoString16('Clear Browsing History'),
        suggestionContents: stringToMojoString16(''),
        iconUrl: 'chrome://theme/current-channel-logo',
      }],
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEl = $$(realbox.$.matches, 'cr-searchbox-match')!;
    verifyMatch(matches[0]!, matchEl);

    const pedalEl = $$($$(matchEl, 'cr-searchbox-action')!, '.contents')!;

    const leftClick = new MouseEvent('click', {
      bubbles: true,
      button: 1,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      ctrlKey: true,
    });

    pedalEl.dispatchEvent(leftClick);
    assertTrue(leftClick.defaultPrevented);

    const args = await testProxy.handler.whenCalled('executeAction');
    assertTrue(args.ctrlKey);
    assertEquals(0, args.line);
    assertEquals(args.mouseButton, 1);
    assertTrue(args.matchSelectionTimestamp['internalValue'] > 0);
    assertEquals(1, testProxy.handler.getCallCount('executeAction'));
  });

  test('Test Actions for Autocomplete Query', async () => {
    realbox.$.input.value = 'Clear Bro';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    const matches = [
      createSearchMatch({contents: stringToMojoString16('Clear Bro')}),
      createSearchMatch({
        actions: [
          {
            a11yLabel: stringToMojoString16(''),
            hint: stringToMojoString16('Clear Browsing History'),
            suggestionContents: stringToMojoString16(''),
            iconUrl: 'chrome://theme/current-channel-logo',
          },
          {
            a11yLabel: stringToMojoString16(''),
            hint: stringToMojoString16('Tab Switch'),
            suggestionContents: stringToMojoString16(''),
            iconUrl: 'chrome://theme/current-channel-logo',
          },
        ],
      }),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-searchbox-match');
    verifyMatch(matches[0]!, matchEls[0]!);
    verifyMatch(matches[1]!, matchEls[1]!);

    const pedalElClear =
        $$($$(matchEls[1]!, 'cr-searchbox-action')!, '.contents')!;

    const leftClick = new MouseEvent('click', {
      bubbles: true,
      button: 0,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    });

    pedalElClear.dispatchEvent(leftClick);
    assertTrue(leftClick.defaultPrevented);

    let args = await testProxy.handler.whenCalled('executeAction');
    assertEquals(1, args.line);
    assertEquals(args.mouseButton, 0);
    assertTrue(args.matchSelectionTimestamp['internalValue'] > 0);
    assertEquals(1, testProxy.handler.getCallCount('executeAction'));

    const pedalElTab =
        $$(matchEls[1]!.shadowRoot!.querySelectorAll('cr-searchbox-action')[1]!,
           '.contents')!;

    pedalElTab.dispatchEvent(leftClick);
    assertTrue(leftClick.defaultPrevented);

    args = await testProxy.handler.whenCalled('executeAction');
    assertEquals(1, args.line);
    assertEquals(args.mouseButton, 0);
    assertTrue(args.matchSelectionTimestamp['internalValue'] > 0);
    assertEquals(2, testProxy.handler.getCallCount('executeAction'));
  });

  //============================================================================
  // Test Forwarding Events
  //============================================================================

  test('arrow events are sent to handler', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: stringToMojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    assertTrue(await areMatchesShowing());

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);

    const args = await testProxy.handler.whenCalled('onNavigationLikely');
    assertEquals(0, args.line);
    assertEquals(
        NavigationPredictor.kUpOrDownArrowButton, args.navigationPredictor);
  });

  //============================================================================
  // Test Set Input Text
  //============================================================================
  test('input text appears on page call from browser', async () => {
    assertEquals(realbox.$.input.value, '');
    testProxy.callbackRouterRemote.setInputText('Hello');
    await waitAfterNextRender(realbox);
    assertEquals(realbox.$.input.value, 'Hello');
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  //============================================================================
  // Test Thumbnails
  //============================================================================
  test('thumbnail appears on page call from browser', async () => {
    assertTrue(
        realbox.$.inputWrapper.querySelector('#thumbnailContainer') === null);
    testProxy.callbackRouterRemote.setThumbnail('foo.png');
    await waitAfterNextRender(realbox);
    const thumbnailContainer =
        realbox.$.inputWrapper.querySelector('#thumbnailContainer');
    assertTrue(thumbnailContainer !== null);
    assertTrue(isVisible(thumbnailContainer));
  });

  test('thumbnail clicked deletion', async () => {
    testProxy.callbackRouterRemote.setThumbnail('foo.png');
    await waitAfterNextRender(realbox);
    const thumbnail = realbox.$.inputWrapper.querySelector('#thumbnail');
    assertTrue(thumbnail !== null);
    const thumbnailRemoveButton =
        thumbnail.shadowRoot!.querySelector<HTMLElement>('#remove');
    assertTrue(thumbnailRemoveButton !== null);
    thumbnailRemoveButton.click();
    await waitAfterNextRender(realbox);
    const thumbnailContainer =
        realbox.$.inputWrapper.querySelector<HTMLElement>(
            '#thumbnailContainer');
    assertTrue(thumbnailContainer !== null);
    // Thumbnail remove button click should remove thumbnail, focus input,
    // and notify browser.
    assertStyle(thumbnailContainer, 'display', 'none');
    assertEquals(realbox.$.input, getDeepActiveElement());
    await testProxy.handler.whenCalled('onThumbnailRemoved');
    assertEquals(1, testProxy.handler.getCallCount('onThumbnailRemoved'));
    // When thumbnail is removed, autocomplete should be requeried
    const args = await testProxy.handler.whenCalled('stopAutocomplete');
    assertTrue(args.clearResult);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('thumbnail keyboard deletion', async () => {
    realbox.$.input.value = '';
    testProxy.callbackRouterRemote.setThumbnail('foo.png');
    await waitAfterNextRender(realbox);
    const thumbnail = realbox.$.inputWrapper.querySelector('#thumbnail');
    assertTrue(thumbnail !== null);
    realbox.$.input.focus();
    realbox.$.inputWrapper.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Backspace',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await waitAfterNextRender(realbox);
    // First backspace should focus the thumbnail
    assertEquals(thumbnail, getDeepActiveElement());
    realbox.$.inputWrapper.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Backspace',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await waitAfterNextRender(realbox);
    const thumbnailContainer =
        realbox.$.inputWrapper.querySelector<HTMLElement>(
            '#thumbnailContainer');
    assertTrue(thumbnailContainer !== null);
    // When thumbnail is focused, a backspace should delete the thumbnail,
    // focus input, and notify browser.
    assertStyle(thumbnailContainer, 'display', 'none');
    assertEquals(realbox.$.input, getDeepActiveElement());
    await testProxy.handler.whenCalled('onThumbnailRemoved');
    assertEquals(1, testProxy.handler.getCallCount('onThumbnailRemoved'));
    // When thumbnail is removed, autocomplete should be requeried
    const args = await testProxy.handler.whenCalled('stopAutocomplete');
    assertTrue(args.clearResult);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('keyboard deletion with non-empty input', async () => {
    testProxy.callbackRouterRemote.setThumbnail('foo.png');
    await waitAfterNextRender(realbox);
    const thumbnail = realbox.$.inputWrapper.querySelector('#thumbnail');
    assertTrue(thumbnail !== null);
    realbox.$.input.value = 'hi';
    realbox.$.input.focus();
    // Cursor is at the end of the input.
    assertEquals(realbox.$.input.selectionStart, 2);
    const backspaceEvent = new KeyboardEvent('keydown', {
      key: 'Backspace',
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    });
    realbox.$.input.dispatchEvent(backspaceEvent);
    // Checking the input value after a backspace event doesn't work
    // so check the default behavior occurs (deleting a character).
    assertFalse(backspaceEvent.defaultPrevented);
  });
});
