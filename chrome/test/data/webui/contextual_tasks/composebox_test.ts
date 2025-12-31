// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {AutocompleteMatch, AutocompleteResult} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

function pressEnter(element: HTMLElement) {
  element.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Enter',
    bubbles: true,
    composed: true,
  }));
}

function createAutocompleteMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  const base: AutocompleteMatch = {
    allowedToBeDefaultMatch: false,
    isSearchType: true,
    contents: 'a suggestion',
    destinationUrl: {url: `https://google.com/search?q=a+suggestion`},
    fillIntoEdit: 'a suggestion',
    type: 'search-suggest',

    // Add all other fields needed to satisfy the AutocompleteMatch type
    isHidden: false,
    a11yLabel: '',
    actions: [],
    isEnterpriseSearchAggregatorPeopleType: false,
    swapContentsAndDescription: false,
    supportsDeletion: false,
    suggestionGroupId: -1,
    contentsClass: [{offset: 0, style: 0}],
    description: '',
    descriptionClass: [{offset: 0, style: 0}],
    inlineAutocompletion: '',
    iconPath: '',
    iconUrl: {url: ''},
    imageDominantColor: '',
    imageUrl: '',
    isNoncannedAimSuggestion: false,
    removeButtonA11yLabel: '',
    isRichSuggestion: false,
    isWeatherAnswerSuggestion: null,
    answer: null,
    tailSuggestCommonPrefix: null,
    hasInstantKeyword: false,
    keywordChipHint: '',
    keywordChipA11y: '',
  } as AutocompleteMatch;

  return Object.assign(base, modifiers);
}

function createAutocompleteResult(modifiers: Partial<AutocompleteResult> = {}):
    AutocompleteResult {
  const base: AutocompleteResult = {
    input: '',
    matches: [],
    suggestionGroupsMap: {},
    smartComposeInlineHint: null,
  };

  return Object.assign(base, modifiers);
}

function simulateUserInput(inputElement: HTMLInputElement, value: string) {
  inputElement.value = value;
  inputElement.dispatchEvent(
      new Event('input', {bubbles: true, composed: true}));
}

async function setupAutocompleteResults(
    searchboxCallbackRouterRemote: SearchboxPageRemote, testQuery: string) {
  const matches = [
    createAutocompleteMatch({
      allowedToBeDefaultMatch: true,
      contents: testQuery,
      destinationUrl: {url: `https://google.com/search?q=${testQuery}`},
      type: 'search-what-you-typed',
      fillIntoEdit: testQuery,
    }),
    createAutocompleteMatch(),
  ];
  searchboxCallbackRouterRemote.autocompleteResultChanged(
      createAutocompleteResult({
        input: testQuery,
        matches: matches,
      }));
  await microtasksFinished();
}

suite('ContextualTasksComposeboxTest', () => {
  let contextualTasksApp: ContextualTasksAppElement;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestContextualTasksBrowserProxy('https://google.com');
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler as any, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler as any, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);
    await microtasksFinished();
  });

  test('ComposeboxSubmitSendsQueryAndClearsInput', async () => {
    const TEST_QUERY = 'test query';

    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const inputElement = composebox.$.input;
    assertTrue(
        isVisible(inputElement), 'Composebox input element should be visible');

    simulateUserInput(inputElement, TEST_QUERY);
    await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

    await setupAutocompleteResults(searchboxCallbackRouterRemote, TEST_QUERY);

    pressEnter(inputElement);

    const [matchIndex, url] =
        await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');
    assertEquals(0, matchIndex);
    assertEquals(`https://google.com/search?q=${TEST_QUERY}`, url.url);
    await microtasksFinished();

    assertEquals(
        '', inputElement.value,
        'Input should be cleared, but input = ' + inputElement.value);
  });

  test('LensButtonTriggersOverlay', async () => {
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    contextualTasksApp.$.composebox.isSidePanel = true;
    await microtasksFinished();

    assertTrue(composebox.lensButtonTriggersOverlay);

    const lensButton = composebox.shadowRoot.querySelector('#lensIcon');
    assertTrue(
        lensButton instanceof HTMLElement,
        'Lens button should be an HTMLElement');
    lensButton.click();

    await mockComposeboxPageHandler.whenCalled('handleLensButtonClick');
    assertEquals(
        1, mockComposeboxPageHandler.getCallCount('handleLensButtonClick'));
  });
});
