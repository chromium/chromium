// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
// clang-format on

suite('cr-link-row', function() {
  let linkRow;

  setup(() => {
    PolymerTest.clearBody();
    document.body.innerHTML = '<cr-link-row></cr-link-row>';
    linkRow = document.body.querySelector('cr-link-row');
  });

  test('check label visibility', () => {
    assertTrue(linkRow.$.labelWrapper.hidden);
    linkRow.usingSlottedLabel = true;
    assertFalse(linkRow.$.labelWrapper.hidden);
    linkRow.usingSlottedLabel = false;
    assertTrue(linkRow.$.labelWrapper.hidden);
    linkRow.label = 'label';
    assertFalse(linkRow.$.labelWrapper.hidden);
  });

  test('icon', () => {
    const iconButton = linkRow.$.icon;
    assertFalse(linkRow.external);
    assertEquals('cr:arrow-right', iconButton.ironIcon);
    linkRow.external = true;
    assertEquals('cr:open-in-new', iconButton.ironIcon);
  });

  test('role description', () => {
    const iconButton = linkRow.$.icon;
    assertEquals(undefined, linkRow.roleDescription);
    assertEquals(null, iconButton.getAttribute('aria-roledescription'));
    const description = 'self destruct button';
    linkRow.roleDescription = description;
    assertEquals(description, iconButton.getAttribute('aria-roledescription'));
  });
});
