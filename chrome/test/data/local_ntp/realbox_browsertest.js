// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stuff shared between all realbox[0-9]+ tests.
test.realbox = {};

// TODO(https://crbug.com/1024825): Numeric suffixes were added to reduce the
// chance of timeouts. This splits these many tests cases over multiple
// TEST_F()s which yield more parallelism and more realistic timing.
test.realbox1 = {};
test.realbox2 = {};

/**
 * @enum {string}
 * @const
 */
test.realbox.IDS = {
  REALBOX: 'realbox',
  REALBOX_INPUT_WRAPPER: 'realbox-input-wrapper',
  REALBOX_MATCHES: 'realbox-matches',
};

/**
 * @enum {string}
 * @const
 */
test.realbox.CLASSES = {
  REMOVABLE: 'removable',
  REMOVE_ICON: 'remove-icon',
  SELECTED: 'selected',
  SHOW_MATCHES: 'show-matches',
};

/** @return {boolean} */
test.realbox.areMatchesShowing = function() {
  return test.realbox.wrapperEl.classList.contains(
    test.realbox.CLASSES.SHOW_MATCHES);
}

/**
 * @param {string} name
 * @param {!ClipboardEvent}
 */
test.realbox.clipboardEvent = function(name) {
  return new ClipboardEvent(
      name, {cancelable: true, clipboardData: new DataTransfer()});
};

/**
 * @param {string} type
 * @param {!Object=} modifiers
 */
test.realbox.trustedEventFacade = function(type, modifiers = {}) {
  return Object.assign(
      {
        type,
        isTrusted: true,
        defaultPrevented: false,
        preventDefault() {
          this.defaultPrevented = true;
        },
      },
      modifiers);
};

/**
 * @param {!Object=} modifiers Things to override about the returned result.
 * @return {!AutocompleteResult}
 */
test.realbox.getUrlMatch = function(modifiers = {}) {
  return Object.assign(
      {
        allowedToBeDefaultMatch: true,
        canDisplay: true,
        contents: 'helloworld.com',
        contentsClass: [{offset: 0, style: 1}],
        description: '',
        descriptionClass: [],
        destinationUrl: 'https://helloworld.com/',
        inlineAutocompletion: '',
        isSearchType: false,
        fillIntoEdit: 'https://helloworld.com',
        swapContentsAndDescription: true,
        type: 'url-what-you-typed',
      },
      modifiers);
};

/**
 * @param {!Object=} modifiers Things to override about the returned result.
 * @return {!AutocompleteResult}
 */
test.realbox.getSearchMatch = function(modifiers = {}) {
  return Object.assign(
      {
        allowedToBeDefaultMatch: true,
        canDisplay: true,
        contents: 'hello world',
        contentsClass: [{offset: 0, style: 0}],
        description: 'Google search',
        descriptionClass: [{offset: 0, style: 4}],
        destinationUrl: 'https://www.google.com/search?q=hello+world',
        inlineAutocompletion: '',
        isSearchType: true,
        fillIntoEdit: 'hello world',
        swapContentsAndDescription: false,
        type: 'search-what-you-typed',
      },
      modifiers);
};

/** @type {!Array<number>} */
test.realbox.deletedLines;

/** @type {!Array<Object>} */
test.realbox.opens;

/** @typedef {{input: string, preventInlineAutocomplete: bool}} */
let AutocompleteQuery;

/** @type {!Array<AutocompleteQuery>} */
test.realbox.queries;

/** @type {!Element} */
test.realbox.realboxEl;

/**
 * Sets up the page for each individual test.
 */
test.realbox1.setUp = test.realbox2.setUp = function() {
  setUpPage('local-ntp-template');

  configData.realboxEnabled = true;
  configData.suggestionTransparencyEnabled = true;

  chrome.embeddedSearch = {
    newTabPage: {},
    searchBox: {
      deleteAutocompleteMatch(line) {
        test.realbox.deletedLines.push(line);
      },
      openAutocompleteMatch(index, url, button, alt, ctrl, meta, shift) {
        test.realbox.opens.push({index, url, button, alt, ctrl, meta, shift});
      },
      queryAutocomplete(input, preventInlineAutocomplete) {
        test.realbox.queries.push({input, preventInlineAutocomplete});
      },
      stopAutocomplete(clearResult) {}
    },
  };

  test.realbox.deletedLines = [];
  test.realbox.opens = [];
  test.realbox.queries = [];

  initLocalNTP(/*isGooglePage=*/ true);

  test.realbox.realboxEl = $(test.realbox.IDS.REALBOX);
  assertTrue(!!test.realbox.realboxEl);

  test.realbox.wrapperEl = $(test.realbox.IDS.REALBOX_INPUT_WRAPPER);
  assertTrue(!!test.realbox.wrapperEl);

  assertFalse(test.realbox.areMatchesShowing());
};

