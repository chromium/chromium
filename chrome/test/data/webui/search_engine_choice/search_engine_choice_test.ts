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

  /**
   * Async spin until predicate() returns true.
   */
  function waitFor(predicate: () => boolean): Promise<void> {
    if (predicate()) {
      return Promise.resolve();
    }
    return new Promise(resolve => setTimeout(() => {
                         resolve(waitFor(predicate));
                       }, 0));
  }

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

  // This tests both forced scroll when clicking on the "More" button and
  // manually scrolling because the test will trigger the same scroll event.
  test('Action button changes state correctly on click', async function() {
    const actionButton = testElement.$.actionButton;

    // Test that the action button text is "More" and that it is initially
    // enabled.
    assertFalse(actionButton.disabled);
    assertEquals(
        actionButton.textContent!.trim(), testElement.i18n('moreButtonText'));

    // The action button text should become "Set as default" after being clicked
    // but still be disabled because we haven't yet made a choice.
    actionButton.click();
    await waitFor(
        () => actionButton.textContent!.trim() ===
            testElement.i18n('submitButtonText'));
    assertTrue(actionButton.disabled);

    // The action button should be enabled after making a choice.
    selectChoice();
    assertFalse(actionButton.disabled);

    actionButton.click();
    assertEquals(handler.getCallCount('handleSearchEngineChoiceSelected'), 1);
  });

  test('Click learn more link', function() {
    testElement.$.infoLink.click();
    assertTrue(testElement.$.infoDialog.open);
    assertEquals(handler.getCallCount('handleLearnMoreLinkClicked'), 1);
  });
});
