// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConfirmationPageElement} from 'chrome://os-feedback/confirmation_page.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

export function confirmationPageTest() {
  /** @type {?ConfirmationPageElement} */
  let component = null;

  setup(() => {
    document.body.innerHTML = '';
    component = /** @type {!ConfirmationPageElement} */ (
        document.createElement('confirmation-page'));
    document.body.appendChild(component);
  });

  teardown(() => {
    component.remove();
    component = null;
  });

  /**TODO(xiangdongkong): test user actions */
  test('confirmationPageLoaded', () => {
    assertEquals(
        'Thanks for your feedback',
        component.shadowRoot.querySelector('#title').textContent);
    assertTrue(!!component.shadowRoot.querySelector('#message'));

    // verify navigation buttons exist
    const doneButton = component.shadowRoot.querySelector('#buttonDone');
    const startNewButton =
        component.shadowRoot.querySelector('#buttonNewReport');
    assertTrue(!!doneButton);
    assertTrue(!!startNewButton);

    // verify help resources exist
    const helpResourcesSection =
        component.shadowRoot.querySelector('#helpResources');
    assertTrue(!!helpResourcesSection);
    assertEquals(
        'Here are some other helpful resources:',
        component.shadowRoot.querySelector('#helpResourcesLabel').textContent);
    const helpLinks = helpResourcesSection.querySelectorAll('cr-link-row');
    assertTrue(!!helpLinks);
    assertEquals(helpLinks.length, 3);

    assertTrue(!!helpLinks[0].shadowRoot.querySelector('#startIcon'));
    assertTrue(!!helpLinks[0].shadowRoot.querySelector('#subLabel'));
    const exploreLabel = helpLinks[0].shadowRoot.querySelector('#label');
    assertTrue(!!exploreLabel);
    assertEquals(exploreLabel.textContent.trim(), 'Explore app');

    assertTrue(!!helpLinks[1].shadowRoot.querySelector('#startIcon'));
    assertTrue(!!helpLinks[1].shadowRoot.querySelector('#subLabel'));
    const diagnosticsLabel = helpLinks[1].shadowRoot.querySelector('#label');
    assertTrue(!!diagnosticsLabel);
    assertEquals(diagnosticsLabel.textContent.trim(), 'Diagnostics app');

    assertTrue(!!helpLinks[2].shadowRoot.querySelector('#startIcon'));
    assertTrue(!!helpLinks[2].shadowRoot.querySelector('#subLabel'));
    const communityLabel = helpLinks[2].shadowRoot.querySelector('#label');
    assertTrue(!!communityLabel);
    assertEquals(communityLabel.textContent.trim(), 'Chromebook community');
  });
}
