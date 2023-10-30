// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://search-engine-choice/app.js';

import {SearchEngineChoiceAppElement} from 'chrome://search-engine-choice/app.js';
import {SearchEngineChoiceBrowserProxy} from 'chrome://search-engine-choice/browser_proxy.js';
import {PageHandlerRemote} from 'chrome://search-engine-choice/search_engine_choice.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('SearchEngineChoiceTest', function() {
  let testElement: SearchEngineChoiceAppElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    SearchEngineChoiceBrowserProxy.setInstance(
        new SearchEngineChoiceBrowserProxy(handler));

    testElement = document.createElement('search-engine-choice-app');
    document.body.appendChild(testElement);
    return waitBeforeNextRender(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  // Selects the first search engine from the list of search engine choices.
  function selectChoice() {
    const radioButtons =
        testElement.shadowRoot!.querySelectorAll('cr-radio-button');

    assertTrue(radioButtons.length > 0);
    radioButtons[0]!.click();
  }

  test('Submit button enabled on choice click', function() {
    assertTrue(testElement.$.submitButton.disabled);

    selectChoice();
    assertFalse(testElement.$.submitButton.disabled);
  });

  test('Clicking submit button calls correct function', function() {
    // Select a search engine to enable the submit button.
    selectChoice();

    testElement.$.submitButton.click();
    assertEquals(handler.getCallCount('handleSearchEngineChoiceSelected'), 1);
  });

  test('Click learn more link', function() {
    testElement.$.infoLink.click();

    assertTrue(testElement.$.infoDialog.open);
    assertEquals(handler.getCallCount('handleLearnMoreLinkClicked'), 1);
  });
});
