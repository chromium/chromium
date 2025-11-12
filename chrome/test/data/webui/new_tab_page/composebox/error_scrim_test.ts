// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ErrorScrimElement} from 'chrome://new-tab-page/lazy_load.js';
import type{CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabPageErrorScrimTest', () => {
  let errorScrimElement: ErrorScrimElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    errorScrimElement = new ErrorScrimElement();
    document.body.appendChild(errorScrimElement);
  });

  test('Both the error scrim and dismiss button are visible', async () => {
    // Assert initial state.
    const initialScrim =
        errorScrimElement.shadowRoot.querySelector('#errorScrim');
    assertEquals(initialScrim, null);

    // Act.
    const emptyFileErrorMessage = 'Can\'t upload. File appears to be empty.';
    errorScrimElement.setErrorMessage(emptyFileErrorMessage);
    await microtasksFinished();

    // Assert.
    const errorScrim =
        errorScrimElement.shadowRoot.querySelector('#errorScrim');
    assertTrue(!!errorScrim);

    // Assert: error scrim should be displayed correctly.
    const errorMessageElement = errorScrim.querySelector('#errorMessage');
    assertTrue(!!errorMessageElement);
    assertEquals(errorMessageElement.textContent, emptyFileErrorMessage);

    // Assert: Dismiss button should be visible and available.
    const dismissButton =
        errorScrim.querySelector<CrButtonElement>('#dismissErrorButton');
    assertTrue(!!dismissButton);
    assertFalse(dismissButton.disabled);
  });

  test(
      'Error scrim should disappear after clicking dismiss button',
      async () => {
        // Initial act: display error scrim.
        const emptyFileErrorMessage =
            'Can\'t upload. File appears to be empty.';
        errorScrimElement.setErrorMessage(emptyFileErrorMessage);
        await microtasksFinished();

        // Assert Initial state.
        const initialScrim =
            errorScrimElement.shadowRoot.querySelector('#errorScrim');
        assertTrue(!!initialScrim);

        // Act.
        const dismissButton =
            initialScrim.querySelector<CrButtonElement>('#dismissErrorButton')!;
        dismissButton.click();
        await microtasksFinished();

        // Assert final state.
        const finalScrim =
            errorScrimElement.shadowRoot.querySelector('#errorScrim');
        assertEquals(finalScrim, null);
        await microtasksFinished();

        // Assert: Component internal state should be reset.
        const anyParagraphs =
            errorScrimElement.shadowRoot.querySelectorAll('#errorMessage');
        assertEquals(anyParagraphs.length, 0);
        const anyButtons =
            errorScrimElement.shadowRoot.querySelector('#dismissErrorButton')!;
        assertEquals(anyButtons, null);
      });
});