test.realbox1.testEmptyValueDoesntQueryAutocomplete = function() {
  test.realbox.realboxEl.value = '';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(test.realbox.queries.length, 0);
};

test.realbox1.testSpacesDontQueryAutocomplete = function() {
  test.realbox.realboxEl.value = '   ';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(test.realbox.queries.length, 0);
};

test.realbox1.testInputSentAsQuery = function() {
  test.realbox.realboxEl.value = 'hello realbox';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);
  assertEquals('hello realbox', test.realbox.queries[0].input);
};

test.realbox1.testReplyWithMatches = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);
  assertEquals('hello world', test.realbox.queries[0].input);

  const matches = [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged(
      {input: test.realbox.realboxEl.value, matches});

  assertTrue(test.realbox.areMatchesShowing());

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertTrue(!!matchesEl);
  assertTrue(matchesEl.hasAttribute('role'));
  assertEquals(matches.length, matchesEl.children.length);
  assertEquals(matches[0].destinationUrl, matchesEl.children[0].href);
  assertEquals(matches[1].destinationUrl, matchesEl.children[1].href);
  assertEquals(matches[1].textContent, matchesEl.children[1].contents);
  assertTrue(matchesEl.children[0].hasAttribute('role'));
};

test.realbox1.testReplyWithInlineAutocompletion = function() {
  test.realbox.realboxEl.value = 'hello ';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);
  assertEquals('hello ', test.realbox.queries[0].input);

  const match = test.realbox.getSearchMatch({
    contents: 'hello ',
    inlineAutocompletion: 'world',
  });
  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [match],
  });

  assertTrue(test.realbox.areMatchesShowing());

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertTrue(!!matchesEl);
  assertEquals(1, matchesEl.children.length);
  assertEquals(match.destinationUrl, matchesEl.children[0].href);

  const realboxValue = test.realbox.realboxEl.value;
  assertEquals('hello world', realboxValue);
  const start = test.realbox.realboxEl.selectionStart;
  const end = test.realbox.realboxEl.selectionEnd;
  assertEquals('world', realboxValue.substring(start, end));
};

// Ensures that deleting text from input informs the backend to prevent inline
// autocompletion for the default match.
test.realbox1.testDeleteWithInlineAutocompletion = function() {
  test.realbox.realboxEl.value = 'supercal';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);
  assertEquals('supercal', test.realbox.queries[0].input);
  assertFalse(test.realbox.queries[0].preventInlineAutocomplete);

  const match = test.realbox.getSearchMatch({
    contents: 'supercal',
    inlineAutocompletion: 'ifragilisticexpialidocious',
  });
  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [match],
  });

  const realboxValue = test.realbox.realboxEl.value;
  assertEquals('supercalifragilisticexpialidocious', realboxValue);

  test.realbox.realboxEl.value = 'superca';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(2, test.realbox.queries.length);
  assertEquals('superca', test.realbox.queries[1].input);
  assertTrue(test.realbox.queries[1].preventInlineAutocomplete)

  match.contents = 'superca';
  match.inlineAutocompletion = '';
  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [match],
  });

  assertEquals('superca', test.realbox.realboxEl.value);
};

