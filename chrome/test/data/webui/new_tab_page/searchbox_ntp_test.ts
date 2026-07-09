// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {NtpSearchboxElement, SearchboxIconElement, SearchboxMatchElement} from 'chrome://new-tab-page/new_tab_page.js';
import {BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {DriveDisclaimerStatus, RenderType, SideType} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {assertIconMaskImageUrl, assertStyle, createClipboardEvent, createUrlMatch, MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

enum Attributes {
  SELECTED = 'selected',
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

  test('input wrapper is hidden when in voice search mode', async () => {
    // Arrange: Create and append realbox element to DOM with active theme.
    realbox = await createAndAppendRealbox();
    await microtasksFinished();

    const inputWrapper =
        realbox.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!inputWrapper);
    const searchboxInput =
        realbox.shadowRoot.querySelector<HTMLElement>('#input');
    assertTrue(!!searchboxInput);
    const dropdownContainer =
        realbox.shadowRoot.querySelector<HTMLElement>('.dropdownContainer');

    // Assert steady state: container layout and active input are visible.
    assertNotEquals('none', getComputedStyle(inputWrapper).display);
    assertEquals('flex', getComputedStyle(inputWrapper).display);
    assertTrue(isVisible(inputWrapper));
    assertTrue(isVisible(searchboxInput));
    assertFalse(realbox.inVoiceSearchMode);

    // Act: Transition component into active voice search listening mode.
    realbox.inVoiceSearchMode = true;
    await microtasksFinished();

    // Assert voice state: wrapper container is removed from visual layout via
    // display: none, concealing background box, shadow, and search input.
    assertEquals('none', getComputedStyle(inputWrapper).display);
    assertFalse(isVisible(inputWrapper));
    assertFalse(isVisible(searchboxInput));
    if (dropdownContainer) {
      assertFalse(isVisible(dropdownContainer));
    }
    assertTrue(realbox.inVoiceSearchMode);

    // Act: Exit voice search mode and return to standard NTP search layout.
    realbox.inVoiceSearchMode = false;
    await microtasksFinished();

    // Assert restored state: wrapper container layout and input return visible.
    assertNotEquals('none', getComputedStyle(inputWrapper).display);
    assertEquals('flex', getComputedStyle(inputWrapper).display);
    assertTrue(isVisible(inputWrapper));
    assertTrue(isVisible(searchboxInput));
    assertFalse(realbox.inVoiceSearchMode);
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

  test('focusing the input triggers onFocusChanged validation', async () => {
    assertEquals(0, testProxy.handler.getCallCount('onFocusChanged'));
    realbox.$.input.inputElement.value = '';
    realbox.$.input.focus();
    assertEquals(realbox.$.input.inputElement, getDeepActiveElement());
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    await testProxy.handler.whenCalled('onFocusChanged');
    assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));
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
          queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));

    // Dropdown should be visible again.
    assertTrue(await areMatchesShowing());

    // Browser returns only 1 match.
    const singleMatch = [createSearchMatchForTesting()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: realbox.activeQueryId,
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: singleMatch,
        }));

    // Dropdown should be hidden (only mirror query match in multi-line mode).
    assertFalse(await areMatchesShowing());
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
          queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
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
              queryId: realbox.activeQueryId,
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
              queryId: realbox.activeQueryId,
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
              queryId: realbox.activeQueryId,
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
              queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
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
          queryId: realbox.activeQueryId,
          input: '',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());
  });

  //============================================================================
  // Test Keydown events for multi-line input
  //============================================================================

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

  test(
      'arrow up/down keys in multi-line input do not query autocomplete',
      async () => {
        realbox.multiLineEnabled = true;
        realbox.initialInputScrollHeight = 50;
        await microtasksFinished();
        Object.defineProperty(realbox.$.input, 'scrollHeight', {
          value: 51,
          configurable: true,
        });

        realbox.$.input.inputElement.dispatchEvent(
            new KeyboardEvent('keydown', {
              bubbles: true,
              cancelable: true,
              composed: true,
              key: 'ArrowDown',
            }));
        await microtasksFinished();
        assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
      });

  test('onOpenDriveUpload_ handles accepted disclaimer', async () => {
    testProxy.handler.setResultFor('getDriveDisclaimerStatus', Promise.resolve({
      status: DriveDisclaimerStatus.kAccepted,
    }));
    testProxy.handler.setResultFor('onDriveUploadClicked', Promise.resolve({
      response: {
        files: [{
          token: {high: 1n, low: 1n},
          mimeType: 'image/png',
          fileName: 'file.png',
          thumbnailUrl: 'thumb',
          iconUrl: {url: 'icon'},
        }],
        error: null,
      },
    }));

    const whenOpenComposebox =
        eventToPromise<CustomEvent>('open-composebox', realbox);

    // Call the protected method.
    await (realbox as unknown as {
      onOpenDriveUpload_: () => Promise<void>,
    }).onOpenDriveUpload_();

    const event = await whenOpenComposebox;
    assertEquals(1n, event.detail.files[0].token.high);
    assertEquals(1n, event.detail.files[0].token.low);
    assertEquals('image/png', event.detail.files[0].mimeType);
    assertEquals('file.png', event.detail.files[0].fileName);
    assertEquals('thumb', event.detail.files[0].thumbnailUrl);
    assertEquals('icon', event.detail.files[0].iconUrl.url);
    assertEquals(1, testProxy.handler.getCallCount('onDriveUploadClicked'));
  });

  test('onOpenDriveUpload_ handles restricted disclaimer', async () => {
    testProxy.handler.setResultFor('getDriveDisclaimerStatus', Promise.resolve({
      status: DriveDisclaimerStatus.kRestricted,
    }));

    // Call the protected method.
    await (realbox as unknown as {
      onOpenDriveUpload_: () => Promise<void>,
    }).onOpenDriveUpload_();

    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('onDriveUploadClicked'));
  });

  test('onOpenDriveUpload_ handles drive upload error', async () => {
    testProxy.handler.setResultFor('getDriveDisclaimerStatus', Promise.resolve({
      status: DriveDisclaimerStatus.kAccepted,
    }));
    testProxy.handler.setResultFor('onDriveUploadClicked', Promise.resolve({
      response: {
        files: [],
        error: {
          errorType: 1,  // kBrowserProcessingError or similar
        },
      },
    }));

    const whenOpenComposebox =
        eventToPromise<CustomEvent>('open-composebox', realbox);

    // Call the protected method.
    await (realbox as unknown as {
      onOpenDriveUpload_: () => Promise<void>,
    }).onOpenDriveUpload_();

    const event = await whenOpenComposebox;
    assertEquals(0, event.detail.files.length);
    assertEquals(1, event.detail.error.errorType);
    assertEquals(1, testProxy.handler.getCallCount('onDriveUploadClicked'));
  });

  test('onOpenDriveUpload_ handles empty selection without opening composebox', async () => {
    testProxy.handler.setResultFor('getDriveDisclaimerStatus', Promise.resolve({
      status: DriveDisclaimerStatus.kAccepted,
    }));
    testProxy.handler.setResultFor('onDriveUploadClicked', Promise.resolve({
      response: {
        files: [],
        error: null,
      },
    }));

    let openComposeboxCalled = false;
    realbox.addEventListener('open-composebox', () => {
      openComposeboxCalled = true;
    });

    // Call the protected method.
    await (realbox as unknown as {
      onOpenDriveUpload_: () => Promise<void>,
    }).onOpenDriveUpload_();

    await microtasksFinished();

    assertFalse(openComposeboxCalled);
    assertEquals(1, testProxy.handler.getCallCount('onDriveUploadClicked'));
  });
});
