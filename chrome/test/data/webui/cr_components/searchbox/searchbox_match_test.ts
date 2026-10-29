// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import {SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {SearchboxMatchElement} from 'chrome://new-tab-page/new_tab_page.js';
import {createAutocompleteMatch, createFormattedString, createMatchKeywordModelForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {NavigationPredictor} from 'chrome://resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import {KeywordActivationMethod, KeywordType, SelectionLineState} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ColorType} from 'chrome://resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';
import {assertArrayEquals, assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createFormattedStringFragment, createSuggestTemplateInfo} from './searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('CrComponentsRealboxMatchTest', () => {
  let matchEl: SearchboxMatchElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    matchEl = document.createElement('cr-searchbox-match');
    matchEl.match = createAutocompleteMatch();
    matchEl.matchIndex = 0;
    document.body.appendChild(matchEl);
  });

  test('MousedownEventsAreSentToHandler', async () => {
    const matchIndex = 2;
    const destinationUrl = 'http://google.com';
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    matchEl.dispatchEvent(new MouseEvent('mousedown'));
    const args = await testProxy.handler.whenCalled('onNavigationLikely');
    assertEquals(matchIndex, args.line);
    assertEquals(destinationUrl, args.url);
    assertEquals(NavigationPredictor.kMouseDown, args.navigationPredictor);
  });

  test('ClickNavigates', async () => {
    const matchIndex = 1;
    const destinationUrl = 'http://google.com';
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    const clickEvent = new MouseEvent('click', {
      button: 0,
      cancelable: true,
      altKey: true,
      ctrlKey: false,
      metaKey: true,
      shiftKey: false,
    });
    matchEl.dispatchEvent(clickEvent);
    assertTrue(clickEvent.defaultPrevented);
    const clickArgs =
        await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertDeepEquals(
        [
          matchIndex,
          destinationUrl,
          true,
          clickEvent.button,
          clickEvent.altKey,
          clickEvent.ctrlKey,
          clickEvent.metaKey,
          clickEvent.shiftKey,
        ],
        [
          clickArgs.line,
          clickArgs.url,
          clickArgs.areMatchesShowing,
          clickArgs.mouseButton,
          clickArgs.modifiers.altKey,
          clickArgs.modifiers.ctrlKey,
          clickArgs.modifiers.metaKey,
          clickArgs.modifiers.shiftKey,
        ]);
    testProxy.handler.reset();

    // Right clicks are ignored.
    const rightClickEvent =
        new MouseEvent('click', {button: 2, cancelable: true});
    matchEl.dispatchEvent(rightClickEvent);
    assertFalse(rightClickEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));

    // Middle clicks are accepted.
    const middleClickEvent =
        new MouseEvent('auxclick', {button: 1, cancelable: true});
    matchEl.dispatchEvent(middleClickEvent);
    assertTrue(middleClickEvent.defaultPrevented);
    const middleClickArgs =
        await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(matchIndex, middleClickArgs.line);
    assertDeepEquals(destinationUrl, middleClickArgs.url);
    assertEquals(1, middleClickArgs.mouseButton);
  });

  test('ClickFiresEvent', async () => {
    const clickPromise = eventToPromise('match-click', matchEl);

    const clickEvent = new MouseEvent('click', {
      button: 0,
      cancelable: true,
      altKey: true,
      ctrlKey: false,
      metaKey: true,
      shiftKey: false,
    });
    matchEl.dispatchEvent(clickEvent);

    await clickPromise;
  });

  test('DeleteButtonRemovesMatch', async () => {
    const matchIndex = 1;
    const destinationUrl = 'http://google.com';
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    // By pressing 'Enter' on the button.
    const keydownEvent = (new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Enter',
    }));
    matchEl.$.remove.dispatchEvent(keydownEvent);
    assertTrue(keydownEvent.defaultPrevented);
    const keydownArgs =
        await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(matchIndex, keydownArgs.line);
    assertEquals(destinationUrl, keydownArgs.url);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
    // Pressing 'Enter' the button doesn't accidentally trigger navigation.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
    testProxy.handler.reset();

    matchEl.$.remove.click();
    const clickArgs =
        await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(matchIndex, clickArgs.line);
    assertEquals(destinationUrl, clickArgs.url);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
    // Clicking the button doesn't accidentally trigger navigation.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('UpdateSelectionUpdatesClasses', async () => {
    // Add keyword chip and 2 actions.
    const match = createAutocompleteMatch();
    match.keywordModel =
        createMatchKeywordModelForTesting({chipHint: 'keyword'});
    match.actions.push({
      hint: 'hint',
      suggestionContents: 'suggestionContents',
      iconPath: 'iconPath',
      a11yLabel: 'a11yLabel',
    });
    match.actions.push({...match.actions[0]!});
    matchEl.match = match;
    await microtasksFinished();

    // When a match is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertFalse(
        !!matchEl.shadowRoot.querySelector('#focusIndicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.shadowRoot.querySelector('#remove.selected'));

    // When a match is unselected.
    matchEl.selection = {
      line: 1,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertFalse(
        !!matchEl.shadowRoot.querySelector('#focusIndicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(matchEl.$.remove.classList.contains('selected'));

    // When the keyword chip is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kKeywordMode,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertTrue(
        !!matchEl.shadowRoot.querySelector('#focusIndicator.selected-within'));
    assertTrue(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.shadowRoot.querySelector('#remove.selected'));

    // When the 1st action chip is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kFocusedButtonAction,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertTrue(
        !!matchEl.shadowRoot.querySelector('#focusIndicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([true, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.shadowRoot.querySelector('#remove.selected'));

    // When the 2nd action chip is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kFocusedButtonAction,
      actionIndex: 1,
    };
    await microtasksFinished();
    assertTrue(
        !!matchEl.shadowRoot.querySelector('#focusIndicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, true], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.shadowRoot.querySelector('#remove.selected'));

    // When the remove button is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kFocusedButtonRemoveSuggestion,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertTrue(
        !!matchEl.shadowRoot.querySelector('#focusIndicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertTrue(!!matchEl.shadowRoot.querySelector('#remove.selected'));
  });

  test('PrimaryAndSecondaryTextWithFragments', async () => {
    const match = createAutocompleteMatch();
    match.suggestTemplate.primaryText = createFormattedString('test content', [
      createFormattedStringFragment({startIndex: 0}),
      createFormattedStringFragment({startIndex: 5, isBolded: true}),
    ]);
    match.suggestTemplate.secondaryText =
        createFormattedString('test description', [
          createFormattedStringFragment({startIndex: 0}),
          createFormattedStringFragment({startIndex: 5, isBolded: true}),
        ]);
    matchEl.match = match;
    await microtasksFinished();

    const primaryTextEl = matchEl.$.primaryText;
    // 'test ' is rendered unstyled. 'content' is rendered as a "match".
    assertEquals(
        '<span>test </span><span class="match">content</span>',
        primaryTextEl.innerHTML);
    const secondaryTextEl = matchEl.$.secondaryText;
    // 'test ' is rendered unstyled. 'description' is rendered as a "match".
    assertEquals(
        '<span>test </span><span class="match">description</span>',
        secondaryTextEl.innerHTML);
  });

  test('FragmentsNotStartingAtZeroIndex', async () => {
    const match = createAutocompleteMatch();
    match.suggestTemplate.primaryText = createFormattedString(
        'prefix content',
        [createFormattedStringFragment({startIndex: 7, isBolded: true})]);
    matchEl.match = match;
    await microtasksFinished();

    const primaryTextEl = matchEl.$.primaryText;
    // 'prefix' is rendered unstyled.
    assertEquals(
        '<span>prefix </span><span class="match">content</span>',
        primaryTextEl.innerHTML);
  });

  test('FragmentColorsMapToCssClasses', async () => {
    const match = createAutocompleteMatch();
    match.suggestTemplate.primaryText = createFormattedString('abcdef', [
      createFormattedStringFragment(
          {startIndex: 0, color: ColorType.kSuggestQueryText}),
      createFormattedStringFragment(
          {startIndex: 1, color: ColorType.kSuggestHintText}),
      createFormattedStringFragment({
        startIndex: 2,
        color: ColorType.kSuggestHintText,
        isBolded: true,
      }),
      createFormattedStringFragment({
        startIndex: 3,
        color: ColorType.kSuggestActionHigh,
        isBolded: true,
      }),
      createFormattedStringFragment({startIndex: 4, color: ColorType.kUnknown}),
      // Other colors get no class and keep the default color.
      createFormattedStringFragment({startIndex: 5, color: ColorType.kPrimary}),
    ]);
    matchEl.match = match;
    await microtasksFinished();

    const primaryTextEl = matchEl.$.primaryText;
    assertEquals(
        '<span class="color-on-surface">a</span>' +
            '<span class="color-on-surface-subtle">b</span>' +
            '<span class="color-on-surface-subtle match">c</span>' +
            '<span class="color-primary match">d</span>' +
            '<span>e</span><span>f</span>',
        primaryTextEl.innerHTML);
  });

  test('EscapesPrimaryAndSecondaryText', async () => {
    const match = createAutocompleteMatch();
    match.suggestTemplate.primaryText = createFormattedString(
        '<script>alert("xss")</script>Safe Content',
        [createFormattedStringFragment()]);
    match.suggestTemplate.secondaryText = createFormattedString(
        '<img src=x onerror=alert(1)>Safe Description',
        [createFormattedStringFragment()]);
    matchEl.match = match;
    await microtasksFinished();

    const primaryTextEl = matchEl.$.primaryText;
    // `<script>` HTML tag is escaped. Primary text is rendered unstyled.
    assertEquals(
        '<span>&lt;script&gt;alert("xss")&lt;/script&gt;Safe Content</span>',
        primaryTextEl.innerHTML);
    assertEquals(0, primaryTextEl.querySelectorAll('script').length);

    const secondaryTextEl = matchEl.$.secondaryText;
    // `<img>` HTML tag is escaped. Secondary text is rendered unstyled.
    assertEquals(
        '<span>&lt;img src=x onerror=alert(1)&gt;Safe Description</span>',
        secondaryTextEl.innerHTML);
    assertEquals(0, secondaryTextEl.querySelectorAll('img').length);
  });

  test('AriaLabelDoesNotFollowVirtualFocusSelection', async () => {
    // Virtual focus selection changes, including onto this match's keyword
    // chip, action and remove button, are narrated by the hosting searchbox.
    // The match's own label stays put so nothing else narrates them.
    matchEl.virtualFocusEnabled = true;
    const match = createAutocompleteMatch();
    match.a11yLabel = 'Search Google';
    match.suggestTemplate.secondaryText = createFormattedString('Google');
    match.keywordModel = createMatchKeywordModelForTesting(
        {chipA11y: 'Search Google in Keyword Mode'});
    match.actions = [
      {
        hint: 'action hint',
        suggestionContents: 'suggestionContents',
        iconPath: 'iconPath',
        a11yLabel: 'First Action A11y Label',
      },
    ];
    match.supportsDeletion = true;
    match.removeButtonA11yLabel = 'Remove Suggestion';
    matchEl.match = match;
    matchEl.matchIndex = 0;
    await microtasksFinished();
    assertEquals('Search Google', matchEl.ariaLabel);

    for (const state
             of [SelectionLineState.kNormal, SelectionLineState.kKeywordMode,
                 SelectionLineState.kFocusedButtonAction,
                 SelectionLineState.kFocusedButtonRemoveSuggestion]) {
      matchEl.selection = {line: 0, state, actionIndex: 0};
      await microtasksFinished();
      assertEquals('Search Google', matchEl.ariaLabel);
    }

    // Nor when another line is selected.
    matchEl.selection = {
      line: 1,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertEquals('Search Google', matchEl.ariaLabel);
  });

  test('VirtualFocusSelectionDoesNotAnnounce', async () => {
    // The hosting searchbox's input narrates virtual focus selection changes,
    // so matches must not compete with it.
    matchEl.virtualFocusEnabled = true;
    const match = createAutocompleteMatch();
    match.a11yLabel = 'Search Google';
    matchEl.match = match;
    matchEl.matchIndex = 0;
    await microtasksFinished();

    let notified = false;
    matchEl.ariaNotify = () => {
      notified = true;
    };

    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertEquals('Search Google', matchEl.ariaLabel);
    assertFalse(notified);
  });

  test('InstantKeywordMatchClickFiresKeywordClickAndRefocuses', async () => {
    matchEl.match = createAutocompleteMatch({
      destinationUrl: 'http://bookmarks',
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kInstant,
        keyword: '@bookmarks',
        chipHint: 'Bookmarks',
      }),
    });
    matchEl.matchIndex = 1;
    await microtasksFinished();

    // Mousedown on instant keyword match prevents default (avoiding focus
    // loss).
    const mousedownEvent = new MouseEvent('mousedown', {
      button: 0,
      cancelable: true,
    });
    matchEl.dispatchEvent(mousedownEvent);
    assertTrue(mousedownEvent.defaultPrevented);

    const keywordClickPromise = eventToPromise('keyword-click', matchEl);
    const clickEvent = new MouseEvent('click', {
      button: 0,
      cancelable: true,
    });
    matchEl.dispatchEvent(clickEvent);

    const event = await keywordClickPromise as CustomEvent;
    assertEquals(matchEl.match, event.detail.match);
    assertEquals(1, event.detail.matchIndex);

    assertEquals(1, testProxy.handler.getCallCount('activateKeyword'));
    const activateKeywordArgs = testProxy.handler.getArgs('activateKeyword')[0];
    assertEquals(1, activateKeywordArgs.line);
    assertEquals(
        KeywordActivationMethod.kClick, activateKeywordArgs.activationMethod);
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('SecondaryTextWithFragments', async () => {
    matchEl.match = createAutocompleteMatch({
      suggestTemplate: createSuggestTemplateInfo({
        secondaryText: createFormattedString(
            'MIA Basketball',
            [
              createFormattedStringFragment({
                startIndex: 0,
                color: ColorType.kSuggestHintText,
                isBolded: true,
              }),
              createFormattedStringFragment(
                  {startIndex: 4, color: ColorType.kSuggestHintText}),
            ]),
      }),
    });
    await microtasksFinished();

    const spans = matchEl.$.secondaryText.querySelectorAll('span');
    assertEquals(2, spans.length);
    assertEquals('MIA ', spans[0]!.textContent);
    assertTrue(spans[0]!.classList.contains('match'));
    assertTrue(spans[0]!.classList.contains('color-on-surface-subtle'));
    assertEquals('Basketball', spans[1]!.textContent);
    assertFalse(spans[1]!.classList.contains('match'));
    assertTrue(spans[1]!.classList.contains('color-on-surface-subtle'));
  });

  test('DescriptionIcon', async () => {
    const iconStyle = () =>
        getComputedStyle(matchEl.$.secondaryText, '::before');
    assertEquals('none', iconStyle().content);

    matchEl.match = createAutocompleteMatch({
      suggestTemplate:
          createSuggestTemplateInfo({secondaryText: 'Opens in side panel'}),
      descriptionIconPath: 'dock_to_right.svg',
    });
    await microtasksFinished();
    assertEquals('""', iconStyle().content);
    assertTrue(iconStyle()
                   .getPropertyValue('-webkit-mask-image')
                   .includes('dock_to_right.svg'));
  });

  test('HideDescription', async () => {
    matchEl.match = createAutocompleteMatch({
      suggestTemplate:
          createSuggestTemplateInfo({secondaryText: 'description'}),
    });
    await microtasksFinished();
    assertEquals(
        'visible', getComputedStyle(matchEl.$.secondaryText).visibility);

    matchEl.style.setProperty(
        '--cr-searchbox-match-description-visibility', 'hidden');
    assertEquals(
        'hidden', getComputedStyle(matchEl.$.secondaryText).visibility);
    assertEquals('hidden', getComputedStyle(matchEl.$.separator).visibility);
  });

  test('RendersNullSecondaryTextAsEmpty', async () => {
    const match = createAutocompleteMatch();
    match.suggestTemplate.primaryText = createFormattedString('Content');
    match.suggestTemplate.secondaryText = null;
    matchEl.match = match;
    await microtasksFinished();

    assertEquals('', matchEl.$.secondaryText.textContent);
  });

  test('SecondaryTextIsUrlSetsUrlLayout', async () => {
    matchEl.style.setProperty(
        '--cr-searchbox-url-primary-text-max-width', '65%');
    matchEl.match = createAutocompleteMatch({secondaryTextIsUrl: true});
    await microtasksFinished();
    assertTrue(matchEl.hasAttribute('secondary-text-is-url_'));
    assertEquals('65%', getComputedStyle(matchEl.$.primaryText).maxWidth);

    // ACTION_HIGH text without the bit doesn't get the URL layout.
    matchEl.match = createAutocompleteMatch({
      suggestTemplate: createSuggestTemplateInfo({
        secondaryText: createFormattedString(
            'description',
            [createFormattedStringFragment(
                {startIndex: 0, color: ColorType.kSuggestActionHigh})]),
      }),
    });
    await microtasksFinished();
    assertFalse(matchEl.hasAttribute('secondary-text-is-url_'));
    assertEquals('none', getComputedStyle(matchEl.$.primaryText).maxWidth);
  });

  test('SecondaryTextTakesColorOfLastSpanForEllipsis', async () => {
    matchEl.style.setProperty(
        '--color-searchbox-results-foreground-dimmed', 'rgb(0, 128, 0)');
    matchEl.style.setProperty(
        '--color-searchbox-results-url', 'rgb(0, 0, 255)');
    const createMatch = (firstColor: ColorType, lastColor: ColorType) =>
        createAutocompleteMatch({
          suggestTemplate: createSuggestTemplateInfo({
            secondaryText: createFormattedString(
                'first last',
                [
                  createFormattedStringFragment(
                      {startIndex: 0, color: firstColor}),
                  createFormattedStringFragment(
                      {startIndex: 6, color: lastColor}),
                ]),
          }),
        });

    matchEl.match =
        createMatch(ColorType.kSuggestHintText, ColorType.kSuggestActionHigh);
    await microtasksFinished();
    assertEquals(
        'rgb(0, 0, 255)', getComputedStyle(matchEl.$.secondaryText).color);

    matchEl.match =
        createMatch(ColorType.kSuggestActionHigh, ColorType.kSuggestHintText);
    await microtasksFinished();
    assertEquals(
        'rgb(0, 128, 0)', getComputedStyle(matchEl.$.secondaryText).color);
  });
});
