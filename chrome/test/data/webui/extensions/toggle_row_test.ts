// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import type {ExtensionsToggleRowElement} from 'chrome://extensions/extensions.js';
import {getTrustedHTML} from 'chrome://extensions/extensions.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('extensions-toggle-row', function() {
  let row: ExtensionsToggleRowElement;

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
      <extensions-toggle-row id="row">
        <span id="label">Description</span>
        <span id="icon" slot="icon">Icon</span>
      </extensions-toggle-row>
    `;

    row = document.querySelector('extensions-toggle-row')!;
    assertFalse(row.checked);
  });


  // Test that the control is toggled when the user taps on the text label.
  test('TestToggleByLabelTap', async function() {
    let whenChanged = eventToPromise('change', row);
    row.getLabel().click();
    await whenChanged;
    assertTrue(row.checked);

    whenChanged = eventToPromise('change', row);
    row.getLabel().click();
    await whenChanged;
    assertFalse(row.checked);
  });

  // Test that when the row is disabled, clicking the label or cr-toggle does
  // not toggle the checked state or fire a change event.
  test('DisabledRowDoesNotToggle', async function() {
    row.disabled = true;
    await microtasksFinished();
    assertTrue(row.$.native.disabled);
    assertTrue(row.$.crToggle.disabled);

    let changeFired = false;
    row.addEventListener('change', () => {
      changeFired = true;
    });

    row.getLabel().click();
    row.$.crToggle.click();
    await microtasksFinished();
    assertFalse(row.checked);
    assertFalse(changeFired);
  });

  // Test that the icon slot is outside the label and clicking a slotted icon
  // does not toggle the row.
  test('IconSlotDoesNotToggle', async function() {
    const iconSlot = row.shadowRoot.querySelector('slot[name="icon"]');
    assertTrue(!!iconSlot);
    assertFalse(row.getLabel().contains(iconSlot));

    let changeFired = false;
    row.addEventListener('change', () => {
      changeFired = true;
    });

    row.querySelector<HTMLElement>('#icon')!.click();
    await microtasksFinished();
    assertFalse(row.checked);
    assertFalse(changeFired);
  });
});