test.realbox.testTypeInlineAutocompletion = function() {
  test.realbox.realboxEl.value = 'what are the';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [
      test.realbox.getSearchMatch({
        contents: 'what are the',
        inlineAutocompletion: 'se strawberries',
      }),
    ],
  });

  assertEquals('what are these strawberries', test.realbox.realboxEl.value);
  assertEquals('what are the'.length, test.realbox.realboxEl.selectionStart);
  assertEquals(
      'what are these strawberries'.length,
      test.realbox.realboxEl.selectionEnd);

  let wasValueSetterCalled = false;
  const originalValue =
      Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value');

  Object.defineProperty(test.realbox.realboxEl, 'value', {
    get: originalValue.get,
    set: value => {
      wasValueSetterCalled = true;
      originalValue.set.call(test.realbox.realboxEl, value);
    }
  });

  // Pretend the user typed the next character of the inline autocompletion.
  const keyEvent =
      new KeyboardEvent('keydown', {bubbles: true, cancelable: true, key: 's'});
  test.realbox.realboxEl.dispatchEvent(keyEvent);
  assertTrue(keyEvent.defaultPrevented);

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [
      test.realbox.getSearchMatch({
        contents: 'what are thes',
        inlineAutocompletion: 'e strawberries',
      }),
    ],
  });


  assertEquals('what are these strawberries', test.realbox.realboxEl.value);
  assertEquals('what are thes'.length, test.realbox.realboxEl.selectionStart);
  assertEquals(
      'what are these strawberries'.length,
      test.realbox.realboxEl.selectionEnd);
  assertFalse(wasValueSetterCalled);
};

test.realbox1.testResultsPreserveCursorPosition = function() {
  test.realbox.realboxEl.value = 'z';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch({contents: 'z'})],
  });

  test.realbox.realboxEl.value = 'az';
  test.realbox.realboxEl.selectionStart = 1;
  test.realbox.realboxEl.selectionEnd = 1;
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch({contents: 'az'})],
  });

  assertEquals(1, test.realbox.realboxEl.selectionStart);
  assertEquals(1, test.realbox.realboxEl.selectionEnd);
};

test.realbox.testCopyEmptyInputFails = function() {
  const copyEvent = test.realbox.clipboardEvent('copy');
  test.realbox.realboxEl.dispatchEvent(copyEvent);
  assertFalse(copyEvent.defaultPrevented);
};

test.realbox1.testCopySearchResultFails = function() {
  test.realbox.realboxEl.value = 'skittles!';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch({contents: 'skittles!'})],
  });

  test.realbox.realboxEl.setSelectionRange(0, 'skittles!'.length);

  const copyEvent = test.realbox.clipboardEvent('copy');
  test.realbox.realboxEl.dispatchEvent(copyEvent);
  assertFalse(copyEvent.defaultPrevented);
};

test.realbox1.testCopyUrlSucceeds = function() {
  test.realbox.realboxEl.value = 'go';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getUrlMatch({
      contents: 'go',
      inlineAutocompletion: 'ogle.com',
      destinationUrl: 'https://www.google.com/',
    })]
  });

  assertEquals('google.com', test.realbox.realboxEl.value);

  test.realbox.realboxEl.setSelectionRange(0, 'google.com'.length);

  const copyEvent = test.realbox.clipboardEvent('copy');
  test.realbox.realboxEl.dispatchEvent(copyEvent);
  assertTrue(copyEvent.defaultPrevented);
  assertEquals(
      'https://www.google.com/', copyEvent.clipboardData.getData('text/plain'));
  assertFalse(test.realbox.realboxEl.value === '');
};

test.realbox1.testCutEmptyInputFails = function() {
  const cutEvent = test.realbox.clipboardEvent('cut');
  test.realbox.realboxEl.dispatchEvent(cutEvent);
  assertFalse(cutEvent.defaultPrevented);
};

test.realbox1.testCutSearchResultFails = function() {
  test.realbox.realboxEl.value = 'skittles!';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch({contents: 'skittles!'})],
  });

  test.realbox.realboxEl.setSelectionRange(0, 'skittles!'.length);

  const cutEvent = test.realbox.clipboardEvent('cut');
  test.realbox.realboxEl.dispatchEvent(cutEvent);
  assertFalse(cutEvent.defaultPrevented);
};

test.realbox1.testCutUrlSucceeds = function() {
  test.realbox.realboxEl.value = 'go';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [
      test.realbox.getUrlMatch({
        contents: 'go',
        inlineAutocompletion: 'ogle.com',
        destinationUrl: 'https://www.google.com/',
      }),
    ],
  });

  assertEquals('google.com', test.realbox.realboxEl.value);

  test.realbox.realboxEl.setSelectionRange(0, 'google.com'.length);

  const cutEvent = test.realbox.clipboardEvent('cut');
  test.realbox.realboxEl.dispatchEvent(cutEvent);
  assertTrue(cutEvent.defaultPrevented);
  assertEquals(
      'https://www.google.com/', cutEvent.clipboardData.getData('text/plain'));
  assertTrue(test.realbox.realboxEl.value === '');
};

