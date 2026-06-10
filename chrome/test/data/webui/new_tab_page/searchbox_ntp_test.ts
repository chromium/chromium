// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {NtpSearchboxElement, SearchboxIconElement, SearchboxMatchElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$, BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createAutocompleteMatch, createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {NavigationPredictor} from 'chrome://resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import type {AutocompleteMatch} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {RenderType, SideType} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {assertStyle, MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

enum Attributes {
  SELECTED = 'selected',
}

function createClipboardEvent(name: string): ClipboardEvent {
  return new ClipboardEvent(
      name, {cancelable: true, clipboardData: new DataTransfer()});
}

function createUrlMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  return createAutocompleteMatch({
    swapContentsAndDescription: true,
    contents: 'helloworld.com',
    contentsClass: [{offset: 0, style: 1}],
    destinationUrl: 'https://helloworld.com/',
    fillIntoEdit: 'https://helloworld.com',
    type: 'url-what-you-typed',
    ...modifiers,
  });
}

function createCalculatorMatch(modifiers: Partial<AutocompleteMatch>):
    AutocompleteMatch {
  return createAutocompleteMatch({
    isSearchType: true,
    contents: '2 + 3',
    contentsClass: [{offset: 0, style: 0}],
    description: '5',
    descriptionClass: [{offset: 0, style: 0}],
    destinationUrl: 'https://www.google.com/search?q=2+%2B+3',
    fillIntoEdit: '5',
    type: 'search-calculator-answer',
    iconPath: 'calculator_cr23.svg',
    ...modifiers,
  });
}

/** Verifies the autocomplete match is showing. */
function verifyMatch(match: AutocompleteMatch, matchEl: SearchboxMatchElement) {
  assertEquals('option', matchEl.getAttribute('role'));
  const matchContents = match.answer ? match.answer.firstLine : match.contents;
  const matchDescription =
      match.answer ? match.answer.secondLine : match.description;
  const separatorText =
      (match.swapContentsAndDescription ? match.contents : match.description) ?
      loadTimeData.getString('searchboxSeparator') :
      '';
  const contents = matchEl.$['contents'].textContent;
  const separator = matchEl.$['separator'].textContent;
  const description = matchEl.$['description'].textContent;
  const text = contents + separator + description;
  assertEquals(
      match.swapContentsAndDescription ?
          matchDescription + separatorText + matchContents :
          matchContents + separatorText + matchDescription,
      text);
}

function arrowDown(realbox: NtpSearchboxElement): KeyboardEvent {
  const arrowDownEvent = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    composed: true,  // So it propagates across shadow DOM boundary.
    key: 'ArrowDown',
  });
  realbox.$.input.inputElement.dispatchEvent(arrowDownEvent);
  return arrowDownEvent;
}

async function createAndAppendRealbox(
    properties: Partial<NtpSearchboxElement> = {}):
    Promise<NtpSearchboxElement> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const realbox = document.createElement('ntp-searchbox');
  Object.assign(realbox, properties);
  document.body.appendChild(realbox);
  await microtasksFinished();
  return realbox;
}

async function setupRealboxTest(): Promise<{
  realbox: NtpSearchboxElement,
  testProxy: TestSearchboxBrowserProxy,
  testMetricsReporterProxy: TestMock<BrowserProxyImpl>,
}> {
  loadTimeData.overrideValues({
    contextualMenuUsePecApi: false,
    isLensSearchbox: false,
    searchboxCyclingPlaceholders: false,
    searchboxDefaultIcon: 'search.svg',
    searchboxSeparator: ' - ',
    searchboxVoiceSearch: true,
    reportMetrics: true,
  });

  // Set up Realbox's browser proxy.
  const testProxy = new TestSearchboxBrowserProxy();
  SearchboxBrowserProxy.setInstance(testProxy);

  // Set up MetricsReporter's browser proxy.
  const testMetricsReporterProxy = TestMock.fromClass(BrowserProxyImpl);
  testMetricsReporterProxy.reset();
  const metricsReporterCallbackRouter = new PageMetricsCallbackRouter();
  testMetricsReporterProxy.setResultFor(
      'getCallbackRouter', metricsReporterCallbackRouter);
  testMetricsReporterProxy.setResultFor('getMark', Promise.resolve(null));
  BrowserProxyImpl.setInstance(testMetricsReporterProxy);
  MetricsReporterImpl.setInstanceForTest(new MetricsReporterImpl());

  testProxy.handler.setResultFor('getInputState', {
    state: new MockInputState({
      toolConfigs: [],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
    }),
  });
  const realbox = await createAndAppendRealbox();
  return {realbox, testProxy, testMetricsReporterProxy};
}

