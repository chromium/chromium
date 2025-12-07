// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewAdvancedSettingsDialogElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {getCddTemplateWithAdvancedSettings} from './print_preview_test_utils.js';

suite('AdvancedDialogTest', function() {
  let dialog: PrintPreviewAdvancedSettingsDialogElement;
  let destination: Destination;
  const printerId: string = 'FooDevice';
  const printerName: string = 'FooName';

  setup(function() {
    // Create destination
    destination =
        new Destination(printerId, DestinationOrigin.EXTENSION, printerName);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const model: PrintPreviewModelElement =
        document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.set('settings.vendorItems.available', true);
    dialog = document.createElement('print-preview-advanced-settings-dialog');

    // Set up settings. Only need the vendor items.
    dialog.settings = model.settings;
    fakeDataBind(model, dialog, 'settings');
  });

  /**
   * Sets up the advanced settings dialog with |count| items.
   */
  function setupDialog(count: number) {
    const template =
        getCddTemplateWithAdvancedSettings(count, printerId, printerName);
    destination.capabilities = template.capabilities;
    dialog.destination = destination;

    document.body.appendChild(dialog);

    flush();
  }

  /**
   * Verifies that the search box is shown or hidden based on the number
   * of capabilities and that the correct number of items are created.
   * @param count The number of vendor capabilities the printer should have.
   */
  function verifyListWithItemCount(count: number) {
    // Search box should be hidden if there is only 1 item.
    const searchBox = dialog.$.searchBox;
    assertEquals(count === 1, searchBox.hidden);

    // Verify item is displayed.
    const items = dialog.shadowRoot!.querySelectorAll(
        'print-preview-advanced-settings-item');
    assertEquals(count, items.length);
  }

  /**
   * Sets some non-default values for the advanced items. Assumes there are 3
   * capabilities.
   */
  function setItemValues() {
    const items = dialog.shadowRoot!.querySelectorAll(
        'print-preview-advanced-settings-item');
    assertEquals(3, items.length);

    // Set some values.
    items[0]!.setCurrentValueForTest('6');
    items[1]!.setCurrentValueForTest('1');
    items[2]!.setCurrentValueForTest('XYZ');
  }

  // Tests that the search box does not appear when there is only one option,
  // and that the vendor item is correctly displayed.
  test('AdvancedSettings1Option', function() {
    setupDialog(1);
    verifyListWithItemCount(1);
  });

  // Tests that the search box appears when there are two options, and that
  // the items are correctly displayed.
  test('AdvancedSettings2Options', function() {
    setupDialog(2);
    verifyListWithItemCount(2);
  });

  // Tests that the advanced settings dialog correctly updates the settings
  // value for vendor items when the apply button is clicked.
  test(
      'AdvancedSettingsApply', function() {
        setupDialog(3);
        setItemValues();

        assertFalse(dialog.getSetting('vendorItems').setFromUi);
        const buttons = dialog.shadowRoot!.querySelectorAll('cr-button');
        assertEquals(2, buttons.length);
        const whenDialogClose = eventToPromise('close', dialog);

        // Click apply button.
        buttons[1]!.click();
        return whenDialogClose.then(() => {
          // Check that the setting has been set.
          const setting = dialog.getSettingValue('vendorItems');
          assertEquals('6', setting.printArea);
          assertEquals('1', setting.paperType);
          assertEquals('XYZ', setting.watermark);
          assertTrue(dialog.getSetting('vendorItems').setFromUi);
        });
      });

  // Tests that the advanced settings dialog updates the settings value for
  // vendor items if Enter is pressed on a cr-input.
  test(
      'AdvancedSettingsApplyWithEnter', function() {
        setupDialog(3);
        setItemValues();

        const items = dialog.shadowRoot!.querySelectorAll(
            'print-preview-advanced-settings-item');
        const typedItemInput =
            items[2]!.shadowRoot!.querySelector('cr-input')!;  // Watermark

        // Simulate typing a value and then pressing enter.
        typedItemInput.value = 'Hello World';
        typedItemInput.dispatchEvent(
            new CustomEvent('input', {composed: true, bubbles: true}));

        const whenDialogClose = eventToPromise('close', dialog);
        keyEventOn(typedItemInput, 'keydown', 13, [], 'Enter');

        return whenDialogClose.then(() => {
          // Check that the setting has been set.
          const setting = dialog.getSettingValue('vendorItems');
          assertEquals('6', setting.printArea);
          assertEquals('1', setting.paperType);
          assertEquals('Hello World', setting.watermark);
        });
      });

  // Tests that the advanced settings dialog does not update the settings
  // value for vendor items when the close button is clicked.
  test(
      'AdvancedSettingsClose', function() {
        setupDialog(3);
        setItemValues();

        const buttons =
            dialog.shadowRoot!.querySelectorAll<HTMLElement>('cr-button');
        assertEquals(2, buttons.length);
        const whenDialogClose = eventToPromise('close', dialog);

        // Click close button.
        buttons[0]!.click();
        return whenDialogClose.then(() => {
          // Check that the setting has not been set.
          const setting = dialog.getSettingValue('vendorItems');
          assertEquals(undefined, setting.printArea);
          assertEquals(undefined, setting.paperType);
          assertEquals(undefined, setting.watermark);
        });
      });

  // Tests that the dialog correctly shows and hides settings based on the
  // value of the search query.
  test(
      'AdvancedSettingsFilter', function() {
        setupDialog(3);
        const searchBox = dialog.$.searchBox;
        const items = dialog.shadowRoot!.querySelectorAll(
            'print-preview-advanced-settings-item');
        const noMatchHint = dialog.shadowRoot!.querySelector<HTMLElement>(
            '.no-settings-match-hint')!;

        // Query is initialized to null. All items are shown and the hint is
        // hidden.
        items.forEach(item => assertFalse(item.hidden));
        assertTrue(noMatchHint.hidden);

        // Searching for Watermark should show only the watermark setting.
        searchBox.searchQuery = /(Watermark)/ig;
        items.forEach((item, index) => assertEquals(index !== 2, item.hidden));
        assertTrue(noMatchHint.hidden);

        // Searching for A4 should show only the print area setting.
        searchBox.searchQuery = /(A4)/ig;
        items.forEach((item, index) => assertEquals(index !== 0, item.hidden));
        assertTrue(noMatchHint.hidden);

        // Searching for WXYZ should show no settings and display the "no match"
        // hint.
        searchBox.searchQuery = /(WXYZ)/ig;
        items.forEach(item => assertTrue(item.hidden));
        assertFalse(noMatchHint.hidden);
      });
});