test.realbox1.testStaleAutocompleteResult = function() {
  test.realbox.realboxEl.value = 'g';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  let RESULTS = [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: RESULTS,
  });

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(RESULTS.length, matchesEl.children.length);
  assertTrue(test.realbox.areMatchesShowing());

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value + 'a',  // Simulate stale response.
    matches: [],
  });

  // Checks to see that the matches UI wasn't re-rendered.
  const matchesEl2 = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(RESULTS.length, matchesEl2.children.length);
  assertTrue(test.realbox.areMatchesShowing());
  assertTrue(matchesEl === matchesEl2);
};

test.realbox2.testAutocompleteResultChanged = function() {
  test.realbox.realboxEl.value = 'g';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  test.realbox.realboxEl.value += 'o';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [],
  });

  assertEquals(0, $(test.realbox.IDS.REALBOX_MATCHES).children.length);
  assertFalse(test.realbox.areMatchesShowing());

  let RESULTS = [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: RESULTS,
  });

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(RESULTS.length, matchesEl.children.length);
  assertTrue(test.realbox.areMatchesShowing());

  test.realbox.realboxEl.value += 'o';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  test.realbox.realboxEl.value += 'g';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [],
  });

  const matchesEl2 = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(0, matchesEl2.children.length);
  assertFalse(test.realbox.areMatchesShowing());
  assertFalse(matchesEl === matchesEl2);

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: RESULTS,
  });

  const matchesEl3 = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(RESULTS.length, matchesEl3.children.length);
  assertTrue(test.realbox.areMatchesShowing());
  assertFalse(matchesEl === matchesEl3);
};

test.realbox2.testDeleteAutocompleteResultUnmodifiedDelete = function() {
  const keyEvent = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Delete',
  });
  test.realbox.realboxEl.dispatchEvent(keyEvent);
  assertFalse(keyEvent.defaultPrevented);
};

test.realbox2.testDeleteAutocompleteResultShiftDeleteWithNoMatches =
    function() {
  const keyEvent = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Delete',
    shiftKey: true,
  });
  test.realbox.realboxEl.dispatchEvent(keyEvent);
  assertFalse(keyEvent.defaultPrevented);
};

test.realbox2.testUnsupportedDeletion = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  const matches = [test.realbox.getSearchMatch({supportsDeletion: false})];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged(
      {input: test.realbox.realboxEl.value, matches});

  const keyEvent = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Delete',
    shiftKey: true,
  });
  test.realbox.realboxEl.dispatchEvent(keyEvent);
  assertFalse(keyEvent.defaultPrevented);
  assertEquals(0, test.realbox.deletedLines.length);

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertFalse(matchesEl.classList.contains(test.realbox.CLASSES.REMOVABLE));
};

test.realbox2.testSupportedDeletionSelectNextMatch = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);

  const matches = [
    test.realbox.getSearchMatch({supportsDeletion: true}),
    test.realbox.getUrlMatch({supportsDeletion: true}),
  ];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged(
      {input: test.realbox.realboxEl.value, matches});

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertTrue(matchesEl.classList.contains(test.realbox.CLASSES.REMOVABLE));

  const downArrow = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowDown',
  });
  test.realbox.realboxEl.dispatchEvent(downArrow);
  assertTrue(downArrow.defaultPrevented);

  assertEquals(2, matchesEl.children.length);
  assertTrue(
      matchesEl.children[1].classList.contains(test.realbox.CLASSES.SELECTED));
  assertFalse(test.realbox.realboxEl.value === 'hello world');

  matchesEl.children[1].focus();
  assertEquals(matchesEl.children[1], document.activeElement);

  const shiftDelete = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Delete',
    shiftKey: true,
  });
  test.realbox.realboxEl.dispatchEvent(shiftDelete);
  assertTrue(shiftDelete.defaultPrevented);

  assertEquals(1, test.realbox.deletedLines.length);
  assertEquals(1, test.realbox.deletedLines[0]);

  matchesEl.children[1].focus();
  assertEquals(matchesEl.children[1], document.activeElement);

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.queries[0].input,
    matches: [test.realbox.getSearchMatch({supportsDeletion: true})]
  });

  const newMatchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(1, newMatchesEl.children.length);
  assertTrue(newMatchesEl.children[0].classList.contains(
      test.realbox.CLASSES.SELECTED));

  assertEquals(test.realbox.realboxEl, document.activeElement);
  assertEquals('hello world', test.realbox.realboxEl.value);
};

