// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';

import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLinkRowElement} from 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('cr-link-row', function() {
  let linkRow: CrLinkRowElement;

  suiteSetup(() => {
    loadTimeData.resetForTesting({opensInNewTab: 'Opens in new tab'});
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    linkRow = document.createElement('cr-link-row');
    document.body.appendChild(linkRow);
  });

  test('check label visibility', () => {
    const labelWrapper =
        linkRow.shadowRoot!.querySelector<HTMLElement>('#labelWrapper')!;
    assertTrue(labelWrapper.hidden);
    linkRow.usingSlottedLabel = true;
    assertFalse(labelWrapper.hidden);
    linkRow.usingSlottedLabel = false;
    assertTrue(labelWrapper.hidden);
    linkRow.label = 'label';
    assertFalse(labelWrapper.hidden);
  });

  test('icon', () => {
    const iconButton =
        linkRow.shadowRoot!.querySelector<CrIconButtonElement>('#icon')!;
    assertFalse(linkRow.external);
    assertEquals('cr:arrow-right', iconButton.ironIcon);
    linkRow.external = true;
    assertEquals('cr:open-in-new', iconButton.ironIcon);
  });

  test('role description', () => {
    const iconButton = linkRow.shadowRoot!.querySelector('#icon')!;
    assertEquals(undefined, linkRow.roleDescription);
    assertEquals(null, iconButton.getAttribute('aria-roledescription'));
    const description = 'self destruct button';
    linkRow.roleDescription = description;
    assertEquals(description, iconButton.getAttribute('aria-roledescription'));
  });

  test('button aria description', () => {
    const buttonAriaDescription = linkRow.$.buttonAriaDescription;
    const defaultString = 'Opens in new tab';
    const customString = 'Opens in new window';

    assertEquals('', buttonAriaDescription.textContent!.trim());

    linkRow.external = true;
    assertEquals(defaultString, buttonAriaDescription.textContent!.trim());

    linkRow.buttonAriaDescription = customString;
    assertEquals(customString, buttonAriaDescription.textContent!.trim());

    linkRow.buttonAriaDescription = '';
    assertEquals('', buttonAriaDescription.textContent!.trim());
  });
});