suite('SearchboxTest', () => {
  let realbox: NtpSearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let testMetricsReporterProxy: TestMock<BrowserProxyImpl>;

  setup(async () => {
    ({realbox, testProxy, testMetricsReporterProxy} = await setupRealboxTest());
    window.open = () => null;
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
    await microtasksFinished();
    return window.getComputedStyle(realbox.getDropdownElement()).display !==
        'none';
  }

  test('when created is not focused and matches are not showing', async () => {
    assertEquals(0, testProxy.handler.getCallCount('onFocusChanged'));
    assertFalse(realbox.hidden);
    assertNotEquals(realbox, getDeepActiveElement());
    assertFalse(await areMatchesShowing());
  });

  test('Voice search button is present by default', async () => {
    // Arrange.
    realbox = await createAndAppendRealbox();

    // Assert
    const voiceSearchButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#voiceSearchButton');
    assertTrue(!!voiceSearchButton);
  });

  test('Voice search button is not present when not enabled', async () => {
    // Arrange.
    loadTimeData.overrideValues({searchboxVoiceSearch: false});
    realbox = await createAndAppendRealbox();

    // Assert
    const voiceSearchButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#voiceSearchButton');
    assertFalse(!!voiceSearchButton);
  });

  test('clicking voice search button send voice search event', async () => {
    // Arrange.
    realbox = await createAndAppendRealbox();

    const whenOpenVoiceSearch = eventToPromise('open-voice-search', realbox);

    // Act.
    const voiceSearchButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#voiceSearchButton');
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();

    // Assert.
    await whenOpenVoiceSearch;
  });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip('realbox default Google G icon', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      searchboxDefaultIcon:
          '//resources/cr_components/searchbox/icons/google_g.svg',
    });
    realbox = await createAndAppendRealbox();

    const faviconImage = realbox.$.input.$.icon.$.faviconImage;
    assertTrue(!!faviconImage);

    const loadPromise = eventToPromise('load', faviconImage);
    faviconImage.dispatchEvent(new Event('load'));
    await loadPromise;

    // Assert.
    assertTrue(isVisible(faviconImage));
    assertEquals(
        faviconImage.getAttribute('src'),
        '//resources/cr_components/searchbox/icons/google_g.svg');

    const realboxIcon = realbox.$.input.$.icon.$.icon;
    assertFalse(isVisible(realboxIcon));
  });

  const webkitTestCases = [
    {
      description: 'theming refresh disabled',
      properties: {
        searchboxChromeRefreshTheming: false,
      },
      shouldUseWebkit: false,
    },
    {
      description: 'theming refresh enabled',
      properties: {
        searchboxChromeRefreshTheming: true,
      },
      shouldUseWebkit: true,
    },
  ];
  webkitTestCases.forEach(({description, properties, shouldUseWebkit}) => {
    test(`useWebkitSearchIcons ${description}`, async () => {
      // Arrange.
      realbox = await createAndAppendRealbox(properties);

      // Assert
      const [iconProperty, nonIconProperty] = shouldUseWebkit ?
          ['-webkit-mask-image', 'background-image'] :
          ['background-image', '-webkit-mask-image'];
      const buttonsToTest = [
        {
          selector: '#voiceSearchButton',
          iconUrl:
              'url("chrome://resources/cr_components/searchbox/icons/mic.svg")',
        },
        {
          selector: '#lensSearchButton',
          iconUrl: 'url("chrome://resources/cr_components/searchbox/icons/' +
              'camera.svg")',
        },
      ];
      for (const {selector, iconUrl} of buttonsToTest) {
        const button = realbox.shadowRoot.querySelector<HTMLElement>(selector);
        assertTrue(!!button);
        assertStyle(button, iconProperty, iconUrl);
        assertStyle(button, nonIconProperty, 'none');
      }
    });
  });

  //============================================================================
  // Test Querying Autocomplete
  //============================================================================

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip('left-clicking the input queries autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.inputElement.value = '';
    // Left click queries autocomplete when matches are not showing.
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));

    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Left click does not query autocomplete when matches are showing.
    // Need to manually focus in order to trigger `onFocusChanged()` since
    // `autocompleteResultChanged` does not focus input.
    realbox.$.input.focus();
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));
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
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 1}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(2, testProxy.handler.getCallCount('onFocusChanged'));

    // Left click queries autocomplete when input is non-empty.
    realbox.$.input.inputElement.value = '   ';
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('focusing the input does not query autocomplete', async () => {
    assertEquals(0, testProxy.handler.getCallCount('onFocusChanged'));
    realbox.$.input.inputElement.value = '';
    realbox.$.input.focus();
    assertEquals(realbox.$.input.inputElement, getDeepActiveElement());
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));
  });

  test('tabbing into empty input queries autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.inputElement.value = '';
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(
        1, testProxy.handler.getCallCount('queryAutocomplete'),
        'query autocomplete count');

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length, 'match count');

    // Tabbing into input does not query autocomplete when matches are
    // showing.
    realbox.$.input.inputElement.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    }));
    assertEquals(
        0,
        testProxy.handler.getCallCount(
            'queryAutocomplete',
            ),
        'query autocomplete count when matches showing');

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Tabbing into empty input queries autocomplete.
    realbox.$.input.inputElement.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    }));
    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Tabbing into non-empty input queries autocomplete.
    realbox.$.input.inputElement.value = '   ';
    realbox.$.input.inputElement.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    }));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('arrow up/down keys in empty input query autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.inputElement.value = '';
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));
    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Arrow up/down keys do not query autocomplete when matches are showing.
    realbox.$.input.inputElement.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'ArrowUp',
    }));
    await microtasksFinished();
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Arrow up/down keys in multiline input do not query autocomplete.
    realbox.multiLineEnabled = true;
    await microtasksFinished();
    Object.defineProperty(realbox.$.input.inputElement, 'scrollHeight', {
      value: 51,
      configurable: true,
    });

    realbox.$.input.inputElement.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'ArrowDown',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('arrow up/down keys in non-empty input query autocomplete', async () => {
    // Query matches.
    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(
        1, testProxy.handler.getCallCount('queryAutocomplete'),
        'autocomplete queried');

    testProxy.handler.reset();

    // Show matches.
    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'hello',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing(), 'matches showing');

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // Arrow up/down keys do not query autocomplete when matches are showing.
    realbox.$.input.inputElement.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
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
    realbox.$.input.inputElement.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'ArrowDown',
    }));
    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('empty input does not query autocomplete', () => {
    realbox.$.input.inputElement.value = '';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('typing space does not query autocomplete', () => {
    realbox.$.input.inputElement.value = ' ';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('typing queries autocomplete', async () => {
    realbox.$.input.inputElement.value = 'he';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Deleting text from input prevents inline autocompletion.
    realbox.$.input.inputElement.value = 'h';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    realbox.$.input.inputElement.value = 'he';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Pasting text into the input prevents inline autocompletion.
    const pasteEvent = createClipboardEvent('paste');
    realbox.$.input.inputElement.dispatchEvent(pasteEvent);
    assertFalse(pasteEvent.defaultPrevented);
    realbox.$.input.inputElement.value = 'hel';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    realbox.$.input.inputElement.value = 'hell';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // If caret isn't at the end of the text inline autocompletion is prevented.
    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.setSelectionRange(0, 0);  // Move caret to beginning.
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // If text is being composed with an IME inline autocompletion is prevented.
    realbox.$.input.inputElement.value = 'hello 간';
    const inputEvent = new InputEvent('input', {isComposing: true});
    realbox.$.input.inputElement.dispatchEvent(inputEvent);

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();
  });

  test('queryAutocomplete passes cursor position', async () => {
    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.selectionStart = 3;
    realbox.$.input.inputElement.selectionEnd = 3;
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, 'hello');
    assertEquals(args.cursorPosition, 3);
  });

  test(
      'queryAutocomplete passes cursor position when input is out of sync',
      async () => {
        // Simulate a programmatic update that makes them out of sync
        realbox.$.input.inputElement.value = 'hello';
        realbox.$.input.inputElement.selectionStart = 3;
        realbox.$.input.inputElement.selectionEnd = 3;

        // Dispatch event with 'hello world' but DOM is still 'hello'
        realbox.$.input.dispatchEvent(new CustomEvent(
            'searchbox-input-text-updated',
            {detail: {value: 'hello world', isComposing: false}}));

        const args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, 'hello world');
        assertEquals(args.cursorPosition, 11);
      });

  test('clearing the input stops autocomplete', async () => {
    realbox.$.input.inputElement.value = 'h';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    realbox.$.input.inputElement.value = '';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    args = await testProxy.handler.whenCalled('stopAutocomplete');
    assertTrue(args.clearResult);
  });


  //============================================================================
  // Test Autocomplete Response
  //============================================================================

  test('autocomplete response', async () => {
    realbox.$.input.inputElement.value = '      hello world';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    assertEquals('listbox', realbox.getDropdownElement().getAttribute('role'));
    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);
    verifyMatch(matches[0]!, matchEls[0]!);
    verifyMatch(matches[1]!, matchEls[1]!);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    assertEquals('      hello world', realbox.$.input.inputElement.value);
    const start = realbox.$.input.inputElement.selectionStart!;
    const end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals('', realbox.$.input.inputElement.value.substring(start, end));
  });

  test('autocomplete response with inline autocompletion', async () => {
    realbox.$.input.inputElement.value = 'hello ';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    const matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'world',
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);
    verifyMatch(matches[0]!, matchEls[0]!);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    assertEquals('hello world', realbox.$.input.inputElement.value);
    let start = realbox.$.input.inputElement.selectionStart!;
    let end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(
        'world', realbox.$.input.inputElement.value.substring(start, end));

    // Define a new |value| property on the input to see whether it gets set.
    let inputValueChanged = false;
    const originalValueProperty =
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value')!;
    Object.defineProperty(realbox.$.input.$.input, 'value', {
      get: originalValueProperty.get,
      set: (value) => {
        inputValueChanged = true;
        originalValueProperty.set!.call(realbox.$.input.$.input, value);
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
    realbox.$.input.inputElement.dispatchEvent(keyEvent);
    assertTrue(keyEvent.defaultPrevented);

    assertFalse(inputValueChanged);
    assertEquals('hello world', realbox.$.input.inputElement.value);
    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(
        'orld', realbox.$.input.inputElement.value.substring(start, end));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, 'hello w');
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('autocomplete response perserves cursor position', async () => {
    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.selectionStart = 0;
    realbox.$.input.inputElement.selectionEnd = 4;
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      contents: 'hello',
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    assertEquals('hello', realbox.$.input.inputElement.value);
    const start = realbox.$.input.inputElement.selectionStart;
    const end = realbox.$.input.inputElement.selectionEnd;
    assertEquals(
        'hell', realbox.$.input.inputElement.value.substring(start, end));
  });

  test('stale autocomplete response is ignored', async () => {
    realbox.$.input.inputElement.value = 'he';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'h',  // Simulate stale response.
          matches: matches,
        }));
    assertFalse(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(0, matchEls.length);
  });

  test('autocomplete response changes', async () => {
    realbox.$.input.inputElement.value = 'he';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    let matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    realbox.$.input.inputElement.value += 'll';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
        }));
    assertFalse(await areMatchesShowing());

    matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(0, matchEls.length);

    realbox.$.input.inputElement.value += 'o';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);
  });

  test('autocomplete should not query for empty inputs', async () => {
    realbox.$.input.inputElement.value = 'he';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    // Deleting a character still queries autocomplete.
    realbox.$.input.inputElement.value = 'h';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(2, testProxy.handler.getCallCount('queryAutocomplete'));

    // Deleting a character does not query autocomplete for empty input.
    realbox.$.input.inputElement.value = '';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    assertEquals(2, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('autocomplete result change does not impact focus', async () => {
    realbox = await createAndAppendRealbox();
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));

    // Voice search button is visible when input is empty.
    realbox.shadowRoot.querySelector<HTMLElement>(
                          '#voiceSearchButton')!.focus();
    assertEquals('voiceSearchButton', getDeepActiveElement()!.id);

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    assertEquals('voiceSearchButton', getDeepActiveElement()!.id);
  });

  test('dropdown suppressed in multi-line mode', async () => {
    realbox = await createAndAppendRealbox({multiLineEnabled: true});

    const initialScrollHeight = realbox.$.input.scrollHeight;

    // The text currently fits on one line (no wrapping).
    Object.defineProperty(realbox.$.input, 'scrollHeight', {
      value: initialScrollHeight,
      configurable: true,
    });

    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));

    // Dropdown should be visible (not wrapping and matchNums > 1).
    assertTrue(await areMatchesShowing());

    // Simulate text wrapping.
    Object.defineProperty(realbox.$.input, 'scrollHeight', {
      value: initialScrollHeight * 2,
      configurable: true,
    });

    realbox.$.input.inputElement.value = 'hello world';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));

    // Dropdown should be hidden.
    assertFalse(await areMatchesShowing());

    // Reset wrapping (simulate text deleted or unwrapped).
    Object.defineProperty(realbox.$.input, 'scrollHeight', {
      value: initialScrollHeight,
      configurable: true,
    });

    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));

    // Dropdown should be visible again.
    assertTrue(await areMatchesShowing());

    // Browser returns only 1 match.
    const singleMatch = [createSearchMatchForTesting()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: singleMatch,
        }));

    // Dropdown should be hidden (only mirror query match in multi-line mode).
    assertFalse(await areMatchesShowing());
  });

  //============================================================================
  // Test Navigation
  //============================================================================

  test('pressing Enter on input navigates to the selected match', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.inputElement.dispatchEvent(new Event('focus'));

    realbox.$.input.inputElement.value = 'hello ';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: true,
        inlineAutocompletion: 'world',
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // Before navigation, input should be inline autocompleted.
    assertEquals('hello world', realbox.$.input.inputElement.value);
    let start = realbox.$.input.inputElement.selectionStart!;
    let end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(
        'world', realbox.$.input.inputElement.value.substring(start, end));

    // Pressing enter...
    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    realbox.$.input.inputElement.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Navigates to the first match.
    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl, args.url);
    assertTrue(args.areMatchesShowing);
    assertTrue(args.shiftKey);
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));

    // Navigation should close the dropdown.
    assertFalse(await areMatchesShowing());

    // After navigation, the inline autocompletion should be applied to the text
    // shown in the input and there should be no visible selection.
    assertEquals('hello world', realbox.$.input.inputElement.value);
    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals('', realbox.$.input.inputElement.value.substring(start, end));
  });

  test(
      'pressing Enter on input navigates to *hidden* selected match',
      async () => {
        // Input is expected to have been focused before any navigation.
        realbox.$.input.inputElement.dispatchEvent(new Event('focus'));

        realbox.$.input.inputElement.value = '  hello  ';
        realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

        const matches = [
          createSearchMatchForTesting({iconPath: 'clock.svg'}),
          createUrlMatch(),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: realbox.$.input.inputElement.value.trimStart(),
              matches: matches,
            }));
        assertTrue(await areMatchesShowing());

        let matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        // Select the first match.
        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));
        await microtasksFinished();

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.inputElement.value);
        // Icon is updated.
        assertIconMaskImageUrl(realbox.$.input.$.icon, 'clock.svg');

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
        matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is not cleared.
        assertEquals('hello world', realbox.$.input.inputElement.value);
        // Icon is not cleared.
        assertIconMaskImageUrl(realbox.$.input.$.icon, 'clock.svg');

        const shiftEnter = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          key: 'Enter',
          composed: true,  // So it propagates across shadow DOM boundary.
          shiftKey: true,
        });
        realbox.$.input.inputElement.dispatchEvent(shiftEnter);
        assertTrue(shiftEnter.defaultPrevented);
        await microtasksFinished();

        // Navigates to the first match.
        const args =
            await testProxy.handler.whenCalled('openAutocompleteMatch');
        assertEquals(0, args.line);
        assertEquals(matches[0]!.destinationUrl, args.url);
        assertFalse(args.areMatchesShowing);
        assertTrue(args.shiftKey);
        assertEquals(
            1, testProxy.handler.getCallCount('openAutocompleteMatch'));
      });

  test('pressing Enter on input is ignored if no selected match', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.inputElement.dispatchEvent(new Event('focus'));

    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
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
    realbox.$.input.inputElement.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Did not navigate to the first match since it's not selected.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test(
      'pressing Enter on input is ignored if no *hidden* selected match',
      async () => {
        realbox.$.input.inputElement.value = '';
        realbox.$.input.inputElement.dispatchEvent(
            new MouseEvent('mousedown', {button: 0}));

        const matches = [
          createSearchMatchForTesting({iconPath: 'clock.svg'}),
          createUrlMatch(),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: realbox.$.input.inputElement.value.trimStart(),
              matches: matches,
            }));
        assertTrue(await areMatchesShowing());

        let matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        // Select the first match.
        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));
        await microtasksFinished();

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.inputElement.value);
        // Icon is updated.
        assertIconMaskImageUrl(realbox.$.input.$.icon, 'clock.svg');

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
        matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(0, matchEls.length);
        // Input is cleared (zero-prefix case).
        assertEquals('', realbox.$.input.inputElement.value);
        // Icon is restored (zero-prefix case).
        assertIconMaskImageUrl(realbox.$.input.$.icon, 'search.svg');

        const shiftEnter = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Enter',
          shiftKey: true,
        });
        realbox.$.input.inputElement.dispatchEvent(shiftEnter);
        assertFalse(shiftEnter.defaultPrevented);
        await microtasksFinished();

        // Did not navigate to the first match since it's not selected.
        assertEquals(
            0, testProxy.handler.getCallCount('openAutocompleteMatch'));
      });

  test('pressing Enter on input too quickly', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.inputElement.dispatchEvent(new Event('focus'));

    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // User types some more and presses Enter before the results update.
    realbox.$.input.inputElement.value = 'hello world';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('queryAutocomplete');

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    realbox.$.input.inputElement.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Did not navigate to the first match since it's stale.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));

    // New matches arrive.
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    // As soon as the new matches arrive, the pending enter triggers a
    // navigation, which closes the dropdown.
    assertFalse(await areMatchesShowing());

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // Navigates to the first match immediately without further user action.
    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl, args.url);
    assertTrue(args.areMatchesShowing);
    assertTrue(args.shiftKey);
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('pressing Enter on the selected match navigates to it', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.inputElement.dispatchEvent(new Event('focus'));

    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
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
    assertEquals(matches[0]!.destinationUrl, args.url);
    assertTrue(args.areMatchesShowing);
    assertTrue(args.shiftKey);
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  //============================================================================
  // Test Deletion
  //============================================================================

  test('Remove button is visible if the match supports deletion', async () => {
    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatchForTesting(),
      createUrlMatch({supportsDeletion: true}),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.remove).display, 'none');

    // Match must be focused/selected for remove button to be shown/
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));
    await microtasksFinished();
    assertNotEquals(
        window.getComputedStyle(matchEls[1]!.$.remove).display, 'none');
  });

  test('Can remove selected match using keyboard shortcut', async () => {
    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch({supportsDeletion: true}),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
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
    realbox.$.input.inputElement.dispatchEvent(deleteEvent);
    assertFalse(deleteEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    const arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    // Second match is selected.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));

    // Unmodified 'Delete' key does not delete matches.
    realbox.$.input.inputElement.dispatchEvent(deleteEvent);
    assertFalse(deleteEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    const shiftDeleteEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Delete',
      shiftKey: true,
    });
    realbox.$.input.inputElement.dispatchEvent(shiftDeleteEvent);
    assertTrue(shiftDeleteEvent.defaultPrevented);
    const args = await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(1, args.line);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
  });

  test('Selection is restored after selected match is removed', async () => {
    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    let matches = [
      createSearchMatchForTesting({
        supportsDeletion: true,
      }),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    let matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
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
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'hello',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    const arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.inputElement.value);

    // Remove the first match.
    const shiftDeleteEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Delete',
      shiftKey: true,
    });
    realbox.$.input.inputElement.dispatchEvent(shiftDeleteEvent);
    assertTrue(shiftDeleteEvent.defaultPrevented);
    args = await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    matches = [createSearchMatchForTesting()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'hello',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.inputElement.value);
  });

  //============================================================================
  // Test Selection
  //============================================================================

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip(
      'pressing Escape selects the first match / hides matches', async () => {
        realbox.$.input.inputElement.value = 'hello';
        realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

        const matches = [createSearchMatchForTesting(), createUrlMatch()];
        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: realbox.$.input.inputElement.value.trimStart(),
              matches: matches,
            }));
        assertTrue(await areMatchesShowing());

        let matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        // Select the second match.
        matchEls[1]!.focus();
        matchEls[1]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));
        await microtasksFinished();

        assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
        assertEquals(
            'https://helloworld.com', realbox.$.input.inputElement.value);
        assertEquals(
            matchEls[1], realbox.getDropdownElement().shadowRoot.activeElement);

        let escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);
        await microtasksFinished();

        // First match gets selected and also gets the focus.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        assertEquals('hello world', realbox.$.input.inputElement.value);
        assertEquals(
            matchEls[0], realbox.getDropdownElement().shadowRoot.activeElement);

        escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);

        // Matches are hidden.
        assertFalse(await areMatchesShowing());

        // Matches are cleared.
        matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(0, matchEls.length);
        // Input is cleared.
        assertEquals('', realbox.$.input.inputElement.value);

        // Show zero-prefix matches.
        realbox.$.input.inputElement.dispatchEvent(
            new MouseEvent('mousedown', {button: 0}));
        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              matches: matches,
            }));
        assertTrue(await areMatchesShowing());

        matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        // Pressing 'Escape' when no matches are selected closes the dropdown.
        escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);

        // Matches are hidden.
        assertFalse(await areMatchesShowing());

        // Matches are cleared.
        matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(0, matchEls.length);
      });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip('arrow up/down moves selection / focus', async () => {
    realbox.$.input.focus();
    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    let arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    // First match is selected but does not get focus while focus is in the
    // input.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.inputElement.value);
    assertEquals(realbox.$.input, realbox.shadowRoot.activeElement);

    // If text is being composed with an IME composition selection is prevented.
    arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      isComposing: true,
      key: 'ArrowDown',
    });
    realbox.$.input.inputElement.dispatchEvent(arrowDownEvent);
    assertFalse(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    // First match remains selected and does not get focus while focus is in the
    // input.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.inputElement.value);
    assertEquals(realbox.$.input, realbox.shadowRoot.activeElement);

    arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    // Second match gets selected but does not get focus while focus is in the
    // input.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.inputElement.value);
    assertEquals(realbox.$.input, realbox.shadowRoot.activeElement);

    // Move the focus to the second match.
    matchEls[1]!.focus();
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));

    // Second match is selected and has focus.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.inputElement.value);
    assertEquals(
        matchEls[1], realbox.getDropdownElement().shadowRoot.activeElement);

    const arrowUpEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowUp',
    });
    matchEls[1]!.dispatchEvent(arrowUpEvent);
    assertTrue(arrowUpEvent.defaultPrevented);
    await microtasksFinished();

    // First match gets selected and gets focus while focus is in the matches.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', realbox.$.input.inputElement.value);
    assertEquals(
        matchEls[0], realbox.getDropdownElement().shadowRoot.activeElement);

    // Changing match selection doesn't result in another onFocusChanged call
    // because focus is for the whole realbox (including input container).
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));
  });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip('focus indicator', async () => {
    realbox.$.input.focus();
    realbox.$.input.inputElement.value = 'clear browsing history';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatchForTesting({
      actions: [{
        hint: 'Clear Browsing History',
        suggestionContents: '',
        iconPath: 'chrome://theme/current-channel-logo',
        a11yLabel: '',
      }],
      fillIntoEdit: 'clear browsing history',
      supportsDeletion: true,
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');

    const focusIndicator = matchEls[0]!.$.focusIndicator;

    // Select the first match
    const arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('clear browsing history', realbox.$.input.inputElement.value);
    assertTrue(isVisible(focusIndicator));

    // Give focus to the action button
    const action = $$<HTMLElement>(matchEls[0]!, '#action')!;
    action.focus();

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals(action, matchEls[0]!.shadowRoot.activeElement);
    assertFalse(isVisible(focusIndicator));

    // Give focus to remove button
    const removeButton = matchEls[0]!.$.remove;
    removeButton.focus();

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals(removeButton, matchEls[0]!.shadowRoot.activeElement);
    assertFalse(isVisible(focusIndicator));
  });

  //============================================================================
  // Test Responsiveness Metrics
  //============================================================================

  test('responsiveness metrics are being recorded', async () => {
    realbox.$.input.inputElement.value = 'he';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    // The responsiveness metrics are not recorded until the results are
    // painted.
    assertEquals(0, testMetricsReporterProxy.getCallCount('umaReportTime'));

    let matches = [createSearchMatchForTesting()];
    MetricsReporterImpl.getInstance().mark('ResultChanged');  // Marked in C++.
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    // The responsiveness metrics are recorded once the results are painted.
    await testMetricsReporterProxy.whenCalled('umaReportTime');
    assertEquals(2, testMetricsReporterProxy.getCallCount('umaReportTime'));
    await testMetricsReporterProxy.whenCalled('clearMark');

    // Delete the last character.
    realbox.$.input.inputElement.value = 'h';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'ello',
    })];
    MetricsReporterImpl.getInstance().mark('ResultChanged');  // Marked in C++.
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    // Only one responsiveness metric is recorded when characters are deleted.
    await testMetricsReporterProxy.whenCalled('umaReportTime');
    assertEquals(3, testMetricsReporterProxy.getCallCount('umaReportTime'));
    await testMetricsReporterProxy.whenCalled('clearMark');

    assertEquals('hello', realbox.$.input.inputElement.value);
    const start = realbox.$.input.inputElement.selectionStart!;
    const end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(
        'ello', realbox.$.input.inputElement.value.substring(start, end));

    // Type the next character of the inline autocompletion.
    const keyEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'e',
    });
    realbox.$.input.inputElement.dispatchEvent(keyEvent);
    assertTrue(keyEvent.defaultPrevented);

    matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'llo',
    })];
    MetricsReporterImpl.getInstance().mark('ResultChanged');  // Marked in C++.
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'he',
          matches: matches,
        }));
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
        realbox.$.input.inputElement.value = 'hello';
        realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

        const matches = [
          createSearchMatchForTesting({iconPath: 'clock.svg'}),
          createUrlMatch({iconPath: 'page.svg'}),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: realbox.$.input.inputElement.value.trimStart(),
              matches: matches,
            }));
        assertTrue(await areMatchesShowing());

        const matchEls =
            realbox.getDropdownElement().shadowRoot.querySelectorAll(
                'cr-searchbox-match');
        assertEquals(2, matchEls.length);
        assertIconMaskImageUrl(matchEls[0]!.$.icon, 'clock.svg');
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(matchEls[1]!.$.icon, matches[1]!.destinationUrl.url);
        assertIconMaskImageUrl(
            realbox.$.input.$.icon, 'search.svg');  // Default icon.

        // Select the first match.
        let arrowDownEvent = arrowDown(realbox);
        assertTrue(arrowDownEvent.defaultPrevented);
        await microtasksFinished();

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.inputElement.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.input.$.icon, 'clock.svg');

        // Select the second match.
        arrowDownEvent = arrowDown(realbox);
        assertTrue(arrowDownEvent.defaultPrevented);
        await microtasksFinished();

        // Second match is selected.
        assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals(
            'https://helloworld.com', realbox.$.input.inputElement.value);
        // Realbox icon is updated.
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(realbox.$.input.$.icon,
        // matches[1]!.destinationUrl.url);

        // Select the first match by pressing 'Escape'.
        const escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);
        await microtasksFinished();

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.inputElement.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.input.$.icon, 'clock.svg');
      });

  test(
      'realbox icons is updated when url match is cut from realbox',
      async () => {
        realbox.$.input.inputElement.value = 'www.test.com';
        realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

        const matches = [createUrlMatch(
            {allowedToBeDefaultMatch: true, iconPath: 'page.svg'})];

        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: realbox.$.input.inputElement.value.trimStart(),
              matches: matches,
            }));
        assertTrue(await areMatchesShowing());

        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(realbox.$.input.$.icon,
        // matches[0]!.destinationUrl.url); Select the entire input.
        realbox.$.input.setSelectionRange(
            0, realbox.$.input.inputElement.value.length);

        const cutEvent = createClipboardEvent('cut');
        realbox.$.input.inputElement.dispatchEvent(cutEvent);
        assertTrue(cutEvent.defaultPrevented);
        await microtasksFinished();

        assertIconMaskImageUrl(realbox.$.input.$.icon, 'search.svg');
      });

  test(
      'match icons are updated when entity images become available',
      async () => {
        realbox.$.input.inputElement.value = 'hello';
        realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

        const matches = [
          createUrlMatch({iconPath: 'page.svg'}),
          createSearchMatchForTesting({
            iconPath: 'clock.svg',
            imageUrl: 'https://gstatic.com/',
            imageDominantColor: '#757575',
            isRichSuggestion: true,
          }),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: realbox.$.input.inputElement.value.trimStart(),
              matches: matches,
            }));
        assertTrue(await areMatchesShowing());

        const matchEls =
            realbox.getDropdownElement().shadowRoot.querySelectorAll(
                'cr-searchbox-match');
        assertEquals(2, matchEls.length);
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(matchEls[0]!.$.icon, matches[0]!.destinationUrl.url);
        assertIconMaskImageUrl(matchEls[1]!.$.icon, 'clock.svg');
        assertIconMaskImageUrl(
            realbox.$.input.$.icon, 'search.svg');  // Default icon.

        // Select the first match.
        let arrowDownEvent = arrowDown(realbox);
        assertTrue(arrowDownEvent.defaultPrevented);
        await microtasksFinished();

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals(
            'https://helloworld.com', realbox.$.input.inputElement.value);
        // Realbox icon is updated.
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(matchEls[0]!.$.icon, matches[0]!.destinationUrl.url);

        // Select the second match.
        arrowDownEvent = arrowDown(realbox);
        assertTrue(arrowDownEvent.defaultPrevented);
        await microtasksFinished();

        // Second match is selected.
        assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.inputElement.value);
        // Second match shows a placeholder color until the image loads.
        const containerEl = matchEls[1]!.$.icon.$.container;
        assertStyle(
            containerEl, 'background-color', 'rgba(117, 117, 117, 0.25)');
        assertIconMaskImageUrl(
            realbox.$.input.$.icon, 'search.svg');  // Default icon.

        assertEquals(
            matchEls[1]!.$.icon.$.image.getAttribute('src'),
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[1]!.imageUrl)}`);

        // Mock image finishing loading, which should remove the temporary
        // background color.
        matchEls[1]!.$.icon.$.image.dispatchEvent(new Event('load'));
        await microtasksFinished();
        assertStyle(containerEl, 'background-color', 'rgba(0, 0, 0, 0)');
        // Realbox icon is not updated as the input does not feature images.
        assertIconMaskImageUrl(
            realbox.$.input.$.icon, 'search.svg');  // Default icon.
        assertTrue(
            window.getComputedStyle(realbox.$.input.$.icon).display !== 'none');

        // Select the first match by pressing 'Escape'.
        const escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);
        await microtasksFinished();

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals(
            'https://helloworld.com', realbox.$.input.inputElement.value);
        // Realbox icon is updated.
        // TODO(crbug.com/328270499): Uncomment once flakiness is fixed.
        // assertFavicon(realbox.$.input.$.icon,
        // matches[0]!.destinationUrl.url);
      });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip(
      'match icons are updated when external icons become available',
      async () => {
        function getIcon(element: NtpSearchboxElement|SearchboxMatchElement):
            SearchboxIconElement {
          if (element.tagName === 'NTP-SEARCHBOX') {
            return (element as NtpSearchboxElement).$.input.$.icon;
          }
          return (element as SearchboxMatchElement).$.icon;
        }

        // Helper function to assert icon states.
        function assertIconState(
            element: NtpSearchboxElement|SearchboxMatchElement|undefined,
            hasEntityImage: boolean, expectUseIconImg: boolean,
            expectedSrc: string|null) {
          const icon = getIcon(element!);
          assertTrue(!!icon.$.icon, 'Icon element does not exists');
          assertEquals(
              isVisible(icon.$.icon), !expectUseIconImg && !hasEntityImage,
              'Icon visibility is incorrect');

          assertTrue(!!icon.$.iconImg, 'Icon image element does not exist');
          assertEquals(
              isVisible(icon.$.iconImg), expectUseIconImg && !hasEntityImage,
              'Icon image visibility is incorrect');

          if (expectedSrc) {
            assertEquals(
                icon.$.iconImg.getAttribute('src'), expectedSrc,
                'Icon image src is incorrect');
          }
        }

        // Helper function to assert and dispatch load event.
        async function assertAndLoadIcon(
            element: NtpSearchboxElement|SearchboxMatchElement|undefined,
            hasEntityImage: boolean, expectedSrc: string|null) {
          // Before load: icon image hidden.
          assertIconState(
              element, hasEntityImage, /*expectUseIconImg=*/ false,
              expectedSrc);

          const icon = getIcon(element!);
          const iconImg = icon.$.iconImg;
          assertTrue(!!iconImg);
          const loadPromise = eventToPromise('load', iconImg);
          iconImg.dispatchEvent(new Event('load'));
          await loadPromise;

          await microtasksFinished();
          // After load: icon image visible.
          assertIconState(
              element, hasEntityImage, /*expectUseIconImg=*/ true, expectedSrc);
        }

        realbox.$.input.inputElement.value = 'hello';
        realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

        const matches = [
          createUrlMatch({
            iconUrl: 'https://helloworld.com/url.png',
            iconPath: 'page.svg',
          }),
          createSearchMatchForTesting({
            iconUrl: 'https://helloworld.com/search.png',
            iconPath: 'clock.svg',
            imageUrl: 'https://gstatic.com/',
            imageDominantColor: '#757575',
            isRichSuggestion: true,
          }),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: realbox.$.input.inputElement.value.trimStart(),
              matches: matches,
            }));
        assertTrue(await areMatchesShowing());

        const matchEls =
            realbox.getDropdownElement().shadowRoot.querySelectorAll(
                'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        // Test initial icon state for the first match: icon image not used.
        assertIconState(
            matchEls[0], /*hasEntityImage=*/ false, /*expectUseIconImg=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[0]!.iconUrl)}`);
        // Test initial icon state for the second match: icon image not used.
        assertIconState(
            matchEls[1], /*hasEntityImage=*/ true, /*expectUseIconImg=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[1]!.iconUrl)}`);

        // Select the first match.
        let arrowDownEvent = arrowDown(realbox);
        assertTrue(arrowDownEvent.defaultPrevented);
        await microtasksFinished();

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals(
            'https://helloworld.com', realbox.$.input.inputElement.value);
        // Realbox icon is updated, but icon image remains not used.
        assertIconState(
            realbox, /*hasEntityImage=*/ false, /*expectUseIconImg=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[0]!.iconUrl)}`);

        // Mock icon image finishing loading for the first match and the realbox
        // itself. The icon image should be used icon.
        await assertAndLoadIcon(
            matchEls[0], /*hasEntityImage=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[0]!.iconUrl)}`);
        await assertAndLoadIcon(
            realbox, /*hasEntityImage=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[0]!.iconUrl)}`);

        // Select the second match.
        arrowDownEvent = arrowDown(realbox);
        assertTrue(arrowDownEvent.defaultPrevented);
        await microtasksFinished();

        // Second match is selected.
        assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.inputElement.value);
        // Realbox icon is updated, but icon image is not used.
        assertIconState(
            realbox, /*hasEntityImage=*/ false, /*expectUseIconImg=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[1]!.iconUrl)}`);
        // Mock icon image finishing loading for the second match and the
        // realbox itself. The icon image should be used.
        await assertAndLoadIcon(
            matchEls[1], /*hasEntityImage=*/ true,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[1]!.iconUrl)}`);
        await assertAndLoadIcon(
            realbox, /*hasEntityImage=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[1]!.iconUrl)}`);

        // Select the first match by pressing 'Escape'.
        const escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);
        await microtasksFinished();

        // First match is selected.
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        // Input is updated.
        assertEquals(
            'https://helloworld.com', realbox.$.input.inputElement.value);
        // Realbox icon is updated, but icon image is not used.
        assertIconState(
            realbox, /*hasEntityImage=*/ false, /*expectUseIconImg=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[0]!.iconUrl)}`);
        // Mock icon image finishing loading for the realbox (now showing the
        // first match's icon image again).
        await assertAndLoadIcon(
            realbox, /*hasEntityImage=*/ false,
            `//image?staticEncode=true&encodeType=webp&url=${
                encodeURIComponent(matches[0]!.iconUrl)}`);
      });


  // TODO(crbug.com/453570027): Test is flaky.
  test.skip('search aggregator people matches use fallback icons', async () => {
    realbox.$.input.inputElement.value = 'hello';
    const inputPromise = eventToPromise('input', realbox.$.input);
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await inputPromise;

    const fallbackIconPath =
        '//resources/cr_components/searchbox/icons/google_agentspace_logo.svg';
    const matches = [
      createUrlMatch({
        iconPath: fallbackIconPath,
        isEnterpriseSearchAggregatorPeopleType: true,
      }),
      createUrlMatch({
        iconUrl: 'https://helloworld-2.com/url.png',
        iconPath: fallbackIconPath,
        isEnterpriseSearchAggregatorPeopleType: true,
        contents: 'helloworld-2.com',
        destinationUrl: 'https://helloworld-2.com/',
        fillIntoEdit: 'https://helloworld-2.com',
      }),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    let faviconImage = matchEls[0]!.$.icon.$.faviconImage;
    assertTrue(!!faviconImage);

    let vectorIcon = matchEls[0]!.$.icon.$.icon;
    assertTrue(!!vectorIcon);

    let loadPromise = eventToPromise('load', faviconImage);
    faviconImage.dispatchEvent(new Event('load'));
    await loadPromise;

    // Test initial icon state for the first match: Google Agentspace logo set
    // as favicon image src.
    assertTrue(isVisible(faviconImage));
    assertEquals(faviconImage.getAttribute('src'), fallbackIconPath);
    assertFalse(isVisible(vectorIcon));

    faviconImage = matchEls[1]!.$.icon.$.faviconImage;
    assertTrue(!!faviconImage);

    vectorIcon = matchEls[1]!.$.icon.$.icon;
    assertTrue(!!vectorIcon);

    loadPromise = eventToPromise('load', faviconImage);
    faviconImage.dispatchEvent(new Event('load'));
    await loadPromise;

    // Test initial icon state for the second match: Google Agentspace logo set
    // as favicon image src.
    assertTrue(isVisible(faviconImage));
    assertEquals(faviconImage.getAttribute('src'), fallbackIconPath);
    assertFalse(isVisible(vectorIcon));

    // Select the first match.
    let arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    // Input is updated.
    assertEquals('https://helloworld.com', realbox.$.input.inputElement.value);

    const realboxIcon = realbox.$.input.$.icon;
    assertTrue(!!realboxIcon);

    loadPromise = eventToPromise('load', realboxIcon.$.faviconImage);
    realboxIcon.$.faviconImage.dispatchEvent(new Event('load'));
    await loadPromise;

    // Realbox icon is updated.
    assertTrue(isVisible(realboxIcon.$.faviconImage));
    assertEquals(
        realboxIcon.$.faviconImage.getAttribute('src'), fallbackIconPath);
    assertFalse(isVisible(realboxIcon.$.icon));
    assertFalse(isVisible(realboxIcon.$.iconImg));

    // Select the second match.
    arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    // Second match is selected.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    // Input is updated.
    assertEquals(
        'https://helloworld-2.com', realbox.$.input.inputElement.value);

    loadPromise = eventToPromise('load', realboxIcon.$.faviconImage);
    realboxIcon.$.faviconImage.dispatchEvent(new Event('load'));
    await loadPromise;

    // Realbox icon is updated.
    assertTrue(isVisible(realboxIcon.$.faviconImage));
    assertEquals(
        realboxIcon.$.faviconImage.getAttribute('src'), fallbackIconPath);
    assertFalse(isVisible(realboxIcon.$.icon));
    assertFalse(isVisible(realboxIcon.$.iconImg));

    // Mock icon image finishing loading for the the realbox
    // itself.
    loadPromise = eventToPromise('load', realboxIcon.$.iconImg);
    realboxIcon.$.iconImg.dispatchEvent(new Event('load'));
    await loadPromise;

    // The icon image should be used and the logo should be hidden.
    assertFalse(isVisible(realboxIcon.$.faviconImage));
    assertFalse(isVisible(realboxIcon.$.icon));
    assertTrue(isVisible(realboxIcon.$.iconImg));
  });

  test('searchboxes always use default icons in searchbox', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      searchboxDefaultIcon: 'hello.svg',
      isLensSearchbox: true,
    });
    realbox = await createAndAppendRealbox();

    assertIconMaskImageUrl(
        realbox.$.input.$.icon, 'hello.svg');  // Default icon.

    realbox.$.input.inputElement.value = 'hello';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [
      createUrlMatch({iconPath: 'page.svg'}),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);

    // Select the first match.
    const arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    // First match is selected.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    // Icon is still default while match is selected.
    assertIconMaskImageUrl(realbox.$.input.$.icon, 'hello.svg');
  });

  //============================================================================
  // Test suggestion groups
  //============================================================================

  test('HidesDropdownIfNoPrimaryMatches', async () => {
    realbox.$.input.inputElement.value = '';
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));

    const matches = [createUrlMatch({suggestionGroupId: 100})];
    const suggestionGroupsMap = {
      100: {
        header: 'People also search for',
        renderType: RenderType.kDefaultVertical,
        sideType: SideType.kSecondary,
      },
    };
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
          suggestionGroupsMap: suggestionGroupsMap,
        }));
    assertFalse(await areMatchesShowing());

    // Verify updating the suggestion group to be a primary group makes the
    // realbox dropdown show.
    suggestionGroupsMap[100].sideType = SideType.kDefaultPrimary;
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());
  });

  //============================================================================
  // Test calculator answer type
  //============================================================================

  test('match calculator answer type', async () => {
    const matches = [createCalculatorMatch({isRichSuggestion: true})];

    realbox.$.input.inputElement.value = '2 + 3';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);

    verifyMatch(matches[0]!, matchEls[0]!);
    assertIconMaskImageUrl(matchEls[0]!.$.icon, 'calculator_cr23.svg');
    assertIconMaskImageUrl(realbox.$.input.$.icon, 'search.svg');

    // Separator is not displayed
    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.separator).display, 'none');

    const arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('5', realbox.$.input.inputElement.value);

    assertIconMaskImageUrl(
        realbox.$.input.$.icon, 'search.svg');  // Default Icon
  });

  //============================================================================
  // Test suggestion answer
  //============================================================================

  test('Test Rich Suggestion Answer for Verbatim Question', async () => {
    realbox.$.input.inputElement.value = 'When is Christmas Day';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    const matches = [createSearchMatchForTesting({
      answer: {
        firstLine: 'When is Christmas Day',
        secondLine: 'Saturday, December 25, 2021',
      },
      isRichSuggestion: true,
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    verifyMatch(matches[0]!, matchEls[0]!);

    // Separator is not displayed
    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.separator).display, 'none');

    const arrowDownEvent = arrowDown(realbox);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    assertIconMaskImageUrl(
        realbox.$.input.$.icon, 'search.svg');  // Default Icon
  });

  //============================================================================
  // Test custom action icons
  //============================================================================

  test('Test action with custom icon', async () => {
    realbox.$.input.inputElement.value = 'Open extension email';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    const matches = [
      createSearchMatchForTesting({
        actions: [{
          hint: 'Open Email',
          suggestionContents: '',
          iconPath: 'data:image/random',
          a11yLabel: '',
        }],
      }),
      createSearchMatchForTesting({
        actions: [{
          hint: 'Open Email',
          suggestionContents: '',
          iconPath: 'icon.png',
          a11yLabel: '',
        }],
      }),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    verifyMatch(matches[0]!, matchEls[0]!);
    verifyMatch(matches[1]!, matchEls[1]!);

    // Match action that has a custom icon associated with it.
    const actionElCustomIcon =
        $$($$(matchEls[0]!, 'cr-searchbox-action')!, '.contents')!;
    const actionIconCustom =
        actionElCustomIcon.querySelector<HTMLElement>('#action-icon')!;
    // Match action that has a standard vector icon associated with it.
    const actionElStandardIcon =
        $$($$(matchEls[1]!, 'cr-searchbox-action')!, '.contents')!;
    const actionIconStandard =
        actionElStandardIcon.querySelector<HTMLElement>('#action-icon')!;

    // Custom icons should use `background-image` while standard vector icons
    // should use `-webkit-mask-image`.
    assertStyle(
        actionIconCustom, 'background-image', 'url("data:image/random")');
    assertStyle(
        actionIconStandard, '-webkit-mask-image',
        'url("chrome://new-tab-page/icon.png")');
  });

  //============================================================================
  // Test pedals
  //============================================================================

  test('Test Actions for Verbatim Query', async () => {
    realbox.$.input.inputElement.value = 'Clear Browsing History';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    const matches = [createSearchMatchForTesting({
      actions: [{
        hint: 'Clear Browsing History',
        suggestionContents: '',
        iconPath: 'chrome://theme/current-channel-logo',
        a11yLabel: '',
      }],
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEl = $$(realbox.getDropdownElement(), 'cr-searchbox-match')!;
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
    realbox.$.input.inputElement.value = 'Clear Bro';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    const matches = [
      createSearchMatchForTesting({contents: 'Clear Bro'}),
      createSearchMatchForTesting({
        actions: [
          {
            hint: 'Clear Browsing History',
            suggestionContents: '',
            iconPath: 'chrome://theme/current-channel-logo',
            a11yLabel: '',
          },
          {
            hint: 'Tab Switch',
            suggestionContents: '',
            iconPath: 'chrome://theme/current-channel-logo',
            a11yLabel: '',
          },
        ],
      }),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    const matchEls = realbox.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
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
        $$(matchEls[1]!.shadowRoot.querySelectorAll('cr-searchbox-action')[1]!,
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
    realbox.$.input.inputElement.value = 'he';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatchForTesting()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    arrowDown(realbox);

    const args = await testProxy.handler.whenCalled('onNavigationLikely');
    assertEquals(0, args.line);
    assertEquals(
        NavigationPredictor.kUpOrDownArrowButton, args.navigationPredictor);
  });

  //============================================================================
  // Test Keyboard Events
  //============================================================================

  test('keyboard modifier keys behavior', () => {
    const metaZEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'z',
      metaKey: true,
    });
    const ctrlZEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'z',
      ctrlKey: true,
    });

    let metaZPropagated = false;
    let ctrlZPropagated = false;
    realbox.addEventListener('keydown', e => {
      if (e.metaKey && e.key === 'z') {
        metaZPropagated = true;
      }
      if (e.ctrlKey && e.key === 'z') {
        ctrlZPropagated = true;
      }
    });

    // Dispatch events to the inputWrapper
    realbox.$.inputWrapper.dispatchEvent(metaZEvent);
    realbox.$.inputWrapper.dispatchEvent(ctrlZEvent);

    // stopPropagation should be called on Mac for meta+Z, and on non-Mac for
    // ctrl+Z.
    assertEquals(!isMac, metaZPropagated);
    assertEquals(isMac, ctrlZPropagated);

    // Default isn't prevented for these explicitly in the handler.
    assertFalse(metaZEvent.defaultPrevented);
    assertFalse(ctrlZEvent.defaultPrevented);
  });

  test('pressing Enter in empty input prevents new line', async () => {
    // Ensure the input is empty.
    realbox.$.input.inputElement.value = '';
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));
    await testProxy.handler.whenCalled('queryAutocomplete');
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: [createSearchMatchForTesting()],
        }));
    await microtasksFinished();
    const enterEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
    });

    // Dispatch the Enter key event.
    realbox.$.input.inputElement.dispatchEvent(enterEvent);
    await microtasksFinished();

    // Assert that the default action (inserting a new line) is prevented.
    assertTrue(enterEvent.defaultPrevented);

    // Assert that no navigation was triggered since the input is empty.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('pressing Shift+Enter in multi-line input allows new line', async () => {
    // Enable multi-line mode.
    realbox.multiLineEnabled = true;
    await microtasksFinished();

    realbox.$.input.inputElement.value = '';

    const shiftEnterEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Enter',
      shiftKey: true,  // Simulate holding the Shift key.
    });

    // Dispatch the Shift + Enter key event.
    realbox.$.input.inputElement.dispatchEvent(shiftEnterEvent);
    await microtasksFinished();

    // Assert that the default action is NOT prevented (browser will insert new
    // line).
    assertFalse(shiftEnterEvent.defaultPrevented);

    // Assert that no navigation was triggered.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });
});