test.realbox2.testSupportedDeletionDoNotSelectNextMatch = function() {
  test.realbox.realboxEl.value = 'hello';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  const matches = [
    test.realbox.getUrlMatch({supportsDeletion: true}),
    test.realbox.getSearchMatch({supportsDeletion: true}),
  ];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged(
      {input: test.realbox.realboxEl.value, matches});

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertTrue(matchesEl.classList.contains(test.realbox.CLASSES.REMOVABLE));

  const downArrow = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowDown',
  });
  test.realbox.realboxEl.dispatchEvent(downArrow);
  assertTrue(downArrow.defaultPrevented);

  assertEquals(2, matchesEl.children.length);
  assertTrue(
      matchesEl.children[1].classList.contains(test.realbox.CLASSES.SELECTED));
  assertEquals('hello world', test.realbox.realboxEl.value);

  const shiftDelete = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Delete',
    shiftKey: true,
  });
  test.realbox.realboxEl.dispatchEvent(shiftDelete);
  assertTrue(shiftDelete.defaultPrevented);

  assertEquals(1, test.realbox.deletedLines.length);
  assertEquals(1, test.realbox.deletedLines[0]);

  matchesEl.children[1].focus();
  assertEquals(matchesEl.children[1], document.activeElement);

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.queries[0].input,
    matches: [test.realbox.getSearchMatch({allowedToBeDefaultMatch: false})]
  });

  const newMatchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(1, newMatchesEl.children.length);
  assertFalse(newMatchesEl.children[0].classList.contains(
      test.realbox.CLASSES.SELECTED));

  assertEquals(test.realbox.realboxEl, document.activeElement);
  assertEquals('hello', test.realbox.realboxEl.value);
};

test.realbox2.testNonShiftDelete = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()],
  });

  const deleteKey = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Delete',
  });
  test.realbox.realboxEl.dispatchEvent(deleteKey);  // Previously threw error.
  assertFalse(deleteKey.defaultPrevented);
};

test.realbox2.testRemoveIcon = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  const matches = [test.realbox.getSearchMatch({supportsDeletion: true})];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged(
      {input: test.realbox.realboxEl.value, matches});

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertTrue(matchesEl.classList.contains(test.realbox.CLASSES.REMOVABLE));

  const enter = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Enter',
  });

  let clicked = false;
  matchesEl.children[0].onclick = () => clicked = true;

  const icon = matchesEl.querySelector(`.${test.realbox.CLASSES.REMOVE_ICON}`);
  icon.dispatchEvent(enter);

  assertFalse(clicked);

  icon.click();

  assertEquals(1, test.realbox.deletedLines.length);
  assertEquals(0, test.realbox.deletedLines[0]);
  assertEquals(0, test.realbox.opens.length);

  chrome.embeddedSearch.searchBox.autocompleteresultchanged(
      {input: test.realbox.queries[0].input, matches: []});

  assertEquals(0, $(test.realbox.IDS.REALBOX_MATCHES).children.length);
  assertFalse(test.realbox.areMatchesShowing());
};

test.realbox2.testPressEnterOnSelectedMatch = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  const matches = [test.realbox.getSearchMatch({supportsDeletion: true})];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged(
      {input: test.realbox.realboxEl.value, matches});

  const matchEls = $(test.realbox.IDS.REALBOX_MATCHES).children;
  assertEquals(1, matchEls.length);

  // First match is selected.
  assertTrue(matchEls[0].classList.contains(test.realbox.CLASSES.SELECTED));

  let clicked = false;
  matchEls[0].onclick = () => clicked = true;

  const shiftEnter = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Enter',
    target: matchEls[0],
    shiftKey: true,
  });
  test.realbox.realboxEl.dispatchEvent(shiftEnter);
  assertTrue(shiftEnter.defaultPrevented);

  assertTrue(clicked);
  assertEquals(0, test.realbox.opens.length);
};

