// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import './test_composebox_mixin.js';

import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {VoiceSearchAction} from 'chrome://resources/cr_components/composebox/composebox_mixin.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock, MockInputState} from './composebox_test_utils.js';
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

function getSubmitContainer(element: TestComposeboxMixinElement): HTMLElement {
  return element.shadowRoot.querySelector('cr-composebox-submit')!;
}

function getSubmitIcon(element: TestComposeboxMixinElement): HTMLElement {
  const submitContainer = getSubmitContainer(element);
  return submitContainer.shadowRoot!.querySelector('#submitIcon')!;
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

          const submitButton = getSubmitIcon(element);
          assertFalse(submitButton.hasAttribute('disabled'));

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
