// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewAdvancedSettingsItemElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {getCddTemplateWithAdvancedSettings} from './print_preview_test_utils.js';

suite('AdvancedItemTest', function() {
  let item: PrintPreviewAdvancedSettingsItemElement;

  /** @override */
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const model: PrintPreviewModelElement =
        document.createElement('print-preview-model');
    document.body.appendChild(model);

    item = document.createElement('print-preview-advanced-settings-item');

    // Create capability.
    item.capability = getCddTemplateWithAdvancedSettings(2, 'FooDevice')
                          .capabilities!.printer.vendor_capability![1]!;
    item.settings = model.settings;
    fakeDataBind(model, item, 'settings');
    model.set('settings.vendorItems.available', true);

    document.body.appendChild(item);
    flush();
  });

  // Test that a select capability is displayed correctly.
  test('DisplaySelect', function() {
    const label = item.shadowRoot!.querySelector('.label')!;
    assertEquals('Paper Type', label.textContent);

    // Check that the default option is selected.
    const select = item.shadowRoot!.querySelector('select')!;
    assertEquals(0, select.selectedIndex);
    assertEquals('Standard', select.options[0]!.textContent!.trim());
    assertEquals('Recycled', select.options[1]!.textContent!.trim());
    assertEquals('Special', select.options[2]!.textContent!.trim());

    // Don't show input or checkbox.
    assertTrue(
        item.shadowRoot!.querySelector('cr-input')!.parentElement!.hidden);
    assertTrue(
        item.shadowRoot!.querySelector('cr-checkbox')!.parentElement!.hidden);
  });

  test('DisplayInput', function() {
    // Create capability
    item.capability = getCddTemplateWithAdvancedSettings(3, 'FooDevice')
                          .capabilities!.printer.vendor_capability![2]!;
    flush();

    const label = item.shadowRoot!.querySelector('.label')!;
    assertEquals('Watermark', label.textContent);

    // The input should be shown.
    const input = item.shadowRoot!.querySelector('cr-input')!;
    assertFalse(input.parentElement!.hidden);
    assertEquals('', input.inputElement.value);

    // Don't show select or checkbox.
    assertEquals(null, item.shadowRoot!.querySelector('select'));
    assertTrue(
        item.shadowRoot!.querySelector('cr-checkbox')!.parentElement!.hidden);
  });

  test('DisplayCheckbox', function() {
    // Create capability
    item.capability = getCddTemplateWithAdvancedSettings(4, 'FooDevice')
                          .capabilities!.printer.vendor_capability![3]!;
    flush();

    const label = item.shadowRoot!.querySelector('.label')!;
    assertEquals('Staple', label.textContent);

    // The checkbox should be shown.
    const checkbox = item.shadowRoot!.querySelector('cr-checkbox')!;
    assertFalse(checkbox.parentElement!.hidden);
    assertFalse(checkbox.checked);

    // Don't show select or input.
    assertEquals(null, item.shadowRoot!.querySelector('select'));
    assertTrue(
        item.shadowRoot!.querySelector('cr-input')!.parentElement!.hidden);
  });

  // Test that a select capability updates correctly when the setting is
  // updated (e.g. when sticky settings are set).
  test('UpdateSelect', function() {
    // Check that the default option is selected.
    const select = item.shadowRoot!.querySelector('select')!;
    assertEquals(0, select.selectedIndex);

    // Update the setting.
    item.set('settings.vendorItems.value', {paperType: 1});
    assertEquals(1, select.selectedIndex);
  });

  // Test that an input capability updates correctly when the setting is
  // updated (e.g. when sticky settings are set).
  test('UpdateInput', async () => {
    // Create capability
    item.capability = getCddTemplateWithAdvancedSettings(3, 'FooDevice')
                          .capabilities!.printer.vendor_capability![2]!;
    flush();

    // Check that the default value is set.
    const input = item.shadowRoot!.querySelector('cr-input')!;
    assertEquals('', input.inputElement.value);

    // Update the setting.
    item.set('settings.vendorItems.value', {watermark: 'ABCD'});
    await input.updateComplete;
    assertEquals('ABCD', input.inputElement.value);
  });

  // Test that an checkbox capability updates correctly when the setting is
  // updated (e.g. when sticky settings are set).
  test('UpdateCheckbox', function() {
    // Create capability
    item.capability = getCddTemplateWithAdvancedSettings(4, 'FooDevice')
                          .capabilities!.printer.vendor_capability![3]!;
    flush();

    // Check that checkbox is unset.
    const checkbox = item.shadowRoot!.querySelector('cr-checkbox');
    assertFalse(checkbox!.checked);

    // Update the setting.
    item.set('settings.vendorItems.value', {'finishings/4': 'true'});
    assertTrue(checkbox!.checked);
  });

  // Test that the setting is displayed correctly when the search query
  // matches its display name.
  test('QueryName', function() {
    const query = /(Type)/ig;
    assertTrue(item.hasMatch(query));
    item.updateHighlighting(query, new Map());

    const label = item.shadowRoot!.querySelector('.label')!;
    assertEquals(
        item.capability.display_name! + item.capability.display_name!,
        label.textContent);

    // Label should be highlighted.
    const searchHits = label.querySelectorAll('.search-highlight-hit');
    assertEquals(1, searchHits.length);
    assertEquals('Type', searchHits[0]!.textContent);

    // No highlighting on the control.
    const control = item.shadowRoot!.querySelector('.value')!;
    assertEquals(0, control.querySelectorAll('.search-highlight-hit').length);
    assertEquals(0, control.querySelectorAll('.search-bubble').length);
  });

  // Test that the setting is displayed correctly when the search query
  // matches one of the select options.
  test('QueryOption', function() {
    const query = /(cycle)/ig;
    assertTrue(item.hasMatch(query));
    item.updateHighlighting(query, new Map());

    const label = item.shadowRoot!.querySelector('.label')!;
    assertEquals('Paper Type', label.textContent);

    // Label should not be highlighted.
    assertEquals(0, label.querySelectorAll('.search-highlight-hit').length);

    // Control should have highlight bubble but no highlighting.
    const control = item.shadowRoot!.querySelector('.value')!;
    assertEquals(0, control.querySelectorAll('.search-highlight-hit').length);
    const searchBubbleHits = control.querySelectorAll('.search-bubble');
    assertEquals(1, searchBubbleHits.length);
    assertEquals('1 result', searchBubbleHits[0]!.textContent);
  });

  // Test that certain Japanese characters can be searched after normalizing
  // and removing diacritics (see b/353394050).
  test('QueryJapaneseCharacters', function() {
    const settingName = 'ジョブシート';
    item.capability = {
      display_name: settingName,
      id: 'capability',
      type: 'SELECT',
      select_cap: {
        option: [],
      },
    };

    const queryText = stripDiacritics(settingName);
    const query = new RegExp(`(${queryText})`, 'ig');
    assertTrue(item.hasMatch(query));
  });
});
