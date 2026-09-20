// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import './test_composebox_mixin.js';

import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {VoiceSearchAction} from 'chrome://resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import {InputMethod, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SuggestStyle} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch, AutocompleteResult, PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle, installMock, MockInputState, setSelectionOffset, simulateUserTextInput} from './composebox_test_utils.js';
import type {TestComposeboxMixinElement} from './test_composebox_mixin.js';

enum Attributes {
  SELECTED = 'selected',
}

const FAKE_TOKEN_STRING = '00000000000000001234567890ABCDEF';

function setInputValue(inputElement: HTMLElement, value: string) {
  if (inputElement instanceof HTMLTextAreaElement ||
      inputElement instanceof HTMLInputElement) {
    inputElement.value = value;
  } else {
    inputElement.innerText = value;
  }
}

function getInputValue(inputElement: HTMLElement): string {
  if (inputElement instanceof HTMLTextAreaElement ||
      inputElement instanceof HTMLInputElement) {
    return inputElement.value;
  }
  return inputElement.innerText;
}

async function areMatchesShowing(
    element: TestComposeboxMixinElement,
    callbackRouter: SearchboxPageRemote): Promise<boolean> {
  await callbackRouter.$.flushForTesting();
  await microtasksFinished();
  return window.getComputedStyle(element.$.matches).display !== 'none';
}

