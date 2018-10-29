// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('advanced_dialog_test', function() {
  /** @enum {string} */
  const TestNames = {
    AdvancedSettings1Option: 'advanced settings 1 option',
    AdvancedSettings2Options: 'advanced settings 2 options',
    AdvancedSettingsApply: 'advanced settings apply',
    AdvancedSettingsClose: 'advanced settings close',
    AdvancedSettingsFilter: 'advanced settings filter',
  };

  const suiteName = 'AdvancedDialogTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAdvancedSettingsDialogElement} */
    let dialog = null;

    /** @type {?print_preview.Destination} */
    let destination = null;

    /** @type {string} */
    let printerId = 'FooDevice';

    /** @type {string} */
    let printerName = 'FooName';

    /** @override */
    setup(function() {
      // Create destination
      destination = new print_preview.Destination(
          printerId, print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, printerName,
          true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      PolymerTest.clearBody();
      dialog = document.createElement('print-preview-advanced-dialog');

      // Set up settings. Only need the vendor items.
      dialog.settings = {
        vendorItems: {
          value: {},
          unavailableValue: {},
          valid: true,
          available: true,
          key: 'vendorOptions',
        },
      };
    });

    /**
     * Sets up the advanced settings dialog with |count| items.
     * @param {number} count
     */
    function setupDialog(count) {
      const template =
          print_preview_test_utils.getCddTemplateWithAdvancedSettings(
              count, printerId, printerName);
      destination.capabilities = template.capabilities;
      dialog.destination = destination;

      document.body.appendChild(dialog);
      Polymer.dom.flush();
    }

    /**
     * Verifies that the search box is shown or hidden based on the number
     * of capabilities and that the correct number of items are created.
     * @param {number} count The number of vendor capabilities the printer
     *     should have.
     */
    function verifyListWithItemCount(count) {
      // Search box should be hidden if there is only 1 item.
      const searchBox = dialog.$.searchBox;
      assertEquals(count == 1, searchBox.hidden);

      // Verify item is displayed.
      const items = dialog.shadowRoot.querySelectorAll(
          'print-preview-advanced-settings-item');
      assertEquals(count, items.length);
    }

    /**
     * Sets some non-default values for the advanced items. Assumes there are 3
     * capabilities.
     */
    function setItemValues() {
      const items = dialog.shadowRoot.querySelectorAll(
          'print-preview-advanced-settings-item');
      assertEquals(3, items.length);

      // Set some values.
      items[0].setCurrentValueForTest(6);
      items[1].setCurrentValueForTest(1);
      items[2].setCurrentValueForTest('XYZ');
    }

    // Tests that the search box does not appear when there is only one option,
    // and that the vendor item is correctly displayed.
    test(assert(TestNames.AdvancedSettings1Option), function() {
      setupDialog(1);
      verifyListWithItemCount(1);
    });

    // Tests that the search box appears when there are two options, and that
    // the items are correctly displayed.
    test(assert(TestNames.AdvancedSettings2Options), function() {
      setupDialog(2);
      verifyListWithItemCount(2);
    });

    // Tests that the advanced settings dialog correctly updates the settings
    // value for vendor items when the apply button is clicked.
    test(assert(TestNames.AdvancedSettingsApply), function() {
      setupDialog(3);
      setItemValues();

      const buttons = dialog.shadowRoot.querySelectorAll('paper-button');
      assertEquals(2, buttons.length);
      const whenDialogClose = test_util.eventToPromise('close', dialog);

      // Click apply button.
      buttons[1].click();
      return whenDialogClose.then(() => {
        // Check that the setting has been set.
        const setting = dialog.getSettingValue('vendorItems');
        assertEquals(6, setting.printArea);
        assertEquals(1, setting.paperType);
        assertEquals('XYZ', setting.watermark);
      });
    });

    // Tests that the advanced settings dialog does not update the settings
    // value for vendor items when the close button is clicked.
    test(assert(TestNames.AdvancedSettingsClose), function() {
      setupDialog(3);
      setItemValues();

      const buttons = dialog.shadowRoot.querySelectorAll('paper-button');
      assertEquals(2, buttons.length);
      const whenDialogClose = test_util.eventToPromise('close', dialog);

      // Click close button.
      buttons[0].click();
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
    test(assert(TestNames.AdvancedSettingsFilter), function() {
      setupDialog(3);
      const searchBox = dialog.$.searchBox;
      const items = dialog.shadowRoot.querySelectorAll(
          'print-preview-advanced-settings-item');
      const noMatchHint = dialog.$$('.no-settings-match-hint');

      // Query is initialized to null. All items are shown and the hint is
      // hidden.
      items.forEach(item => assertFalse(item.hidden));
      assertTrue(noMatchHint.hidden);

      // Searching for Watermark should show only the watermark setting.
      searchBox.searchQuery = /(Watermark)/i;
      items.forEach((item, index) => assertEquals(index != 2, item.hidden));
      assertTrue(noMatchHint.hidden);

      // Searching for A4 should show only the print area setting.
      searchBox.searchQuery = /(A4)/i;
      items.forEach((item, index) => assertEquals(index != 0, item.hidden));
      assertTrue(noMatchHint.hidden);

      // Searching for WXYZ should show no settings and display the "no match"
      // hint.
      searchBox.searchQuery = /(WXYZ)/i;
      items.forEach(item => assertTrue(item.hidden));
      assertFalse(noMatchHint.hidden);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
