// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';

import type {CrChipElement} from 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-chip', function() {
  let crChip: CrChipElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    crChip = document.createElement('cr-chip');
    document.body.appendChild(crChip);
  });

  function assertSelected() {
    assertTrue(crChip.selected);
    assertTrue(crChip.$.button.hasAttribute('selected'));
    assertEquals('true', crChip.$.button.getAttribute('aria-pressed'));
  }

  function assertNotSelected() {
    assertFalse(crChip.selected);
    assertFalse(crChip.$.button.hasAttribute('selected'));
    assertEquals('false', crChip.$.button.getAttribute('aria-pressed'));
  }

  function assertDisabled() {
    assertTrue(crChip.disabled);
    assertTrue(crChip.$.button.disabled);
  }

  function assertNotDisabled() {
    assertFalse(crChip.disabled);
    assertFalse(crChip.$.button.disabled);
  }

  test('Selected', async () => {
    assertNotSelected();
    crChip.selected = true;
    await microtasksFinished();
    assertSelected();
    crChip.selected = false;
    await microtasksFinished();
    assertNotSelected();
  });

  test('Disabled', async () => {
    assertNotDisabled();
    crChip.disabled = true;
    await microtasksFinished();
    assertDisabled();
    crChip.disabled = false;
    await microtasksFinished();
    assertNotDisabled();
  });

  test('Ripple', function() {
    assertFalse(!!crChip.shadowRoot!.querySelector('#ink'));
    crChip.dispatchEvent(
        new CustomEvent('pointerdown', {bubbles: true, composed: true}));
    assertTrue(!!crChip.shadowRoot!.querySelector('#ink'));
  });
});