suite('ComposeboxAutocomplete', () => {
  let element: TestComposeboxMixinElement;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    fakeMetricsPrivate();

    const callbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new SearchboxPageHandlerRemote(), callbackRouter)));
    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setPromiseResolveFor('getInputState', {
      state: new MockInputState(),
    });
    searchboxHandler.setPromiseResolveFor('getPageClassification', {
      metricSource: 'NTP_COMPOSEBOX',
    });
  });

  function createTestElement(
      properties: Partial<TestComposeboxMixinElement> = {}):
      TestComposeboxMixinElement {
    const el = document.createElement('test-composebox-mixin');
    Object.assign(el, properties);
    document.body.appendChild(el);
    el.focusInput();
    return el;
  }

  suite('Dropdown', () => {
    test('dropdown shows when suggestions enabled', async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      setInputValue(element.getInputElement().inputElement, '');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const composeboxDropdown = element.$.matches;
      assertTrue(composeboxDropdown.hidden);

      const matches = [
        createSearchMatchForTesting(),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            matches,
          }));
      await microtasksFinished();

      assertFalse(composeboxDropdown.hidden);
    });

    test('dropdown does not show for multiline input', async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      setInputValue(element.getInputElement().inputElement, 'Test');
      element.getInputElement().inputElement.style.height = '64px';
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const composeboxDropdown = element.$.matches;
      const matches = [
        createSearchMatchForTesting(),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            matches,
          }));
      await microtasksFinished();

      assertTrue(composeboxDropdown.hidden);

      const arrowDownEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'ArrowDown',
      });

      element.getInputElement().inputElement.dispatchEvent(arrowDownEvent);
      await microtasksFinished();
      assertFalse(arrowDownEvent.defaultPrevented);
    });

    test('dropdown does not show with multiple context files', async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      setInputValue(element.getInputElement().inputElement, '');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const composeboxDropdown = element.$.matches;
      const matches = [
        createSearchMatchForTesting(),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            matches,
          }));
      await microtasksFinished();
      assertFalse(composeboxDropdown.hidden);

      element.addFileContextForTesting({
        uuid: FAKE_TOKEN_STRING,
        name: 'foo.jpg',
        status: 0,
        type: 'image/jpeg',
        inputType: InputType.kLensFile,
        isDeletable: true,
        objectUrl: null,
        dataUrl: null,
        url: null,
        tabId: null,
        iconName: null,
        supportsUnimodal: true,
      });
      element.addFileContextForTesting({
        uuid: FAKE_TOKEN_STRING + '2',
        name: 'foo2.jpg',
        status: 0,
        type: 'image/jpeg',
        inputType: InputType.kLensFile,
        isDeletable: true,
        objectUrl: null,
        dataUrl: null,
        url: null,
        tabId: null,
        iconName: null,
        supportsUnimodal: true,
      });
      await microtasksFinished();
      assertTrue(composeboxDropdown.hidden);
    });

    test(
        'dropdown does not show when no typed suggestions enabled',
        async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: false});
      element = createTestElement();
      await microtasksFinished();

      setInputValue(element.getInputElement().inputElement, '');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const composeboxDropdown = element.$.matches;
      const matches = [
        createSearchMatchForTesting(),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            matches,
          }));
      await microtasksFinished();
      assertFalse(composeboxDropdown.hidden);

      setInputValue(element.getInputElement().inputElement, 'Hello');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      assertTrue(composeboxDropdown.hidden);
    });

    test('dropdown does not show for typed suggest with context', async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      setInputValue(element.getInputElement().inputElement, 'Test');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const composeboxDropdown = element.$.matches;
      const matches = [
        createSearchMatchForTesting({
          fillIntoEdit: 'hello world 1',
          allowedToBeDefaultMatch: true,
        }),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 3'}),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 4'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            matches,
            input: 'Test',
          }));
      await microtasksFinished();
      assertFalse(composeboxDropdown.hidden);

      element.addFileContextForTesting({
        uuid: FAKE_TOKEN_STRING,
        name: 'foo.jpg',
        status: 0,
        type: 'image/jpeg',
        inputType: InputType.kLensFile,
        isDeletable: true,
        objectUrl: null,
        dataUrl: null,
        url: null,
        tabId: null,
        iconName: null,
        supportsUnimodal: true,
      });
      await microtasksFinished();
      assertTrue(composeboxDropdown.hidden);
    });

    test(
        'dropdown does not show for typed suggest with verbatim match only',
        async () => {
          loadTimeData.overrideValues(
              {composeboxShowZps: true, composeboxShowTypedSuggest: true});
          element = createTestElement();
          await microtasksFinished();

          setInputValue(element.getInputElement().inputElement, 'Test');
          element.getInputElement().inputElement.dispatchEvent(
              new Event('input'));
          await microtasksFinished();

          const composeboxDropdown = element.$.matches;
          const matches = [createSearchMatchForTesting()];
          searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                queryId: element.activeQueryId,
                matches,
                input: 'Test',
              }));
          await microtasksFinished();

          assertTrue(composeboxDropdown.hidden);
        });

    test('composebox does not show verbatim match', async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      // Add zps input.
      setInputValue(element.getInputElement().inputElement, '');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const matches = [
        createSearchMatchForTesting(),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            matches,
          }));
      await microtasksFinished();
      assertTrue(
          await areMatchesShowing(element, searchboxCallbackRouterRemote));

      let matchEls =
          element.$.matches.shadowRoot.querySelectorAll('cr-composebox-match');
      assertEquals(2, matchEls.length);
      let matchEl = matchEls[0];
      assertTrue(!!matchEl);
      // First match shows for zps.
      assertStyle(matchEl, 'display', 'block');

      // Add typed input.
      setInputValue(element.getInputElement().inputElement, 'awesome');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const typedMatches = [
        createSearchMatchForTesting({allowedToBeDefaultMatch: true}),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: 'awesome',
            matches: typedMatches,
          }));
      await microtasksFinished();
      assertTrue(
          await areMatchesShowing(element, searchboxCallbackRouterRemote));

      matchEls =
          element.$.matches.shadowRoot.querySelectorAll('cr-composebox-match');
      assertEquals(2, matchEls.length);
      matchEl = matchEls[0];
      assertTrue(!!matchEl);
      // Verbatim match does not show for typed suggest.
      assertStyle(matchEl, 'display', 'none');
    });

    test('dropdown does not flash after clicking ZPS suggestion', async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      element.input = '';
      element.showZps = true;

      const matches = [
        createSearchMatchForTesting({
          contents: 'zps suggestion',
          destinationUrl: 'https://google.com/',
        }),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: '',
            matches,
          }));
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      const composeboxDropdown = element.$.matches;
      assertFalse(composeboxDropdown.hidden);
      assertTrue(!!element.result);

      composeboxDropdown.dispatchEvent(new CustomEvent(
          'match-click',
          {detail: {ctrlKey: false, metaKey: false, shiftKey: false}}));
      await microtasksFinished();

      assertTrue(composeboxDropdown.hidden);
      assertEquals(null, element.result);

      element.getInputElement().dispatchEvent(new CustomEvent('input-focusin'));

      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: '',
            matches,
          }));
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      assertTrue(composeboxDropdown.hidden);
      assertEquals(null, element.result);
    });
  });

  suite('KeyboardNavigation', () => {
    test('escape key behavior with suggestions', async () => {
      loadTimeData.overrideValues({composeboxShowZps: true});
      element = createTestElement();
      await microtasksFinished();

      const matches = [
        createSearchMatchForTesting(),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting(
              {queryId: element.activeQueryId, matches}));
      await microtasksFinished();
      assertTrue(
          await areMatchesShowing(element, searchboxCallbackRouterRemote));

      element.closeOnEscape = false;
      const closePromise = eventToPromise('close-composebox', element);
      let closed = false;
      closePromise.then(() => closed = true);

      setInputValue(element.getInputElement().inputElement, 'test');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      element.getInputElement().inputElement.dispatchEvent(
          new KeyboardEvent(
              'keydown', {key: 'Escape', bubbles: true, composed: true}));
      await microtasksFinished();

      assertEquals(searchboxHandler.getCallCount('clearFiles'), 1);
      assertFalse(closed);
      assertEquals('', getInputValue(element.getInputElement().inputElement));

      element.closeOnEscape = true;
      const whenCloseComposebox = eventToPromise('close-composebox', element);
      element.getInputElement().inputElement.dispatchEvent(
          new KeyboardEvent(
              'keydown', {key: 'Escape', bubbles: true, composed: true}));
      await whenCloseComposebox;
      assertEquals(searchboxHandler.getCallCount('clearFiles'), 2);
    });

    test('arrow up/down moves selection / focus', async () => {
      loadTimeData.overrideValues({composeboxShowZps: true});
      element = createTestElement();
      await microtasksFinished();

      // When no matches are present, ArrowDown does not select anything.
      element.getInputElement().inputElement.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(-1, element.$.matches.selectedMatchIndex);

      setInputValue(element.getInputElement().inputElement, '');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));

      const matches = [
        createSearchMatchForTesting(),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            matches,
          }));

      assertTrue(
          await areMatchesShowing(element, searchboxCallbackRouterRemote));

      const matchEls =
          element.$.matches.shadowRoot.querySelectorAll('cr-composebox-match');
      assertEquals(2, matchEls.length);

      const arrowDownEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'ArrowDown',
      });

      element.getInputElement().inputElement.dispatchEvent(arrowDownEvent);
      await microtasksFinished();
      assertTrue(arrowDownEvent.defaultPrevented);

      assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
      assertEquals(0, element.$.matches.selectedMatchIndex);
      assertEquals(
          'hello world', getInputValue(element.getInputElement().inputElement));

      matchEls[1]!.focus();
      matchEls[1]!.dispatchEvent(new Event('focusin', {
        bubbles: true,
        cancelable: true,
        composed: true,
      }));
      await microtasksFinished();

      assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
      assertEquals(1, element.$.matches.selectedMatchIndex);
      assertEquals(
          'hello world 2',
          getInputValue(element.getInputElement().inputElement));
      assertEquals(matchEls[1]!, element.$.matches.shadowRoot.activeElement);

      const arrowUpEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'ArrowUp',
      });

      matchEls[1]!.dispatchEvent(arrowUpEvent);
      await microtasksFinished();
      assertTrue(arrowUpEvent.defaultPrevented);

      assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
      assertEquals(0, element.$.matches.selectedMatchIndex);
      assertEquals(
          'hello world', getInputValue(element.getInputElement().inputElement));
      assertEquals(matchEls[0]!, element.$.matches.shadowRoot.activeElement);

      // Key modifiers (e.g. Ctrl) ignore arrow navigation.
      element.getInputElement().inputElement.dispatchEvent(
          new KeyboardEvent('keydown', {
            key: 'ArrowDown',
            ctrlKey: true,
            bubbles: true,
            composed: true,
          }));
      await microtasksFinished();
      assertEquals(0, element.$.matches.selectedMatchIndex);

      // Arrow navigation is ignored when dropdown is not needed.
      element.dropdownNeeded = false;
      element.getInputElement().inputElement.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(0, element.$.matches.selectedMatchIndex);

      loadTimeData.overrideValues({composeboxShowZps: false});
    });

    test('arrow keys work for typed suggest', async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      // Add typed input.
      setInputValue(element.getInputElement().inputElement, 'Test');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const composeboxDropdown = element.$.matches;

      const matches = [
        createSearchMatchForTesting(
            {fillIntoEdit: 'hello world 1', allowedToBeDefaultMatch: true}),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 3'}),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 4'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            matches: matches,
            input: 'Test',
          }));
      await microtasksFinished();

      // Dropdown should show when matches are available.
      assertFalse(composeboxDropdown.hidden);

      const matchEls =
          element.$.matches.shadowRoot.querySelectorAll('cr-composebox-match');
      assertEquals(4, matchEls.length);
      const matchEl = matchEls[0];
      assertTrue(!!matchEl);
      // Verbatim match does not show for typed suggest.
      assertStyle(matchEl, 'display', 'none');

      // Arrow down should do default action.
      const arrowDownEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'ArrowDown',
      });

      element.getInputElement().inputElement.dispatchEvent(arrowDownEvent);
      await microtasksFinished();
      assertTrue(arrowDownEvent.defaultPrevented);

      // First SHOWN match (second match) is selected.
      assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
      assertEquals(
          'hello world 2',
          getInputValue(element.getInputElement().inputElement));

      // Arrow down navigates forward to next suggestion.
      element.getInputElement().inputElement.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertTrue(matchEls[2]!.hasAttribute(Attributes.SELECTED));
      assertEquals(
          'hello world 3',
          getInputValue(element.getInputElement().inputElement));

      // Arrow up navigates backwards to previous suggestion.
      element.getInputElement().inputElement.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowUp', bubbles: true, composed: true}));
      await microtasksFinished();
      assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
      assertEquals(
          'hello world 2',
          getInputValue(element.getInputElement().inputElement));

      // Arrow up should do default action.
      const arrowUpEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'ArrowUp',
      });

      element.getInputElement().inputElement.dispatchEvent(arrowUpEvent);
      await microtasksFinished();
      assertTrue(arrowUpEvent.defaultPrevented);
      // Last match gets selected when arrowing up from the first shown match.
      assertTrue(matchEls[3]!.hasAttribute(Attributes.SELECTED));
      assertEquals(
          'hello world 4',
          getInputValue(element.getInputElement().inputElement));

      // When arrowing down from last match, first SHOWN match should be
      // selected.
      const secondArrowDownEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'ArrowDown',
      });
      element.getInputElement().inputElement.dispatchEvent(
          secondArrowDownEvent);
      await microtasksFinished();
      assertTrue(secondArrowDownEvent.defaultPrevented);
      assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
      assertEquals(
          'hello world 2',
          getInputValue(element.getInputElement().inputElement));
    });

    test(
        'does not fill suggestion into input on keyboard navigation for ' +
            'image suggestions',
        async () => {
          element = createTestElement();
          element.richImageSuggestionsEnabled = true;
          const inputElem = element.getInputElement();
          const input = inputElem.inputElement;
          const matchesElement = element.$.matches;

          element.input = 'cat';
          element.lastQueriedInput = 'cat';

          const matches = [
            createSearchMatchForTesting({
              fillIntoEdit: 'cat',
              allowedToBeDefaultMatch: true,
            }),
            createSearchMatchForTesting({
              fillIntoEdit: 'caterpillar',
              suggestStyle: SuggestStyle.kDefault,
            }),
            createSearchMatchForTesting({
              fillIntoEdit: 'cat image prompt',
              suggestStyle: SuggestStyle.kRichImage,
            }),
          ];
          searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                input: 'cat',
                matches,
                queryId: element.activeQueryId,
              }));
          await microtasksFinished();

          // ArrowDown to text suggestion -> input fills with fillIntoEdit.
          input.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
          await microtasksFinished();
          assertEquals(1, matchesElement.selectedMatchIndex);
          assertEquals('caterpillar', element.input);

          // ArrowDown to rich image suggestion -> input reverts to original
          // query.
          input.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
          await microtasksFinished();
          assertEquals(2, matchesElement.selectedMatchIndex);
          assertEquals('cat', element.input);

          // ArrowUp back to text suggestion -> input fills with fillIntoEdit.
          input.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'ArrowUp', bubbles: true, composed: true}));
          await microtasksFinished();
          assertEquals(1, matchesElement.selectedMatchIndex);
          assertEquals('caterpillar', element.input);
        });

    test(
        'arrow up/down enables submit for suggestion with no query',
        async () => {
          loadTimeData.overrideValues({composeboxShowZps: true});
          element = createTestElement({searchboxNextEnabled: true});
          await microtasksFinished();

          setInputValue(element.getInputElement().inputElement, '');
          element.getInputElement().inputElement.dispatchEvent(
              new Event('input'));

          const matches = [
            createSearchMatchForTesting({fillIntoEdit: ''}),
          ];
          searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                queryId: element.activeQueryId,
                matches,
              }));

          assertTrue(
              await areMatchesShowing(element, searchboxCallbackRouterRemote));

          const matchEls = element.$.matches.shadowRoot.querySelectorAll(
              'cr-composebox-match');
          assertEquals(1, matchEls.length);

          const arrowDownEvent = new KeyboardEvent('keydown', {
            bubbles: true,
            cancelable: true,
            composed: true,
            key: 'ArrowDown',
          });

          element.getInputElement().inputElement.dispatchEvent(arrowDownEvent);
          await microtasksFinished();
          assertTrue(arrowDownEvent.defaultPrevented);

          assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
          assertEquals(
              '', getInputValue(element.getInputElement().inputElement));

          assertFalse(element.$.submit.$.submitIcon.hasAttribute('disabled'));

          const keydownEvent = new KeyboardEvent('keydown', {
            bubbles: true,
            cancelable: true,
            composed: true,
            key: 'Enter',
          });
          matchEls[0]!.dispatchEvent(keydownEvent);
          assertTrue(keydownEvent.defaultPrevented);

          await microtasksFinished();

          assertEquals(
              searchboxHandler.getCallCount('openAutocompleteMatch'), 1);

          loadTimeData.overrideValues({composeboxShowZps: false});
        });

    test(
        'Shift+Enter submits dropdown selection when focus is in dropdown',
        async () => {
          element = createTestElement();
          await microtasksFinished();

          const event = new KeyboardEvent('keydown', {
            key: 'Enter',
            shiftKey: true,
            bubbles: true,
            cancelable: true,
          });

          element.setActiveElement(element.getDropdownElement());

          element.getWrapperElement().dispatchEvent(event);
          await microtasksFinished();

          assertTrue(event.defaultPrevented);
        });

    test('selects first or last match with PageUp and PageDown', async () => {
      element = createTestElement();
      await microtasksFinished();

      const input = element.getInputElement().inputElement;
      const matchesElement = element.getDropdownElement();

      const matches = [
        {fillIntoEdit: 'test1'} as AutocompleteMatch,
        {fillIntoEdit: 'test2'} as AutocompleteMatch,
        {fillIntoEdit: 'test3'} as AutocompleteMatch,
      ];
      element.result = {input: 'test', matches} as AutocompleteResult;
      await microtasksFinished();

      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'PageDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(2, matchesElement.selectedMatchIndex);

      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'PageUp', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(0, matchesElement.selectedMatchIndex);

      input.dispatchEvent(new KeyboardEvent('keydown', {
        key: 'PageDown',
        altKey: true,
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();
      assertEquals(0, matchesElement.selectedMatchIndex);
    });

    test(
        'PageDown and PageUp are ignored when no matches are available',
        async () => {
          element = createTestElement();
          await microtasksFinished();

          const input = element.getInputElement().inputElement;
          const matchesElement = element.getDropdownElement();

          input.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'PageDown', bubbles: true, composed: true}));
          await microtasksFinished();
          assertEquals(-1, matchesElement.selectedMatchIndex);
        });

    test('Tab behavior when focus is in input', async () => {
      element = createTestElement({smartComposeEnabled: true});
      await microtasksFinished();

      const inputElem = element.getInputElement();
      const input = inputElem.inputElement;
      const matchesElement = element.getDropdownElement();

      const matches = [{fillIntoEdit: 'match1'} as AutocompleteMatch];
      element.result = {input: 'tes', matches} as AutocompleteResult;
      await microtasksFinished();

      matchesElement.selectNext();
      assertEquals(0, matchesElement.selectedMatchIndex);
      input.focus();

      input.dispatchEvent(new KeyboardEvent(
          'keydown',
          {key: 'Tab', shiftKey: true, bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(-1, matchesElement.selectedMatchIndex);

      await simulateUserTextInput(inputElem, 'tes');
      element.smartComposeInlineHint = 't';
      await microtasksFinished();

      const tabEvent = new KeyboardEvent(
          'keydown',
          {key: 'Tab', bubbles: true, cancelable: true, composed: true});
      input.dispatchEvent(tabEvent);
      await microtasksFinished();

      assertEquals('test', (input as HTMLTextAreaElement).value);
      assertTrue(tabEvent.defaultPrevented);
    });

    test('Tab on last dropdown match unselects active match', async () => {
      element = createTestElement();
      await microtasksFinished();

      const matchesElement = element.getDropdownElement();
      const matches = [
        {fillIntoEdit: 'match1', supportsDeletion: false} as AutocompleteMatch,
        {fillIntoEdit: 'match2', supportsDeletion: false} as AutocompleteMatch,
      ];
      element.result = {input: 'm', matches} as AutocompleteResult;
      await microtasksFinished();

      matchesElement.selectNext();
      matchesElement.selectNext();
      assertEquals(1, matchesElement.selectedMatchIndex);

      await microtasksFinished();
      element.setActiveElement(matchesElement);

      const tabEvent = new KeyboardEvent(
          'keydown',
          {key: 'Tab', bubbles: true, cancelable: true, composed: true});
      matchesElement.dispatchEvent(tabEvent);
      await microtasksFinished();

      assertEquals(-1, matchesElement.selectedMatchIndex);
      assertFalse(tabEvent.defaultPrevented);
    });

    test(
        'Tab in dropdown is ignored when key modifiers are active',
        async () => {
          element = createTestElement();
          await microtasksFinished();

          const matchesElement = element.getDropdownElement();
          const matches = [
            {fillIntoEdit: 'match1', supportsDeletion: false} as
                AutocompleteMatch,
            {fillIntoEdit: 'match2', supportsDeletion: false} as
                AutocompleteMatch,
          ];
          element.result = {input: 'm', matches} as AutocompleteResult;
          await microtasksFinished();

          matchesElement.selectNext();
          matchesElement.selectNext();
          await microtasksFinished();
          element.setActiveElement(matchesElement);
          const tabEventCtrl = new KeyboardEvent('keydown', {
            key: 'Tab',
            ctrlKey: true,
            bubbles: true,
            cancelable: true,
          });
          matchesElement.dispatchEvent(tabEventCtrl);
          await microtasksFinished();
          assertEquals(1, matchesElement.selectedMatchIndex);
        });

    test(
        'Tab in dropdown is ignored when no matches are available',
        async () => {
          element = createTestElement();
          await microtasksFinished();

          const matchesElement = element.getDropdownElement();

          const tabEventNoMatch = new KeyboardEvent('keydown', {
            key: 'Tab',
            bubbles: true,
            cancelable: true,
          });
          matchesElement.dispatchEvent(tabEventNoMatch);
          await microtasksFinished();
          assertEquals(-1, matchesElement.selectedMatchIndex);
        });
  });

  suite('MatchRemoval', () => {
    test('Selection is restored after selected match is removed', async () => {
      loadTimeData.overrideValues(
          {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      setInputValue(element.getInputElement().inputElement, '');
      element.getInputElement().inputElement.dispatchEvent(
          new InputEvent('input'));

      let matches = [
        createSearchMatchForTesting({
          supportsDeletion: true,
        }),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: getInputValue(element.getInputElement().inputElement)
                       .trimStart(),
            matches,
          }));
      await microtasksFinished();
      assertTrue(
          await areMatchesShowing(element, searchboxCallbackRouterRemote));

      let matchEls =
          element.$.matches.shadowRoot.querySelectorAll('cr-composebox-match');
      assertEquals(1, matchEls.length);
      assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

      matchEls[0]!.$.remove.click();
      const args =
          await searchboxHandler.whenCalled('deleteAutocompleteMatch');
      assertEquals(0, args[0]);
      assertEquals(1, searchboxHandler.getCallCount('deleteAutocompleteMatch'));
      assertEquals(0, searchboxHandler.getCallCount('submitQuery'));

      searchboxHandler.reset();

      matches = [
        createSearchMatchForTesting({supportsDeletion: true}),
        createSearchMatchForTesting({
          supportsDeletion: true,
          fillIntoEdit: 'hello world 2',
        }),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: '',
            matches,
          }));
      await microtasksFinished();
      assertTrue(
          await areMatchesShowing(element, searchboxCallbackRouterRemote));

      matchEls =
          element.$.matches.shadowRoot.querySelectorAll('cr-composebox-match');
      assertEquals(2, matchEls.length);
      assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

      const arrowDownEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'ArrowDown',
      });

      element.getInputElement().inputElement.dispatchEvent(arrowDownEvent);
      await microtasksFinished();
      assertTrue(arrowDownEvent.defaultPrevented);

      assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
      assertEquals(
          'hello world', getInputValue(element.getInputElement().inputElement));

      const keydownEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'Enter',
      });
      matchEls[0]!.$.remove.dispatchEvent(keydownEvent);
      assertTrue(keydownEvent.defaultPrevented);
      const keydownArgs =
          await searchboxHandler.whenCalled('deleteAutocompleteMatch');
      await microtasksFinished();
      assertEquals(0, keydownArgs[0]);
      assertEquals(1, searchboxHandler.getCallCount('deleteAutocompleteMatch'));
      assertEquals(0, searchboxHandler.getCallCount('submitQuery'));

      matches = [createSearchMatchForTesting({
        supportsDeletion: true,
        fillIntoEdit: 'hello world 2',
      })];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: '',
            matches,
          }));
      await microtasksFinished();
      assertTrue(
          await areMatchesShowing(element, searchboxCallbackRouterRemote));

      assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
      assertEquals(
          'hello world 2',
          getInputValue(element.getInputElement().inputElement));
    });
  });

  suite('SmartCompose', () => {
    test('smart compose response added', async () => {
      element = createTestElement();
      await microtasksFinished();

      setInputValue(element.getInputElement().inputElement, 'smart ');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));

      element.haveReceivedSynchronousAutocompleteResponse = true;
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: 'smart ',
            matches: [],
            smartComposeInlineHint: 'compose',
          }));
      await microtasksFinished();

      assertEquals('compose', element.smartComposeInlineHint);
    });

    test('tab adds smart compose to input', async () => {
      element = createTestElement();
      await microtasksFinished();
      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);

      setInputValue(element.getInputElement().inputElement, 'smart ');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 2);

      element.haveReceivedSynchronousAutocompleteResponse = true;
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: 'smart ',
            matches: [],
            smartComposeInlineHint: 'compose',
          }));
      await microtasksFinished();

      const tabEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'Tab',
      });

      element.getInputElement().inputElement.dispatchEvent(tabEvent);
      await microtasksFinished();
      assertTrue(tabEvent.defaultPrevented);

      assertEquals(
          'smart compose',
          getInputValue(element.getInputElement().inputElement));
      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 3);
    });

    test('arrow up/down clears smart compose', async () => {
      loadTimeData.overrideValues({composeboxShowTypedSuggest: true});
      element = createTestElement();
      await microtasksFinished();

      const matches = [
        createSearchMatchForTesting(),
        createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      ];

      setInputValue(element.getInputElement().inputElement, 'awesome');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      element.haveReceivedSynchronousAutocompleteResponse = true;
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            queryId: element.activeQueryId,
            input: 'awesome',
            matches,
            smartComposeInlineHint: 'compose',
          }));
      assertTrue(
          await areMatchesShowing(element, searchboxCallbackRouterRemote));

      const smartCompose =
          $$<HTMLElement>(element.getInputElement(), '#smartCompose');
      assertTrue(!!smartCompose);

      const arrowDownEvent = new KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        composed: true,
        key: 'ArrowDown',
      });

      element.getInputElement().inputElement.dispatchEvent(arrowDownEvent);
      await microtasksFinished();
      assertTrue(arrowDownEvent.defaultPrevented);

      assertFalse(
          !!$$<HTMLElement>(element.getInputElement(), '#smartCompose'));
    });

    test('smartComposeInlineHint is sliced on sequential typing', async () => {
      element = createTestElement({smartComposeEnabled: true});
      await microtasksFinished();

      element.input = 'hello';
      element.smartComposeInlineHint = ' world';
      await microtasksFinished();

      const inputElem = element.getInputElement();
      await simulateUserTextInput(inputElem, 'hello ');

      assertEquals('world', element.smartComposeInlineHint);
      assertEquals('hello ', element.input);

      await simulateUserTextInput(inputElem, 'hello w');

      assertEquals('orld', element.smartComposeInlineHint);
    });

    test(
        'smartComposeInlineHint is cleared on non-matching typing',
        async () => {
          element = createTestElement({smartComposeEnabled: true});
          await microtasksFinished();

          element.input = 'hello';
          element.smartComposeInlineHint = ' world';
          await microtasksFinished();

          const inputElem = element.getInputElement();
          await simulateUserTextInput(inputElem, 'hello!');

          assertEquals('', element.smartComposeInlineHint);
        });

    test('Smart Compose hint is hidden during backspacing', async () => {
      element = createTestElement({smartComposeEnabled: true});
      await microtasksFinished();

      const inputElem = element.getInputElement();
      const input = inputElem.inputElement;

      await simulateUserTextInput(inputElem, 'tes');
      element.smartComposeInlineHint = 't';
      await microtasksFinished();

      assertTrue(!!inputElem.shadowRoot.querySelector('#smartCompose'));

      input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Backspace'}));
      await microtasksFinished();

      assertFalse(!!inputElem.shadowRoot.querySelector('#smartCompose'));
    });

    test('Smart Compose hint is hidden when cursor is not at end', async () => {
      element = createTestElement({smartComposeEnabled: true});
      await microtasksFinished();

      const inputElem = element.getInputElement();

      await simulateUserTextInput(inputElem, 'test');
      element.smartComposeInlineHint = 'a';
      await microtasksFinished();

      assertTrue(!!inputElem.shadowRoot.querySelector('#smartCompose'));

      inputElem.inputElement.focus();
      setSelectionOffset(inputElem.inputElement, 1);
      inputElem.requestUpdate();
      await microtasksFinished();

      assertFalse(!!inputElem.shadowRoot.querySelector('#smartCompose'));
    });

    test(
        'Smart Compose hint is hidden when it wraps in the middle of a word',
        async () => {
          element = createTestElement({smartComposeEnabled: true});
          await microtasksFinished();

          const inputElement = element.getInputElement();
          const input = inputElement.inputElement as HTMLTextAreaElement;

          const originalMeasureText =
              CanvasRenderingContext2D.prototype.measureText;
          try {
            CanvasRenderingContext2D.prototype.measureText = function(
                text: string) {
              if (text.includes('wrap')) {
                return {width: 150} as TextMetrics;
              }
              return {width: 50} as TextMetrics;
            };
            Object.defineProperty(
                input, 'clientWidth', {configurable: true, get: () => 100});

            await simulateUserTextInput(inputElement, 'tes.');
            element.smartComposeInlineHint = 'wrap';
            await microtasksFinished();

            assertFalse(
                !!inputElement.shadowRoot.querySelector('#smartCompose'));
          } finally {
            CanvasRenderingContext2D.prototype.measureText =
                originalMeasureText;
          }
        });

    test(
        'Smart Compose hint is NOT hidden when only full hint wraps but first word fits',
        async () => {
          element = createTestElement({smartComposeEnabled: true});
          await microtasksFinished();

          const inputElement = element.getInputElement();
          const input = inputElement.inputElement as HTMLTextAreaElement;

          const originalMeasureText =
              CanvasRenderingContext2D.prototype.measureText;
          try {
            CanvasRenderingContext2D.prototype.measureText = function(
                text: string) {
              if (text.includes('wraps')) {
                return {width: 150} as TextMetrics;
              }
              return {width: 50} as TextMetrics;
            };
            Object.defineProperty(
                input, 'clientWidth', {configurable: true, get: () => 100});

            await simulateUserTextInput(inputElement, 'tes.');
            element.smartComposeInlineHint = 'fits wraps';
            await microtasksFinished();

            assertTrue(
                !!inputElement.shadowRoot.querySelector('#smartCompose'));
          } finally {
            CanvasRenderingContext2D.prototype.measureText =
                originalMeasureText;
          }
        });

    test(
        'Tab key does not accept Smart Compose when hidden by wrapping',
        async () => {
          element = createTestElement({smartComposeEnabled: true});
          await microtasksFinished();

          const inputElement = element.getInputElement();
          const input = inputElement.inputElement as HTMLTextAreaElement;

          const originalMeasureText =
              CanvasRenderingContext2D.prototype.measureText;
          try {
            CanvasRenderingContext2D.prototype.measureText = function(
                text: string) {
              if (text.includes('wrap')) {
                return {width: 150} as TextMetrics;
              }
              return {width: 50} as TextMetrics;
            };
            Object.defineProperty(
                input, 'clientWidth', {configurable: true, get: () => 100});

            await simulateUserTextInput(inputElement, 'tes.');
            element.smartComposeInlineHint = 'wrap';
            await microtasksFinished();

            element.setActiveElement(input);
            const tabEvent = new KeyboardEvent('keydown', {
              key: 'Tab',
              bubbles: true,
              cancelable: true,
            });
            element.getWrapperElement().dispatchEvent(tabEvent);
            await microtasksFinished();

            assertEquals('tes.', element.input);
          } finally {
            CanvasRenderingContext2D.prototype.measureText =
                originalMeasureText;
          }
        });
  });

  suite('Querying', () => {
    test('composebox queries autocomplete on load', async () => {
      loadTimeData.overrideValues({composeboxShowZps: true});
      element = createTestElement();
      await microtasksFinished();

      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);

      loadTimeData.overrideValues({composeboxShowZps: false});
    });

    test('composebox stops autocomplete when clearing input', async () => {
      element = createTestElement();
      await microtasksFinished();

      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
      assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 0);

      setInputValue(element.getInputElement().inputElement, 'T');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();
      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 2);

      setInputValue(element.getInputElement().inputElement, '');
      element.getInputElement().inputElement.dispatchEvent(new Event('input'));
      await microtasksFinished();

      assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 1);
      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 3);
    });

    test('queryAutocomplete passes cursor position', async () => {
      element = createTestElement();
      await microtasksFinished();

      element.input = 'hello';
      await microtasksFinished();

      const inputElement = element.getInputElement();
      (inputElement.inputElement as HTMLTextAreaElement).value = 'hello';
      inputElement.inputElement.focus();
      setSelectionOffset(inputElement.inputElement, 3);

      searchboxHandler.resetResolver('queryAutocomplete');
      element.queryAutocomplete(/*clearMatches=*/ false);

      const args = await searchboxHandler.whenCalled('queryAutocomplete');
      // queryId is 1 because queryId 0 was issued by the initial ZPS query on
      // load.
      assertDeepEquals(
          [
            1,
            null,
            'hello',
            false,
            3,
            SuggestInventory.kDefault,
            false,
            '',
            InputMethod.kKeyboard,
          ],
          args);
    });

    test(
        'queryAutocomplete passes cursor position when input is out of sync',
        async () => {
          element = createTestElement();
          await microtasksFinished();

          element.input = 'hello';
          await microtasksFinished();

          const inputElement = element.getInputElement();
          (inputElement.inputElement as HTMLTextAreaElement).value = 'hello';
          inputElement.inputElement.focus();

          // Simulate a programming update of the input as happens when, e.g.,
          // the user closes the composebox. This update won't be immediately
          // reflected in the DOM.
          element.input = 'hello world';

          // Clear the `queryAutocomplete` called for ZPS.
          searchboxHandler.resetResolver('queryAutocomplete');
          element.queryAutocomplete(/*clearMatches=*/ false);

          const args = await searchboxHandler.whenCalled('queryAutocomplete');
          // queryId is 1 because queryId 0 was issued by the initial ZPS query
          // on load.
          assertDeepEquals(
              [
                1,
                null,
                'hello world',
                false,
                11,
                SuggestInventory.kDefault,
                false,
                '',
                InputMethod.kKeyboard,
              ],
              args);
        });

    test(
        'does not query autocomplete on load when queryZpsOnLoad is false',
        async () => {
          searchboxHandler.resetResolver('queryAutocomplete');
          const freshComposebox =
              document.createElement('test-composebox-mixin');
          // queryZpsOnLoad is read in connectedCallback, so it must be set
          // before the element connects. Contextual Tasks sets it false and
          // drives autocomplete from its own zero-state logic instead.
          freshComposebox.queryZpsOnLoad = false;
          document.body.appendChild(freshComposebox);
          await microtasksFinished();

          assertEquals(0, searchboxHandler.getCallCount('queryAutocomplete'));
        });

    test('autocomplete matches are cleared on submit', async () => {
      element = createTestElement();
      await microtasksFinished();

      element.input = 'Some text';
      await microtasksFinished();

      const event = new KeyboardEvent('keydown', {
        key: 'Enter',
        shiftKey: false,
        bubbles: true,
        cancelable: true,
      });
      element.setActiveElement(element.getInputElement().inputElement);
      element.getWrapperElement().dispatchEvent(event);
      await microtasksFinished();

      const clearResult = await searchboxHandler.whenCalled('stopAutocomplete');
      assertTrue(clearResult);
      assertFalse(element.showDropdown);
      assertEquals(null, element.result);
      assertEquals('', element.lastQueriedInput);
    });

    test(
        'activeQueryId is not reset to -1 when selection cleared and input' +
            ' is empty',
        async () => {
          element = createTestElement();
          await microtasksFinished();

          element.input = '';
          element.activeQueryId = 0;
          element.lastQueriedInput = '';

          const matches = [
            {fillIntoEdit: 'match1', supportsDeletion: false} as
                AutocompleteMatch,
          ];
          element.result = {input: '', matches} as AutocompleteResult;
          element.selectedMatchIndex = 0;
          await microtasksFinished();

          element.selectedMatchIndex = -1;
          await microtasksFinished();

          assertEquals(0, element.activeQueryId);
        });

    test(
        'activeQueryId is reset to -1 when selection cleared and input' +
            ' is not empty',
        async () => {
          element = createTestElement();
          await microtasksFinished();

          element.input = 'Some text';
          element.activeQueryId = 0;
          element.lastQueriedInput = '';

          const matches = [
            {fillIntoEdit: 'match1', supportsDeletion: false} as
                AutocompleteMatch,
          ];
          element.result = {input: '', matches} as AutocompleteResult;
          element.selectedMatchIndex = 0;
          await microtasksFinished();

          element.selectedMatchIndex = -1;
          await microtasksFinished();

          assertEquals(-1, element.activeQueryId);
        });
  });

  suite('VoiceSearch', () => {
    test(
        'submits w/o querying autocomplete on voice search final result',
        async () => {
          loadTimeData.overrideValues({composeboxShowZps: true});
          element = createTestElement({showVoiceSearch: true});
          await microtasksFinished();
          searchboxHandler.reset();

          const voiceSearchActionPromise =
              eventToPromise<CustomEvent<{value: VoiceSearchAction}>>(
                  'voice-search-action', element);
          const voiceQuery = 'hello';
          const voiceSearchElement = $$<ComposeboxVoiceSearchElement>(
              element, 'cr-composebox-voice-search');
          assertTrue(!!voiceSearchElement);
          voiceSearchElement.dispatchEvent(new CustomEvent(
              'voice-search-final-result',
              {detail: voiceQuery, bubbles: true, composed: true}));

          const voiceSearchActionEvent = await voiceSearchActionPromise;
          assertEquals(
              VoiceSearchAction.QUERY_SUBMITTED,
              voiceSearchActionEvent.detail.value);
          await microtasksFinished();

          assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 0);
          assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);
          assertEquals(
              voiceQuery,
              searchboxHandler.getArgs('submitQuery')[0][0]);
        });
  });
});