test.realbox2.testPressEnterNoSelectedMatch = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  const matches =
      [test.realbox.getSearchMatch({allowedToBeDefaultMatch: false})];
  chrome.embeddedSearch.searchBox.autocompleteresultchanged(
      {input: test.realbox.realboxEl.value, matches});

  const matchEls = $(test.realbox.IDS.REALBOX_MATCHES).children;
  assertEquals(1, matchEls.length);

  // First match is not selected.
  assertFalse(matchEls[0].classList.contains(test.realbox.CLASSES.SELECTED));

  let clicked = false;
  matchEls[0].onclick = () => clicked = true;

  const enter = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Enter',
    target: matchEls[0],
  });
  test.realbox.realboxEl.dispatchEvent(enter);
  assertFalse(enter.defaultPrevented);

  assertFalse(clicked);
  assertEquals(0, test.realbox.opens.length);
};

test.realbox2.testArrowDownMovesFocus = function() {
  test.realbox.realboxEl.value = 'hello ';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  test.realbox.realboxEl.focus();

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [
      test.realbox.getSearchMatch({contents: 'hello fresh'}),
      test.realbox.getSearchMatch({contents: 'hello kitty'}),
      test.realbox.getSearchMatch({contents: 'hello dolly'}),
      test.realbox.getUrlMatch(),
    ],
  });

  const matchEls = $(test.realbox.IDS.REALBOX_MATCHES).children;
  assertEquals(4, matchEls.length);

  // First match is selected but does not get the focus.
  assertTrue(matchEls[0].classList.contains(test.realbox.CLASSES.SELECTED));
  assertEquals(document.activeElement, test.realbox.realboxEl);

  const arrowDown = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowDown',
  });
  test.realbox.realboxEl.dispatchEvent(arrowDown);
  assertTrue(arrowDown.defaultPrevented);

  // Arrow up/down while focus is in realbox should not focus matches.
  assertTrue(matchEls[1].classList.contains(test.realbox.CLASSES.SELECTED));
  assertEquals(document.activeElement, test.realbox.realboxEl);

  // Pretend that user moved focus to first match via Tab.
  matchEls[0].focus();
  matchEls[0].dispatchEvent(new Event('focusin', {bubbles: true}));
  assertTrue(matchEls[0].classList.contains(test.realbox.CLASSES.SELECTED));

  const arrowDown2 = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowDown',
  });
  matchEls[0].dispatchEvent(arrowDown2);
  assertTrue(arrowDown2.defaultPrevented);

  // Arrow up/down while focus is in the matches should change focus.
  assertTrue(matchEls[1].classList.contains(test.realbox.CLASSES.SELECTED));
  assertEquals(document.activeElement, matchEls[1])
};

test.realbox2.testPressEnterAfterFocusout = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()],
  });

  const downArrow = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowDown',
  });
  test.realbox.realboxEl.dispatchEvent(downArrow);
  assertTrue(downArrow.defaultPrevented);

  const matchEls = $(test.realbox.IDS.REALBOX_MATCHES).children;
  assertEquals(2, matchEls.length);
  assertTrue(matchEls[1].classList.contains(test.realbox.CLASSES.SELECTED));

  test.realbox.realboxEl.dispatchEvent(new Event('focusout', {
    bubbles: true,
    cancelable: true,
    target: test.realbox.realboxEl,
    relatedTarget: document.body,
  }));

  test.realbox.realboxEl.dispatchEvent(new Event('focusin', {
    bubbles: true,
    cancelable: true,
    target: test.realbox.realboxEl,
  }));

  let clicked = false;
  matchEls[1].onclick = () => clicked = true;

  const enter = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Enter',
  });
  test.realbox.realboxEl.dispatchEvent(enter);
  assertTrue(enter.defaultPrevented);

  assertTrue(clicked);
  assertEquals(0, test.realbox.opens.length);
};

test.realbox2.testInputAfterFocusoutPrefixMatches = function() {
  test.realbox.realboxEl.value = 'hello';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  assertEquals(1, test.realbox.queries.length);

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.queries[0].input,
    matches: [test.realbox.getSearchMatch()],
  });

  assertTrue(test.realbox.areMatchesShowing());

  test.realbox.realboxEl.dispatchEvent(new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowDown',
  }));

  const matchEls = $(test.realbox.IDS.REALBOX_MATCHES).children;
  assertEquals(1, matchEls.length);
  assertTrue(matchEls[0].classList.contains(test.realbox.CLASSES.SELECTED));
  assertEquals('hello world', test.realbox.realboxEl.value);

  test.realbox.realboxEl.dispatchEvent(new Event('focusout', {
    bubbles: true,
    cancelable: true,
    target: test.realbox.realboxEl,
    relatedTarget: document.body,
  }));

  assertFalse(test.realbox.areMatchesShowing());
  assertEquals('hello world', test.realbox.realboxEl.value);
};

