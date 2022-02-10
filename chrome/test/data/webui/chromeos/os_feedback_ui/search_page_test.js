// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeHelpContentList} from 'chrome://os-feedback/fake_data.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {HelpContentElement} from 'chrome://os-feedback/help_content.js';
import {setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {SearchPageElement} from 'chrome://os-feedback/search_page.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function searchPageTestSuite() {
  /** @type {?SearchPageElement} */
  let page = null;

  /** @type {?FakeHelpContentProvider} */
  let provider = null;

  setup(() => {
    document.body.innerHTML = '';
    // Create provider.
    provider = new FakeHelpContentProvider();
    // Setup help contents.
    provider.setFakeHelpContents(fakeHelpContentList);
    // Set the fake provider.
    setHelpContentProviderForTesting(provider);
  });

  teardown(() => {
    page.remove();
    page = null;
    provider = null;
  });

  function initializePage() {
    assertFalse(!!page);
    page =
        /** @type {!SearchPageElement} */ (
            document.createElement('search-page'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /** Test that expected html elements are in the page after loaded. */
  test('SearchPageLoaded', () => {
    return initializePage().then(() => {
      // Verify the title is in the page.
      const title = page.shadowRoot.querySelector('#title');
      assertTrue(!!title);
      assertEquals('Send feedback', title.textContent);

      // Verify the help content is not in the page. For security reason, help
      // contents fetched online can't be displayed in trusted context.
      const helpContent = page.shadowRoot.querySelector('help-content');
      assertFalse(!!helpContent);

      // Verify the iframe is in the page.
      const untrustedFrame = page.shadowRoot.querySelector('iframe');
      assertTrue(!!untrustedFrame);
      assertEquals(
          'chrome-untrusted://os-feedback/untrusted_index.html',
          untrustedFrame.src);

      // Verify the continue button is in the page.
      const btnContinue = page.shadowRoot.querySelector('#btnContinue');
      assertTrue(!!btnContinue);
    });
  });

  /**
   * Test that the text area accepts input and may fire search query to retrieve
   * help contents.
   * - Case 1: When number of characters newly entered is less than 3, search is
   *   not triggered.
   * - Case 2: When number of characters newly entered is 3 or more, search is
   *   triggered and help contents are populated.
   */
  test('HelpContentPopulated', () => {
    /** {?Element} */
    let textAreaElement = null;

    return initializePage()
        .then(() => {
          textAreaElement = page.shadowRoot.querySelector('#descriptionText');
          assertTrue(!!textAreaElement);
          // Verify the textarea is empty.
          assertEquals('', textAreaElement.value);

          // Enter three chars.
          textAreaElement.value = 'abc';
          // Setting the value of the textarea in code does not trigger the
          // input event. So we trigger it here.
          textAreaElement.dispatchEvent(new Event('input'));

          return flushTasks();
        })
        .then(() => {
          // Verify that getHelpContent() has been called with query 'abc'.
          assertEquals('abc', provider.lastQuery);

          // Enter 2 more characters. This should NOT trigger another search.
          textAreaElement.value = 'abc12';
          textAreaElement.dispatchEvent(new Event('input'));

          return flushTasks();
        })
        .then(() => {
          // Verify that getHelpContent() has NOT been called with query
          // 'abc12'.
          assertNotEquals('abc12', provider.lastQuery);

          // Enter one more characters. This should trigger another search.
          textAreaElement.value = 'abc123';
          textAreaElement.dispatchEvent(new Event('input'));

          return flushTasks();
        })
        .then(() => {
          // Verify that getHelpContent() has been called with query 'abc123'.
          assertEquals('abc123', provider.lastQuery);
        });
  });
}
