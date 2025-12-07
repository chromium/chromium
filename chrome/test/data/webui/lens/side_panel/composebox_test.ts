// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome-untrusted://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome-untrusted://resources/cr_components/composebox/composebox_proxy.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {type AutocompleteMatch, type AutocompleteResult, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome-untrusted://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome-untrusted://webui-test/test_mock.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

// This is a slow function, but is guaranteed to return false if the element is
// not actually visible. isVisible() is fast, but may return true even if the
// element is has opacity: 0 or is hidden because its parents opacity is 0.
function isTrulyVisible(element: HTMLElement): boolean {
  let el: HTMLElement|null = element;
  while (el) {
    const style = window.getComputedStyle(el);
    if (style.display === 'none' || style.visibility === 'hidden' ||
        style.opacity === '0') {
      return false;
    }
    el = el.parentElement;
  }
  return true;
}

// Returns a promise that resolves when the element has finished any
// transition. If a property is provided, only resolves when that property has
// finished transitioning.
function getTransitionEndPromise(
    element: HTMLElement, property?: string): Promise<void> {
  return new Promise<void>(
      resolve =>
          element.addEventListener('transitionend', (e: TransitionEvent) => {
            if (!property || e.propertyName === property) {
              resolve();
            }
          }));
}
suite('Composebox', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let lensSidePanelElement: LensSidePanelAppElement;
  let mockPageHandler: TestMock<PageHandlerRemote>;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;

  function createAutocompleteMatch(): AutocompleteMatch {
    return {
      isHidden: false,
      a11yLabel: '',
      actions: [],
      allowedToBeDefaultMatch: false,
      isSearchType: false,
      isEnterpriseSearchAggregatorPeopleType: false,
      swapContentsAndDescription: false,
      supportsDeletion: false,
      suggestionGroupId: -1,  // Indicates a missing suggestion group Id.
      contents: '',
      contentsClass: [{offset: 0, style: 0}],
      description: '',
      descriptionClass: [{offset: 0, style: 0}],
      destinationUrl: {url: ''},
      inlineAutocompletion: '',
      fillIntoEdit: '',
      iconPath: '',
      iconUrl: {url: ''},
      imageDominantColor: '',
      imageUrl: '',
      isNoncannedAimSuggestion: false,
      removeButtonA11yLabel: '',
      type: '',
      isRichSuggestion: false,
      isWeatherAnswerSuggestion: null,
      answer: null,
      tailSuggestCommonPrefix: null,
      hasInstantKeyword: false,
      keywordChipHint: '',
      keywordChipA11y: '',
    };
  }

  function createAutocompleteResult(
      modifiers: Partial<AutocompleteResult> = {}): AutocompleteResult {
    const base: AutocompleteResult = {
      input: '',
      matches: [],
      suggestionGroupsMap: {},
      smartComposeInlineHint: null,
    };

    return Object.assign(base, modifiers);
  }

  function createSearchMatch(modifiers: Partial<AutocompleteMatch> = {}):
      AutocompleteMatch {
    return Object.assign(
        createAutocompleteMatch(), {
          isSearchType: true,
          contents: 'hello world',
          destinationUrl: {url: 'https://www.google.com/search?q=hello+world'},
          fillIntoEdit: 'hello world',
          type: 'search-suggest',
        },
        modifiers);
  }

  // Returns the composebox element.
  async function setupTest(): Promise<HTMLElement> {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    // Mock the composebox handlers.
    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockPageHandler as any, new PageCallbackRouter(),
        mockSearchboxPageHandler as any, searchboxCallbackRouter));

    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);

    await waitAfterNextRender(lensSidePanelElement);
    const composebox =
        lensSidePanelElement.shadowRoot!.querySelector('cr-composebox');
    assertTrue(!!composebox);

    testBrowserProxy.page.setIsOverlayShowing(false);
    await waitAfterNextRender(lensSidePanelElement);

    return composebox;
  }

  test('HidesComposeboxWhenDisabled', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: false});
    const composebox = await setupTest();

    assertFalse(isVisible(composebox));
    assertTrue(isVisible(lensSidePanelElement.$.searchboxContainer));
  });

  test('ShowsComposeboxWhenEnabled', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    assertTrue(isVisible(composebox));
    assertFalse(isVisible(lensSidePanelElement.$.searchboxContainer));
  });

  test('HidesFileInputs', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    const imageUploadButton =
        composebox.shadowRoot!.querySelector('#imageUploadButton');
    const fileUploadButton =
        composebox.shadowRoot!.querySelector('#fileUploadButton');

    // The hide-file-inputs_ attribute is added in side_panel_app.html, so
    // the file input buttons should not be visible.
    assertFalse(isVisible(imageUploadButton));
    assertFalse(isVisible(fileUploadButton));
  });

  test('ShowsLensButtonWhenEnabled', async () => {
    loadTimeData.overrideValues(
        {enableAimSearchbox: true, showLensButton: true});
    const composebox = await setupTest();

    const lensButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#lensIcon');
    assertTrue(!!lensButton);

    // The button should be visible.
    assertTrue(isTrulyVisible(lensButton));

    // Grab the input to focus it.
    const input = composebox.shadowRoot!.querySelector<HTMLTextAreaElement>(
        'textarea#input');
    assertTrue(!!input);

    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);

    // Focusing the input should expand the composebox.
    const expandPromise =
        getTransitionEndPromise(animatedElement, 'max-height');
    input.focus();
    await expandPromise;

    // The button should still be visible now that the composebox is expanded.
    assertTrue(isTrulyVisible(lensButton));
  });

  test('HidesLensButtonWhenDisabled', async () => {
    loadTimeData.overrideValues(
        {enableAimSearchbox: true, showLensButton: false});
    const composebox = await setupTest();

    const lensButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#lensIcon');
    assertTrue(!!lensButton);
    assertFalse(isVisible(lensButton));
  });

  test('IsCollapsible', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);

    const initialHeight = composebox.offsetHeight;
    assertTrue(initialHeight > 0);

    const input = composebox.shadowRoot!.querySelector<HTMLTextAreaElement>(
        'textarea#input');
    assertTrue(!!input);

    // Focusing the input should expand the composebox.
    const expandPromise =
        getTransitionEndPromise(animatedElement, 'max-height');
    input.focus();
    await expandPromise;
    const expandedHeight = composebox.offsetHeight;
    assertTrue(expandedHeight > initialHeight);

    // Blurring the input should collapse the composebox.
    const collapsePromise =
        getTransitionEndPromise(animatedElement, 'max-height');
    input.blur();
    await collapsePromise;
    assertEquals(initialHeight, composebox.offsetHeight);
  });

  test('ButtonsUpdateOnInputAndExpansion', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    // Grab the buttons to do visibility checks.
    const submitContainer =
        composebox.shadowRoot!.querySelector<HTMLElement>('#submitContainer');
    const submitButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#submitIcon');
    const cancelButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#cancelIcon');
    assertTrue(!!submitContainer);
    assertTrue(!!submitButton);
    assertTrue(!!cancelButton);

    // The buttons should not be visible initially while the composebox is
    // collapsed.
    assertFalse(isTrulyVisible(submitContainer));
    assertFalse(isTrulyVisible(submitButton));
    assertFalse(isTrulyVisible(cancelButton));

    // Grab the input to focus it.
    const input = composebox.shadowRoot!.querySelector<HTMLTextAreaElement>(
        'textarea#input');
    assertTrue(!!input);

    // Focusing the input should expand the composebox.
    const expansionPromise =
        getTransitionEndPromise(submitContainer, 'opacity');
    input.focus();
    await expansionPromise;

    // With no text, submit button is visible but disabled. Cancel is not
    // visible and is disabled.
    assertTrue(isTrulyVisible(submitContainer));
    assertTrue(isTrulyVisible(submitButton));
    assertTrue(submitButton.hasAttribute('disabled'));
    assertFalse(isTrulyVisible(cancelButton));
    assertTrue(cancelButton.hasAttribute('disabled'));

    // The buttons should be visible now that there is text.
    input.value = 'hello world';
    const cancelShowPromise = getTransitionEndPromise(cancelButton, 'opacity');
    const cancelContainerShowPromise =
        getTransitionEndPromise(cancelButton.parentElement!, 'opacity');
    const submitContainerShowPromise =
        getTransitionEndPromise(submitContainer, 'opacity');
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await waitAfterNextRender(composebox);
    await Promise.all([
      cancelShowPromise, cancelContainerShowPromise, submitContainerShowPromise,
    ]);

    assertTrue(isTrulyVisible(submitContainer));
    assertTrue(isTrulyVisible(submitButton));
    assertFalse(submitButton.hasAttribute('disabled'));
    assertTrue(isTrulyVisible(cancelButton));
    assertFalse(cancelButton.hasAttribute('disabled'));

    // Clear the input to allow the composebox to collapse.
    input.value = '';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await waitAfterNextRender(composebox);

    // Blur the input to collapse the composebox.
    const submitHidePromise = getTransitionEndPromise(
        submitContainer, 'opacity');
    const cancelHidePromise =
        getTransitionEndPromise(cancelButton.parentElement!, 'opacity');
    input.blur();
    await waitAfterNextRender(composebox);
    await Promise.all([submitHidePromise, cancelHidePromise]);

    // The buttons should not be visible again.
    assertFalse(isTrulyVisible(submitContainer));
    assertFalse(isTrulyVisible(submitButton));
    assertFalse(isTrulyVisible(cancelButton));
  });

  test('HidesDropdownWhenDisabled', async () => {
    loadTimeData.overrideValues({
      enableAimSearchbox: true,
      enableLensAimSuggestions: false,
      composeboxShowZps: false,
    });
    const composebox = await setupTest();
    const dropdown =
        composebox.shadowRoot!.querySelector<HTMLElement>('[part=dropdown]');
    assertTrue(!!dropdown);

    // Focus input to expand composebox.
    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);
    input.focus();
    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);
    await getTransitionEndPromise(animatedElement, 'max-height');

    // Send suggestions to the composebox.
    const matches = [createSearchMatch({fillIntoEdit: 'match 1'})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({matches}));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(composebox);

    // Verify dropdown is not visible.
    assertFalse(isVisible(dropdown));
  });

  test('ShowsDropdownWhenEnabled', async () => {
    loadTimeData.overrideValues({
      enableAimSearchbox: true,
      enableLensAimSuggestions: true,
      composeboxShowZps: true,
    });
    const composebox = await setupTest();
    const dropdown =
        composebox.shadowRoot!.querySelector<HTMLElement>('[part=dropdown]');
    assertTrue(!!dropdown);


    // Focus input to expand composebox.
    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);
    input.focus();
    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);
    await getTransitionEndPromise(animatedElement, 'max-height');

    // Send suggestions to the composebox.
    const matches = [createSearchMatch({fillIntoEdit: 'match 1'})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({matches}));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(composebox);

    // Verify dropdown is not visible.
    assertTrue(isVisible(dropdown));
  });

  test('RendersSuggestionsAboveComposebox', async () => {
    loadTimeData.overrideValues({
      enableAimSearchbox: true,
      enableLensAimSuggestions: true,
      composeboxShowZps: true,
    });
    const composebox = await setupTest();
    const dropdown =
        composebox.shadowRoot!.querySelector<HTMLElement>('[part=dropdown]');
    assertTrue(!!dropdown);
    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);

    // Focus input to expand composebox and show dropdown.
    input.focus();
    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);
    await getTransitionEndPromise(animatedElement, 'max-height');

    // Send suggestions to the composebox.
    const matches = [createSearchMatch({fillIntoEdit: 'match 1'})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({matches}));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(composebox);

    // Verify dropdown is visible and above composebox.
    assertTrue(isVisible(dropdown));
    const composeboxRect = animatedElement.getBoundingClientRect();
    const dropdownRect = dropdown.getBoundingClientRect();
    assertTrue(dropdownRect.bottom <= composeboxRect.top);
  });

  test('DropdownHidesOnEmptySuggestions', async () => {
    loadTimeData.overrideValues({
      enableAimSearchbox: true,
      enableLensAimSuggestions: true,
      composeboxShowZps: true,
    });
    const composebox = await setupTest();
    const dropdown =
        composebox.shadowRoot!.querySelector<HTMLElement>('[part=dropdown]');
    assertTrue(!!dropdown);
    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);

    // Focus input to expand composebox.
    input.focus();
    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);
    await getTransitionEndPromise(animatedElement, 'max-height');

    // Send suggestions to the composebox and assert dropdown is visible.
    const matches = [createSearchMatch({fillIntoEdit: 'match 1'})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({matches}));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(composebox);
    assertTrue(isVisible(dropdown));

    // Send empty suggestions and assert dropdown is hidden.
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({matches: []}));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(composebox);
    assertFalse(isVisible(dropdown));
  });

  test('TabbingOrder', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();
    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);
    const submitContainer =
        composebox.shadowRoot!.querySelector<HTMLElement>('#submitContainer');
    const submitButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#submitIcon');
    const cancelButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#cancelIcon');
    assertTrue(!!submitContainer);
    assertTrue(!!submitButton);
    assertTrue(!!cancelButton);

    const getFocusableElements = () => {
      // This is a simplified focusable element query that is sufficient for
      // this test.
      return Array
          .from(composebox.shadowRoot!.querySelectorAll<HTMLElement>(
              'button, [href], input, select, textarea, [tabindex]'))
          .filter(el => {
            if (el.getAttribute('tabindex') === '-1' ||
                el.hasAttribute('disabled')) {
              return false;
            }
            return isTrulyVisible(el);
          });
    };

    input.focus();
    // Wait for expansion.
    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);
    await getTransitionEndPromise(animatedElement, 'max-height');

    // When no text is present, only the input should be focusable inside the
    // component. Tabbing should therefore move focus outside the component.
    let focusable = getFocusableElements();
    assertEquals(1, focusable.length);
    assertEquals(input, focusable[0]);

    // When text is present, tabbing follows order: input -> cancel -> submit.
    input.value = 'some text';
    input.dispatchEvent(new Event('input', {bubbles: true, composed: true}));
    await waitAfterNextRender(composebox);

    // The buttons get enabled after an animation. Wait for it.
    const cancelShowPromise = getTransitionEndPromise(cancelButton, 'opacity');
    const cancelContainerShowPromise =
        getTransitionEndPromise(cancelButton.parentElement!, 'opacity');
    const submitContainerShowPromise =
        getTransitionEndPromise(submitContainer, 'opacity');
    await Promise.all([
      cancelShowPromise,
      cancelContainerShowPromise,
      submitContainerShowPromise,
    ]);

    focusable = getFocusableElements();
    assertEquals(3, focusable.length);
    assertEquals(input, focusable[0]);
    assertEquals(cancelButton, focusable[1]);
    assertEquals(submitButton, focusable[2]);
  });

  test('KeyboardActions', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);
    const submitButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#submitIcon');
    const cancelButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#cancelIcon');
    assertTrue(!!submitButton);
    assertTrue(!!cancelButton);

    input.focus();
    // Wait for expansion.
    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);
    await getTransitionEndPromise(animatedElement, 'max-height');

    input.value = 'some text';
    input.dispatchEvent(new Event('input', {bubbles: true, composed: true}));
    await waitAfterNextRender(composebox);

    // Focusing the clear button and pressing enter should clear the searchbox.
    cancelButton.focus();
    cancelButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Enter',
      bubbles: true,
      composed: true,
    }));
    await waitAfterNextRender(composebox);
    assertEquals(input.value, '');

    // Focusing the submit button and pressing enter should submit the query.
    const query = 'some other text';
    input.value = query;
    input.dispatchEvent(new Event('input', {bubbles: true, composed: true}));
    await waitAfterNextRender(composebox);

    const matches = [createSearchMatch({
      fillIntoEdit: query,
      destinationUrl:
          {url: `https://www.google.com/search?q=${query.replace(/ /g, '+')}`},
      allowedToBeDefaultMatch: true,
    })];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          input: query,
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(composebox);

    submitButton.focus();
    submitButton.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Enter',
      bubbles: true,
      composed: true,
    }));
    const [matchIndex, url] =
        await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');
    assertEquals(matchIndex, 0);
    assertEquals(
        url.url, `https://www.google.com/search?q=${query.replace(/ /g, '+')}`);
  });

  test('SubmitButtonNoopWhenDisabled', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();

    const submitContainer =
        composebox.shadowRoot!.querySelector<HTMLElement>('#submitContainer');
    const submitButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#submitIcon');
    assertTrue(!!submitContainer);
    assertTrue(!!submitButton);

    // The button should be disabled initially with no input.
    assertTrue(submitButton.hasAttribute('disabled'));

    // Click the submit container.
    submitContainer.click();
    await waitAfterNextRender(composebox);

    // Verify that neither of the submit handlers were called.
    assertEquals(0, mockSearchboxPageHandler.getCallCount('submitQuery'));
    assertEquals(
        0, mockSearchboxPageHandler.getCallCount('openAutocompleteMatch'));
  });

  test('SelectingMatchPopulatesComposebox', async () => {
    loadTimeData.overrideValues({
      enableAimSearchbox: true,
      enableLensAimSuggestions: true,
    });
    const composebox = await setupTest();
    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);

    // Focus input to expand composebox.
    input.focus();
    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);
    await getTransitionEndPromise(animatedElement, 'max-height');

    // Send suggestions to the composebox.
    const matches = [
      createSearchMatch({fillIntoEdit: 'match 1'}),
      createSearchMatch({fillIntoEdit: 'match 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({matches}));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await waitAfterNextRender(composebox);

    // Pressing ArrowDown should select the first item and populate the input.
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await waitAfterNextRender(composebox);
    assertEquals(input.value, 'match 1');

    // Pressing ArrowDown again should select the second item.
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await waitAfterNextRender(composebox);
    assertEquals(input.value, 'match 2');

    // Pressing ArrowUp should select the first item again.
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowUp', bubbles: true, composed: true}));
    await waitAfterNextRender(composebox);
    assertEquals(input.value, 'match 1');
  });

  test('LensButtonClickNotifiesHandler', async () => {
    loadTimeData.overrideValues(
        {enableAimSearchbox: true, showLensButton: true});
    const composebox = await setupTest();

    const lensButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#lensIcon');
    assertTrue(!!lensButton);

    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);

    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);

    // The button should be visible.
    assertTrue(isTrulyVisible(lensButton));

    lensButton.click();
    await mockPageHandler.whenCalled('handleLensButtonClick');
    assertEquals(1, mockPageHandler.getCallCount('handleLensButtonClick'));
  });

  test('LensButtonDisabledChangesOnOverlayState', async () => {
    loadTimeData.overrideValues(
        {enableAimSearchbox: true, showLensButton: true});
    const composebox = await setupTest();

    const lensButton =
        composebox.shadowRoot!.querySelector<HTMLElement>('#lensIcon');
    assertTrue(!!lensButton);

    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);

    const animatedElement =
        composebox.shadowRoot!.querySelector<HTMLElement>('#composebox');
    assertTrue(!!animatedElement);

    // The button should be visible.
    assertTrue(isTrulyVisible(lensButton));

    // The Lens button is in an enabled state by default.
    assertFalse(lensButton.hasAttribute('disabled'));

    // Setting the overlay to not showing should make the button enabled.
    testBrowserProxy.page.setIsOverlayShowing(true);
    await waitAfterNextRender(lensSidePanelElement);

    assertTrue(lensButton.hasAttribute('disabled'));

    // Setting the overlay to showing should make the button disabled again.
    testBrowserProxy.page.setIsOverlayShowing(false);
    await waitAfterNextRender(lensSidePanelElement);

    assertFalse(lensButton.hasAttribute('disabled'));
  });

  test('FocusesComposeboxOnCallback', async () => {
    loadTimeData.overrideValues({enableAimSearchbox: true});
    const composebox = await setupTest();
    const input =
        composebox.shadowRoot!.querySelector<HTMLTextAreaElement>('textarea');
    assertTrue(!!input);

    // Make sure input is not focused initially.
    input.blur();
    assertNotEquals(input, composebox.shadowRoot!.activeElement);

    // Trigger the mojom callback to focus the composebox.
    testBrowserProxy.page.focusSearchbox();
    await waitAfterNextRender(composebox);

    // Verify the input is now focused.
    assertEquals(input, composebox.shadowRoot!.activeElement);
  });
});
