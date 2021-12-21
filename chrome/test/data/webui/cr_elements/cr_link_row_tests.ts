// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('cr-link-row', function() {
  let linkRow: CrLinkRowElement;

  setup(() => {
    document.body.innerHTML = '<cr-link-row></cr-link-row>';
    linkRow = document.body.querySelector('cr-link-row')!;
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
});