test.realbox2.testInputAfterFocusoutZeroPrefixMatches = function() {
  test.realbox.realboxEl.value = '';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  // Empty input doesn't query autocomplete.
  test.realbox.realboxEl.dispatchEvent(new Event('focusin', {
    bubbles: true,
    cancelable: true,
    target: test.realbox.realboxEl,
  }));
  assertEquals(1, test.realbox.queries.length);

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.queries[0].input,
    matches: [test.realbox.getSearchMatch()],
  });

  assertTrue(test.realbox.areMatchesShowing());

  test.realbox.realboxEl.dispatchEvent(new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowDown',
  }));

  const matchEls = $(test.realbox.IDS.REALBOX_MATCHES).children;
  assertEquals(1, matchEls.length);
  assertTrue(matchEls[0].classList.contains(test.realbox.CLASSES.SELECTED));
  assertEquals('hello world', test.realbox.realboxEl.value);

  test.realbox.realboxEl.dispatchEvent(new Event('focusout', {
    bubbles: true,
    cancelable: true,
    target: test.realbox.realboxEl,
    relatedTarget: document.body,
  }));

  assertFalse(test.realbox.areMatchesShowing());
  assertEquals('', test.realbox.realboxEl.value);
};

test.realbox2.testArrowUpDownShowsMatchesWhenHidden = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  assertEquals(1, test.realbox.queries.length);
  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()],
  });

  test.realbox.realboxEl.dispatchEvent(new Event('focusout', {
    bubbles: true,
    cancelable: true,
    target: test.realbox.realboxEl,
    relatedTarget: document.body,
  }));

  assertFalse(test.realbox.areMatchesShowing());

  test.realbox.realboxEl.dispatchEvent(new Event('focusin', {
    bubbles: true,
    cancelable: true,
    target: test.realbox.realboxEl,
  }));

  const arrowDown = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowDown',
  });
  test.realbox.realboxEl.dispatchEvent(arrowDown);
  assertTrue(arrowDown.defaultPrevented);

  assertFalse(test.realbox.areMatchesShowing());

  assertEquals(2, test.realbox.queries.length);
  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()],
  });

  assertTrue(test.realbox.areMatchesShowing());
};

// Test that trying to open e.g. chrome:// links goes through the mojo API.
test.realbox2.testPrivilegedDestinationUrls = function() {
  test.realbox.realboxEl.value = 'about';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.autocompleteresultchanged({
    input: test.realbox.realboxEl.value,
    matches: [
      test.realbox.getUrlMatch({
        canDisplay: false,
        destinationUrl: 'chrome://settings/',
        supportsDeletion: true,
      }),
    ],
  });

  const matchEls = $(test.realbox.IDS.REALBOX_MATCHES).children;
  assertEquals(1, matchEls.length);

  const target = matchEls[0];
  matchEls[0].onclick(test.realbox.trustedEventFacade('click', {target}));
  // Accept left clicks.
  assertEquals(1, test.realbox.opens.length);
  assertEquals('chrome://settings/', test.realbox.opens[0].url);

  // Accept middle clicks.
  const middleClick =
      test.realbox.trustedEventFacade('auxclick', {button: 1, target});
  matchEls[0].onauxclick(middleClick);
  assertTrue(middleClick.defaultPrevented);
  assertEquals(2, test.realbox.opens.length);

  // Ignore right clicks.
  const rightClick =
      test.realbox.trustedEventFacade('auxclick', {button: 2, target});
  matchEls[0].onauxclick(rightClick);
  assertFalse(rightClick.defaultPrevented);
  assertEquals(2, test.realbox.opens.length);

  // Accept 'Enter' keypress.
  const enter = new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'Enter',
  });
  test.realbox.realboxEl.dispatchEvent(enter);
  assertTrue(enter.defaultPrevented);
  assertEquals(3, test.realbox.opens.length);

  // Ensure clicking remove icon doesn't accidentally trigger navigation.
  assertEquals(0, test.realbox.deletedLines.length);
  matchEls[0].querySelector(`.${test.realbox.CLASSES.REMOVE_ICON}`).click();
  assertEquals(1, test.realbox.deletedLines.length);
  assertEquals(3, test.realbox.opens.length);
};
