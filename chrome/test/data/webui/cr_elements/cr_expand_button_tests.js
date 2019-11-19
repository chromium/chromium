// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
// clang-format on

suite('cr-expand-button', function() {
  let button;

  setup(() => {
    PolymerTest.clearBody();
    document.body.innerHTML = '<cr-expand-button></cr-expand-button>';
    button = document.body.querySelector('cr-expand-button');
  });

  test('setting |alt| label', () => {
    const iconButton = button.$.icon;
    assertFalse(!!button.alt);
    assertEquals('label', iconButton.getAttribute('aria-labelledby'));
    assertEquals(null, iconButton.getAttribute('aria-label'));
    const altLabel = 'alt label';
    button.alt = altLabel;
    assertEquals(null, iconButton.getAttribute('aria-labelledby'));
    assertEquals('alt label', iconButton.getAttribute('aria-label'));
  });

  test('changing |expanded|', () => {
    const iconButton = button.$.icon;
    assertFalse(button.expanded);
    assertEquals('false', iconButton.getAttribute('aria-expanded'));
    assertEquals('cr:expand-more', iconButton.ironIcon);
    button.expanded = true;
    assertEquals('true', iconButton.getAttribute('aria-expanded'));
    assertEquals('cr:expand-less', iconButton.ironIcon);
  });

  test('changing |disabled|', () => {
    const iconButton = button.$.icon;
    assertFalse(button.disabled);
    assertEquals('false', iconButton.getAttribute('aria-expanded'));
    assertFalse(iconButton.disabled);
    button.disabled = true;
    assertFalse(iconButton.hasAttribute('aria-expanded'));
    assertTrue(iconButton.disabled);
  });
});
