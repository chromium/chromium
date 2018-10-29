// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sections_tests', function() {
  /** @enum {string} */
  const TestNames = {
    Copies: 'copies',
    Layout: 'layout',
    Color: 'color',
    ColorSaveToDrive: 'color save to drive',
    MediaSize: 'media size',
    MediaSizeCustomNames: 'media size custom names',
    Margins: 'margins',
    Dpi: 'dpi',
    Scaling: 'scaling',
    Other: 'other',
    HeaderFooter: 'header footer',
    SetPages: 'set pages',
    SetCopies: 'set copies',
    SetLayout: 'set layout',
    SetColor: 'set color',
    SetMediaSize: 'set media size',
    SetDpi: 'set dpi',
    SetMargins: 'set margins',
    SetPagesPerSheet: 'set pages per sheet',
    SetScaling: 'set scaling',
    SetOther: 'set other',
    PresetCopies: 'preset copies',
    PresetDuplex: 'preset duplex',
    DisableMarginsByPagesPerSheet: 'disable margins by pages per sheet',
    ColorManaged: 'color selection disabled by policy',
  };

  const suiteName = 'SettingsSectionsTests';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @override */
    setup(function() {
      const initialSettings =
          print_preview_test_utils.getDefaultInitialSettings();
      const nativeLayer = new print_preview.NativeLayerStub();
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      nativeLayer.setPageCount(3);
      print_preview.NativeLayer.setInstance(nativeLayer);
      const pluginProxy = new print_preview.PDFPluginStub();
      print_preview_new.PluginProxy.setInstance(pluginProxy);

      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;
      pluginProxy.setLoadCallback(previewArea.onPluginLoad_.bind(previewArea));

      // Wait for initialization to complete.
      return Promise
          .all([
            nativeLayer.whenCalled('getInitialSettings'),
            nativeLayer.whenCalled('getPrinterCapabilities')
          ])
          .then(function() {
            Polymer.dom.flush();
          });
    });

    /**
     * @param {boolean} isPdf Whether the document should be a PDF.
     * @param {boolean} hasSelection Whether the document has a selection.
     */
    function initDocumentInfo(isPdf, hasSelection) {
      const info = new print_preview.DocumentInfo();
      info.init(!isPdf, 'title', hasSelection);
      if (isPdf)
        info.updateFitToPageScaling(98);
      info.updatePageCount(3);
      page.set('documentInfo_', info);
      Polymer.dom.flush();
    }

    function addSelection() {
      // Add a selection.
      let info = new print_preview.DocumentInfo();
      info.init(page.documentInfo_.isModifiable, 'title', true);
      page.set('documentInfo_', info);
      Polymer.dom.flush();
    }

    function setPdfDestination() {
      const saveAsPdfDestination = new print_preview.Destination(
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL,
          loadTimeData.getString('printToPDF'), false /*isRecent*/,
          print_preview.DestinationConnectionStatus.ONLINE);
      saveAsPdfDestination.capabilities =
          print_preview_test_utils.getCddTemplate(saveAsPdfDestination.id)
              .capabilities;
      page.set('destination_', saveAsPdfDestination);
    }

    function toggleMoreSettings() {
      const moreSettingsElement = page.$$('print-preview-more-settings');
      moreSettingsElement.$.label.click();
    }

    test(assert(TestNames.Copies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      assertFalse(copiesElement.hidden);

      // Remove copies capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.copies;

      // Copies section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      assertTrue(copiesElement.hidden);
    });

    test(assert(TestNames.Layout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');

      // Set up with HTML document. No selection.
      initDocumentInfo(false, false);
      assertFalse(layoutElement.hidden);

      // Remove layout capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.page_orientation;

      // Each of these settings should not show the capability.
      [null, {option: [{type: 'PORTRAIT', is_default: true}]},
       {option: [{type: 'LANDSCAPE', is_default: true}]},
      ].forEach(layoutCap => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.page_orientation = layoutCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        assertTrue(layoutElement.hidden);
      });

      // Reset full capabilities
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      assertFalse(layoutElement.hidden);

      // Test with PDF - should be hidden.
      initDocumentInfo(true, false);
      assertTrue(layoutElement.hidden);
    });

    test(assert(TestNames.Color), function() {
      const colorElement = page.$$('print-preview-color-settings');
      assertFalse(colorElement.hidden);

      // Remove color capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.color;

      // Each of these settings should not show the capability. The value should
      // be the default for settings with multiple options and the only
      // available option otherwise.
      [{
        colorCap: null,
        expectedValue: false,
      },
       {
         colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
         expectedValue: true,
       },
       {
         colorCap: {
           option: [
             {type: 'STANDARD_COLOR', is_default: true},
             {type: 'CUSTOM_COLOR'}
           ]
         },
         expectedValue: true,
       },
       {
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME', is_default: true},
             {type: 'CUSTOM_MONOCHROME'}
           ]
         },
         expectedValue: false,
       },
       {
         colorCap: {option: [{type: 'STANDARD_MONOCHROME'}]},
         expectedValue: false,
       },
       {
         colorCap: {option: [{type: 'CUSTOM_MONOCHROME', vendor_id: '42'}]},
         expectedValue: false,
       },
       {
         colorCap: {option: [{type: 'CUSTOM_COLOR', vendor_id: '42'}]},
         expectedValue: true,
       }].forEach(capabilityAndValue => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.color = capabilityAndValue.colorCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        assertTrue(colorElement.hidden);
        assertEquals(
            capabilityAndValue.expectedValue, page.getSettingValue('color'));
      });

      // Each of these settings should show the capability with the default
      // value given by expectedValue.
      [{
        colorCap: {
          option: [
            {type: 'STANDARD_MONOCHROME', is_default: true},
            {type: 'STANDARD_COLOR'}
          ]
        },
        expectedValue: false,
      },
       {
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME'},
             {type: 'STANDARD_COLOR', is_default: true}
           ]
         },
         expectedValue: true,
       },
       {
         colorCap: {
           option: [
             {type: 'CUSTOM_MONOCHROME', vendor_id: '42'},
             {type: 'CUSTOM_COLOR', is_default: true, vendor_id: '43'}
           ]
         },
         expectedValue: true,
       }].forEach(capabilityAndValue => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.color = capabilityAndValue.colorCap;
        page.set('destination_.capabilities', capabilities);
        const selectElement = colorElement.$$('select');
        assertFalse(colorElement.hidden);
        assertEquals(
            capabilityAndValue.expectedValue ? 'color' : 'bw',
            selectElement.value);
        assertEquals(
            capabilityAndValue.expectedValue, page.getSettingValue('color'));
        // Check that setting is not marked as managed.
        assertFalse(colorElement.$$('print-preview-settings-section').managed);
        assertFalse(selectElement.disabled);
      });
    });

    test(assert(TestNames.ColorSaveToDrive), function() {
      // Check that the Save to Google Drive printer does not show the color
      // capability, but sets the value as true by default.
      const colorElement = page.$$('print-preview-color-settings');
      const googleDrivePrinter = new print_preview.Destination(
          print_preview.Destination.GooglePromotedId.DOCS,
          print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES,
          print_preview.Destination.GooglePromotedId.DOCS, true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE, {});
      page.set('destination_', googleDrivePrinter);
      const capabilities =
          print_preview_test_utils
              .getCddTemplate(print_preview.Destination.GooglePromotedId.DOCS)
              .capabilities;
      delete capabilities.printer.color;
      page.set('destination_.capabilities', capabilities);
      assertTrue(colorElement.hidden);
      assertEquals(true, page.getSettingValue('color'));
    });

    test(assert(TestNames.MediaSize), function() {
      const mediaSizeElement = page.$$('print-preview-media-size-settings');

      toggleMoreSettings();
      assertFalse(mediaSizeElement.hidden);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.media_size;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      assertTrue(mediaSizeElement.hidden);

      // Reset
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);

      // Set PDF document type.
      initDocumentInfo(true, false);
      assertFalse(mediaSizeElement.hidden);

      // Set save as PDF. This should hide the settings section.
      setPdfDestination();
      assertTrue(mediaSizeElement.hidden);

      // Set HTML document type, should now show the section.
      initDocumentInfo(false, false);
      assertFalse(mediaSizeElement.hidden);
    });

    test(assert(TestNames.MediaSizeCustomNames), function() {
      const customLocalizedMediaName = 'Vendor defined localized media name';
      const customMediaName = 'Vendor defined media name';
      const mediaSizeElement = page.$$('print-preview-media-size-settings');

      // Expand more settings to reveal the element.
      toggleMoreSettings();
      assertFalse(mediaSizeElement.hidden);

      // Change capability to have custom paper sizes.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.media_size =
          print_preview_test_utils.getMediaSizeCapabilityWithCustomNames();
      page.set('destination_.capabilities', capabilities);
      Polymer.dom.flush();

      const settingsSelect =
          mediaSizeElement.$$('print-preview-settings-select');

      assertEquals(capabilities.printer.media_size, settingsSelect.capability);
      assertFalse(settingsSelect.disabled);
      assertEquals('mediaSize', settingsSelect.settingName);
    });

    test(assert(TestNames.Margins), function() {
      const marginsElement = page.$$('print-preview-margins-settings');

      // Section is available for HTML (modifiable) documents
      initDocumentInfo(false, false);

      toggleMoreSettings();
      assertFalse(marginsElement.hidden);

      // Unavailable for PDFs.
      initDocumentInfo(true, false);
      assertTrue(marginsElement.hidden);
    });

    test(assert(TestNames.Dpi), function() {
      const dpiElement = page.$$('print-preview-dpi-settings');

      // Expand more settings to reveal the element.
      toggleMoreSettings();
      assertFalse(dpiElement.hidden);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.dpi;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      assertTrue(dpiElement.hidden);

      // Does not show up for only 1 option.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.dpi.option.pop();
      page.set('destination_.capabilities', capabilities);
      assertTrue(dpiElement.hidden);
    });

    test(assert(TestNames.Scaling), function() {
      const scalingElement = page.$$('print-preview-scaling-settings');

      toggleMoreSettings();
      assertFalse(scalingElement.hidden);

      // HTML to non-PDF destination -> only input shown
      initDocumentInfo(false, false);
      const fitToPageSection =
          scalingElement.$$('print-preview-settings-section');
      const scalingInputWrapper =
          scalingElement.$$('print-preview-number-settings-section')
              .$$('.input-wrapper');
      assertFalse(scalingElement.hidden);
      assertTrue(fitToPageSection.hidden);
      assertFalse(scalingInputWrapper.hidden);

      // PDF to non-PDF destination -> checkbox and input shown. Check that if
      // more settings is collapsed the section is hidden.
      initDocumentInfo(true, false);
      assertFalse(scalingElement.hidden);
      assertFalse(fitToPageSection.hidden);
      assertFalse(scalingInputWrapper.hidden);

      // PDF to PDF destination -> section disappears.
      setPdfDestination();
      assertTrue(scalingElement.hidden);
    });

    /**
     * @param {!CrCheckboxElement} checkbox The checkbox to check
     * @return {boolean} Whether the checkbox's parent section is hidden.
     */
    function isSectionHidden(checkbox) {
      return checkbox.parentNode.parentNode.hidden;
    }

    test(assert(TestNames.Other), function() {
      const optionsElement = page.$$('print-preview-other-options-settings');
      const headerFooter = optionsElement.$$('#headerFooter');
      const duplex = optionsElement.$$('#duplex');
      const cssBackground = optionsElement.$$('#cssBackground');
      const rasterize = optionsElement.$$('#rasterize');
      const selectionOnly = optionsElement.$$('#selectionOnly');

      // Start with HTML + duplex capability.
      initDocumentInfo(false, false);
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);

      // Expanding more settings will show the section.
      toggleMoreSettings();
      assertFalse(isSectionHidden(headerFooter));
      assertFalse(isSectionHidden(duplex));
      assertFalse(isSectionHidden(cssBackground));
      assertTrue(isSectionHidden(rasterize));
      assertTrue(isSectionHidden(selectionOnly));

      // Add a selection - should show selection only.
      initDocumentInfo(false, true);
      assertFalse(optionsElement.hidden);
      assertFalse(isSectionHidden(selectionOnly));

      // Remove duplex capability.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.duplex;
      page.set('destination_.capabilities', capabilities);
      Polymer.dom.flush();
      assertFalse(optionsElement.hidden);
      assertTrue(isSectionHidden(duplex));

      // Set a duplex capability with only 1 type, no duplex.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.duplex;
      capabilities.printer.duplex = {
        option:
            [{type: print_preview_new.DuplexType.NO_DUPLEX, is_default: true}]
      };
      page.set('destination_.capabilities', capabilities);
      Polymer.dom.flush();
      assertFalse(optionsElement.hidden);
      assertTrue(isSectionHidden(duplex));

      // PDF
      initDocumentInfo(true, false);
      Polymer.dom.flush();
      if (cr.isWindows || cr.isMac) {
        // No options
        assertTrue(optionsElement.hidden);
      } else {
        // All sections hidden except rasterize
        assertTrue(isSectionHidden(headerFooter));
        assertTrue(isSectionHidden(duplex));
        assertTrue(isSectionHidden(cssBackground));
        assertEquals(cr.isWindows || cr.isMac, isSectionHidden(rasterize));
        assertTrue(isSectionHidden(selectionOnly));
      }

      // Add a selection - should do nothing for PDFs.
      initDocumentInfo(true, true);
      assertEquals(cr.isWindows || cr.isMac, optionsElement.hidden);
      assertTrue(isSectionHidden(selectionOnly));

      // Add duplex.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      Polymer.dom.flush();
      assertFalse(optionsElement.hidden);
      assertFalse(isSectionHidden(duplex));
    });

    test(assert(TestNames.HeaderFooter), function() {
      const optionsElement = page.$$('print-preview-other-options-settings');
      const headerFooter = optionsElement.$$('#headerFooter');

      // HTML page to show Header/Footer option.
      initDocumentInfo(false, false);
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);

      toggleMoreSettings();
      assertFalse(optionsElement.hidden);
      assertFalse(isSectionHidden(headerFooter));

      // Set margins to NONE
      page.set(
          'settings.margins.value',
          print_preview.ticket_items.MarginsTypeValue.NO_MARGINS);
      assertTrue(isSectionHidden(headerFooter));

      // Custom margins of 0.
      page.set(
          'settings.margins.value',
          print_preview.ticket_items.MarginsTypeValue.CUSTOM);
      page.set(
          'settings.customMargins.vaue',
          {marginTop: 0, marginLeft: 0, marginRight: 0, marginBottom: 0});
      assertTrue(isSectionHidden(headerFooter));

      // Custom margins of 36 -> header/footer available
      page.set(
          'settings.customMargins.value',
          {marginTop: 36, marginLeft: 36, marginRight: 36, marginBottom: 36});
      assertFalse(isSectionHidden(headerFooter));

      // Zero top and bottom -> header/footer unavailable
      page.set(
          'settings.customMargins.value',
          {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 0});
      assertTrue(isSectionHidden(headerFooter));

      // Zero top and nonzero bottom -> header/footer available
      page.set(
          'settings.customMargins.value',
          {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 36});
      assertFalse(isSectionHidden(headerFooter));

      // Small paper sizes
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.media_size = {
        'option': [
          {
            'name': 'SmallLabel',
            'width_microns': 38100,
            'height_microns': 12700,
            'is_default': false
          },
          {
            'name': 'BigLabel',
            'width_microns': 50800,
            'height_microns': 76200,
            'is_default': true
          }
        ]
      };
      page.set('destination_.capabilities', capabilities);
      page.set(
          'settings.margins.value',
          print_preview.ticket_items.MarginsTypeValue.DEFAULT);

      // Header/footer should be available for default big label
      assertFalse(isSectionHidden(headerFooter));

      page.set(
          'settings.mediaSize.value',
          capabilities.printer.media_size.option[0]);

      // Header/footer should not be available for small label
      assertTrue(isSectionHidden(headerFooter));
    });

    test(assert(TestNames.SetPages), function() {
      const pagesElement = page.$$('print-preview-pages-settings');
      // This section is always visible.
      assertFalse(pagesElement.hidden);

      // Default value is all pages. Print ticket expects this to be empty.
      const allRadio = pagesElement.$.allRadioButton;
      const customRadio = pagesElement.$.customRadioButton;
      const pagesCrInput = pagesElement.$.pageSettingsCustomInput;
      const pagesInput = pagesCrInput.inputElement;

      /**
       * @param {boolean} allChecked Whether the all pages radio button is
       *     selected.
       * @param {string} inputString The expected string in the pages input.
       * @param {boolean} valid Whether the input string is valid.
       */
      const validateInputState = function(allChecked, inputString, valid) {
        assertEquals(allChecked, allRadio.checked);
        assertEquals(!allChecked, customRadio.checked);
        assertEquals(inputString, pagesInput.value);
        assertEquals(valid, !pagesCrInput.invalid);
      };
      validateInputState(true, '', true);
      assertEquals(0, page.settings.ranges.value.length);
      assertEquals(3, page.settings.pages.value.length);
      assertTrue(page.settings.pages.valid);

      // Set selection of pages 1 and 2.
      customRadio.click();

      // Manually set |optionSelected_| since focus may not work correctly on
      // MacOS. The PageSettingsTests verify this behavior is correct on all
      // platforms.
      pagesElement.set('optionSelected_', pagesElement.pagesValueEnum_.CUSTOM);

      print_preview_test_utils.triggerInputEvent(pagesInput, '1-2');
      return test_util.eventToPromise('input-change', pagesElement)
          .then(function() {
            validateInputState(false, '1-2', true);
            assertEquals(1, page.settings.ranges.value.length);
            assertEquals(1, page.settings.ranges.value[0].from);
            assertEquals(2, page.settings.ranges.value[0].to);
            assertEquals(2, page.settings.pages.value.length);
            assertTrue(page.settings.pages.valid);

            // Select pages 1 and 3
            print_preview_test_utils.triggerInputEvent(pagesInput, '1, 3');
            return test_util.eventToPromise('input-change', pagesElement);
          })
          .then(function() {
            validateInputState(false, '1, 3', true);
            assertEquals(2, page.settings.ranges.value.length);
            assertEquals(1, page.settings.ranges.value[0].from);
            assertEquals(1, page.settings.ranges.value[0].to);
            assertEquals(3, page.settings.ranges.value[1].from);
            assertEquals(3, page.settings.ranges.value[1].to);
            assertEquals(2, page.settings.pages.value.length);
            assertTrue(page.settings.pages.valid);

            // Enter an out of bounds value.
            print_preview_test_utils.triggerInputEvent(pagesInput, '5');
            return test_util.eventToPromise('input-change', pagesElement);
          })
          .then(function() {
            // Now the pages settings value should be invalid, and the error
            // message should be displayed.
            validateInputState(false, '5', false);
            assertFalse(page.settings.pages.valid);
          });
    });

    test(assert(TestNames.SetCopies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      assertFalse(copiesElement.hidden);

      // Default value is 1
      const copiesInput =
          copiesElement.$$('print-preview-number-settings-section')
              .$.userValue.inputElement;
      assertEquals('1', copiesInput.value);
      assertEquals(1, page.settings.copies.value);

      // Change to 2
      print_preview_test_utils.triggerInputEvent(copiesInput, '2');
      return test_util.eventToPromise('input-change', copiesElement)
          .then(function() {
            assertEquals(2, page.settings.copies.value);

            // Collate is true by default.
            const collateInput = copiesElement.$.collate;
            assertTrue(collateInput.checked);
            assertTrue(page.settings.collate.value);

            // Uncheck the box.
            MockInteractions.tap(collateInput);
            assertFalse(collateInput.checked);
            collateInput.dispatchEvent(new CustomEvent('change'));
            assertFalse(page.settings.collate.value);

            // Set an empty value.
            print_preview_test_utils.triggerInputEvent(copiesInput, '');
            return test_util.eventToPromise('input-change', copiesElement);
          })
          .then(function() {
            // Collate should be hidden now, but no update to the backing value
            // occurs.
            assertTrue(copiesElement.$$('.checkbox').hidden);
            assertTrue(page.settings.copies.valid);
            assertEquals(2, page.settings.copies.value);

            // If the field is blurred, it will be reset to the default by the
            // number-settings-section. Simulate this ocurring.
            const numberSettingsSection =
                copiesElement.$$('print-preview-number-settings-section');
            numberSettingsSection.$.userValue.value = '1';
            numberSettingsSection.currentValue = '1';
            assertTrue(page.settings.copies.valid);
            assertEquals(1, page.settings.copies.value);

            // Enter an invalid value.
            print_preview_test_utils.triggerInputEvent(copiesInput, '0');
            return test_util.eventToPromise('input-change', copiesElement);
          })
          .then(function() {
            // Collate should be hidden. Value is not updated to the invalid
            // number. Setting is marked invalid.
            assertTrue(copiesElement.$$('.checkbox').hidden);
            assertFalse(page.settings.copies.valid);
            assertEquals(1, page.settings.copies.value);
          });
    });

    test(assert(TestNames.SetLayout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');
      assertFalse(layoutElement.hidden);

      // Default is portrait
      const layoutInput = layoutElement.$$('select');
      assertEquals('portrait', layoutInput.value);
      assertFalse(page.settings.layout.value);

      // Change to landscape
      layoutInput.value = 'landscape';
      layoutInput.dispatchEvent(new CustomEvent('change'));
      return test_util.eventToPromise('process-select-change', layoutElement)
          .then(function() {
            assertTrue(page.settings.layout.value);
          });
    });

    test(assert(TestNames.SetColor), function() {
      const colorElement = page.$$('print-preview-color-settings');
      assertFalse(colorElement.hidden);

      // Default is color
      const colorInput = colorElement.$$('select');
      assertEquals('color', colorInput.value);
      assertTrue(page.settings.color.value);

      // Change to black and white.
      colorInput.value = 'bw';
      colorInput.dispatchEvent(new CustomEvent('change'));
      return test_util.eventToPromise('process-select-change', colorElement)
          .then(function() {
            assertFalse(page.settings.color.value);
          });
    });

    test(assert(TestNames.SetMediaSize), function() {
      toggleMoreSettings();
      const mediaSizeElement = page.$$('print-preview-media-size-settings');
      assertFalse(mediaSizeElement.hidden);

      const mediaSizeCapabilities =
          page.destination_.capabilities.printer.media_size;
      const letterOption = JSON.stringify(mediaSizeCapabilities.option[0]);
      const squareOption = JSON.stringify(mediaSizeCapabilities.option[1]);

      // Default is letter
      const mediaSizeInput =
          mediaSizeElement.$$('print-preview-settings-select').$$('select');
      assertEquals(letterOption, mediaSizeInput.value);
      assertEquals(letterOption, JSON.stringify(page.settings.mediaSize.value));

      // Change to square
      mediaSizeInput.value = squareOption;
      mediaSizeInput.dispatchEvent(new CustomEvent('change'));

      return test_util.eventToPromise('process-select-change', mediaSizeElement)
          .then(function() {
            assertEquals(
                squareOption, JSON.stringify(page.settings.mediaSize.value));
          });
    });

    test(assert(TestNames.SetDpi), function() {
      toggleMoreSettings();
      const dpiElement = page.$$('print-preview-dpi-settings');
      assertFalse(dpiElement.hidden);

      const dpiCapabilities = page.destination_.capabilities.printer.dpi;
      const highQualityOption = dpiCapabilities.option[0];
      const lowQualityOption = dpiCapabilities.option[1];

      // Default is 200
      const dpiInput =
          dpiElement.$$('print-preview-settings-select').$$('select');
      const isDpiEqual = function(value1, value2) {
        return value1.horizontal_dpi == value2.horizontal_dpi &&
            value1.vertical_dpi == value2.vertical_dpi &&
            value1.vendor_id == value2.vendor_id;
      };
      expectTrue(isDpiEqual(highQualityOption, JSON.parse(dpiInput.value)));
      expectTrue(isDpiEqual(highQualityOption, page.settings.dpi.value));

      // Change to 100
      dpiInput.value =
          JSON.stringify(dpiElement.capabilityWithLabels_.option[1]);
      dpiInput.dispatchEvent(new CustomEvent('change'));

      return test_util.eventToPromise('process-select-change', dpiElement)
          .then(function() {
            expectTrue(isDpiEqual(
                lowQualityOption, JSON.parse(dpiInput.value)));
            expectTrue(isDpiEqual(lowQualityOption, page.settings.dpi.value));
          });
    });

    test(assert(TestNames.SetMargins), function() {
      toggleMoreSettings();
      const marginsElement = page.$$('print-preview-margins-settings');
      assertFalse(marginsElement.hidden);

      // Default is DEFAULT_MARGINS
      const marginsInput = marginsElement.$$('select');
      assertEquals(
          print_preview.ticket_items.MarginsTypeValue.DEFAULT.toString(),
          marginsInput.value);
      assertEquals(
          print_preview.ticket_items.MarginsTypeValue.DEFAULT,
          page.settings.margins.value);

      // Change to minimum.
      marginsInput.value =
          print_preview.ticket_items.MarginsTypeValue.MINIMUM.toString();
      marginsInput.dispatchEvent(new CustomEvent('change'));
      return test_util.eventToPromise('process-select-change', marginsElement)
          .then(function() {
            assertEquals(
                print_preview.ticket_items.MarginsTypeValue.MINIMUM,
                page.settings.margins.value);
          });
    });

    test(assert(TestNames.SetPagesPerSheet), function() {
      toggleMoreSettings();
      const pagesPerSheetElement =
          page.$$('print-preview-pages-per-sheet-settings');
      assertFalse(pagesPerSheetElement.hidden);

      const pagesPerSheetInput = pagesPerSheetElement.$$('select');
      assertEquals(1, page.settings.pagesPerSheet.value);

      // Change to a different value.
      pagesPerSheetInput.value = 2;
      pagesPerSheetInput.dispatchEvent(new CustomEvent('change'));
      return test_util
          .eventToPromise('process-select-change', pagesPerSheetElement)
          .then(function() {
            assertEquals(2, page.settings.pagesPerSheet.value);
          });
    });

    // This test verifies that changing pages per sheet to N > 1 resets the
    // margins dropdown value to DEFAULT and disables it, and resetting
    // pages per sheet back to 1 re-enables the dropdown.
    test(assert(TestNames.DisableMarginsByPagesPerSheet), function() {
      toggleMoreSettings();
      const pagesPerSheetElement =
          page.$$('print-preview-pages-per-sheet-settings');
      assertFalse(pagesPerSheetElement.hidden);

      const pagesPerSheetInput = pagesPerSheetElement.$$('select');
      assertEquals(1, page.settings.pagesPerSheet.value);

      const marginsElement = page.$$('print-preview-margins-settings');
      assertFalse(marginsElement.hidden);

      const marginsTypeEnum = print_preview.ticket_items.MarginsTypeValue;

      // Default is DEFAULT_MARGINS
      const marginsInput = marginsElement.$$('select');
      assertEquals(marginsTypeEnum.DEFAULT, page.settings.margins.value);

      // Change margins to minimum.
      marginsInput.value = marginsTypeEnum.MINIMUM.toString();
      marginsInput.dispatchEvent(new CustomEvent('change'));
      return test_util.eventToPromise('process-select-change', marginsElement)
          .then(function() {
            assertEquals(marginsTypeEnum.MINIMUM, page.settings.margins.value);
            assertFalse(marginsInput.disabled);
            // Change pages per sheet to a different value.
            pagesPerSheetInput.value = 2;
            pagesPerSheetInput.dispatchEvent(new CustomEvent('change'));
            return test_util.eventToPromise(
                'process-select-change', pagesPerSheetElement);
          })
          .then(function() {
            assertEquals(2, page.settings.pagesPerSheet.value);
            assertEquals(marginsTypeEnum.DEFAULT, page.settings.margins.value);
            assertEquals(
                marginsTypeEnum.DEFAULT.toString(), marginsInput.value);
            assertTrue(marginsInput.disabled);

            // Set pages per sheet back to 1.
            pagesPerSheetInput.value = 1;
            pagesPerSheetInput.dispatchEvent(new CustomEvent('change'));
            return test_util.eventToPromise(
                'process-select-change', pagesPerSheetElement);
          })
          .then(function() {
            assertEquals(1, page.settings.pagesPerSheet.value);
            assertEquals(marginsTypeEnum.DEFAULT, page.settings.margins.value);
            assertEquals(
                marginsTypeEnum.DEFAULT.toString(), marginsInput.value);
            assertFalse(marginsInput.disabled);
          });
    });

    test(assert(TestNames.SetScaling), function() {
      toggleMoreSettings();
      const scalingElement = page.$$('print-preview-scaling-settings');

      // Default is 100
      const scalingInput =
          scalingElement.$$('print-preview-number-settings-section')
              .$.userValue.inputElement;
      const fitToPageCheckbox = scalingElement.$$('#fit-to-page-checkbox');

      const validateScalingState =
          (scalingValue, scalingValid, fitToPage, fitToPageDisplay) => {
            // Invalid scalings are always set directly in the input, so no need
            // to verify that the input matches them.
            if (scalingValid) {
              const scalingDisplay = fitToPage ?
                  page.documentInfo_.fitToPageScaling.toString() :
                  scalingValue;
              assertEquals(scalingDisplay, scalingInput.value);
            }
            assertEquals(scalingValue, page.settings.scaling.value);
            assertEquals(scalingValid, page.settings.scaling.valid);
            assertEquals(fitToPageDisplay, fitToPageCheckbox.checked);
            assertEquals(fitToPage, page.settings.fitToPage.value);
          };

      // Set PDF so both scaling and fit to page are active.
      initDocumentInfo(true, false);
      assertFalse(scalingElement.hidden);

      // Default is 100
      validateScalingState('100', true, false, false);

      // Change to 105
      print_preview_test_utils.triggerInputEvent(scalingInput, '105');
      return test_util.eventToPromise('input-change', scalingElement)
          .then(function() {
            validateScalingState('105', true, false, false);

            // Change to fit to page. Should display fit to page scaling but not
            // alter the scaling setting.
            fitToPageCheckbox.checked = true;
            fitToPageCheckbox.dispatchEvent(new CustomEvent('change'));
            return test_util.eventToPromise(
                'update-checkbox-setting', scalingElement);
          })
          .then(function(event) {
            assertEquals('fitToPage', event.detail);
            validateScalingState('105', true, true, true);

            // Set scaling. Should uncheck fit to page and set the settings for
            // scaling and fit to page.
            print_preview_test_utils.triggerInputEvent(scalingInput, '95');
            return test_util.eventToPromise('input-change', scalingElement);
          })
          .then(function() {
            validateScalingState('95', true, false, false);

            // Set scaling to something invalid. Should change setting validity
            // but not value.
            print_preview_test_utils.triggerInputEvent(scalingInput, '5');
            return test_util.eventToPromise('input-change', scalingElement);
          })
          .then(function() {
            validateScalingState('95', false, false, false);

            // Check fit to page. Should set scaling valid.
            fitToPageCheckbox.checked = true;
            fitToPageCheckbox.dispatchEvent(new CustomEvent('change'));
            return test_util.eventToPromise(
                'update-checkbox-setting', scalingElement);
          })
          .then(function(event) {
            assertEquals('fitToPage', event.detail);
            validateScalingState('95', true, true, true);

            // Uncheck fit to page. Should reset scaling to last valid.
            fitToPageCheckbox.checked = false;
            fitToPageCheckbox.dispatchEvent(new CustomEvent('change'));
            return test_util.eventToPromise(
                'update-checkbox-setting', scalingElement);
          })
          .then(function(event) {
            assertEquals('fitToPage', event.detail);
            validateScalingState('95', true, false, false);

            // Change to fit to page. Should display fit to page scaling but not
            // alter the scaling setting.
            fitToPageCheckbox.checked = true;
            fitToPageCheckbox.dispatchEvent(new CustomEvent('change'));
            return test_util.eventToPromise(
                'update-checkbox-setting', scalingElement);
          })
          .then(function(event) {
            assertEquals('fitToPage', event.detail);
            validateScalingState('95', true, true, true);

            // Enter something invalid in the scaling field. This should not
            // change the stored value of scaling or fit to page, to avoid an
            // unnecessary preview regeneration, but should display fit to page
            // as unchecked.
            print_preview_test_utils.triggerInputEvent(scalingInput, '9');
            return test_util.eventToPromise('input-change', scalingElement);
          })
          .then(function() {
            validateScalingState('95', false, true, false);

            // Enter a blank value in the scaling field. This should not
            // change the stored value of scaling or fit to page, to avoid an
            // unnecessary preview regeneration.
            print_preview_test_utils.triggerInputEvent(scalingInput, '');
            return test_util.eventToPromise('input-change', scalingElement);
          })
          .then(function() {
            validateScalingState('95', false, true, false);

            // Entering something valid unsets fit to page and sets scaling
            // valid to true.
            print_preview_test_utils.triggerInputEvent(scalingInput, '90');
            return test_util.eventToPromise('input-change', scalingElement);
          })
          .then(function() {
            validateScalingState('90', true, false, false);
          });
    });

    test(assert(TestNames.SetOther), function() {
      toggleMoreSettings();
      const optionsElement = page.$$('print-preview-other-options-settings');
      assertFalse(optionsElement.hidden);

      // HTML - Header/footer, duplex, and CSS background. Also add selection.
      initDocumentInfo(false, true);

      const testOptionCheckbox = (settingName, defaultValue) => {
        const element = optionsElement.$$('#' + settingName);
        const optionSetting = page.settings[settingName];
        assertFalse(isSectionHidden(element));
        assertEquals(defaultValue, element.checked);
        assertEquals(defaultValue, optionSetting.value);
        element.checked = !defaultValue;
        element.dispatchEvent(new CustomEvent('change'));
        return test_util
            .eventToPromise('update-checkbox-setting', optionsElement)
            .then(function(event) {
              assertEquals(element.id, event.detail);
              assertEquals(!defaultValue, optionSetting.value);
            });
      };

      return testOptionCheckbox('headerFooter', true)
          .then(function() {
            // Duplex defaults to false, since the printer sets no duplex as the
            // default in the CDD (see print_preview_test_utils.js).
            return testOptionCheckbox('duplex', false);
          })
          .then(function() {
            return testOptionCheckbox('cssBackground', false);
          })
          .then(function() {
            return testOptionCheckbox('selectionOnly', false);
          })
          .then(function() {
            // Set PDF to test rasterize
            if (!cr.isWindows && !cr.isMac) {
              initDocumentInfo(true, false);
              return testOptionCheckbox('rasterize', false);
            }
          });
    });

    test(assert(TestNames.PresetCopies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      assertFalse(copiesElement.hidden);

      // Default value is 1
      const copiesInput =
          copiesElement.$$('print-preview-number-settings-section')
              .$.userValue.inputElement;
      assertEquals('1', copiesInput.value);
      assertEquals(1, page.settings.copies.value);

      // Send a preset value of 2
      const copies = 2;
      cr.webUIListenerCallback('print-preset-options', true, copies);
      assertEquals(copies, page.settings.copies.value);
      assertEquals(copies.toString(), copiesInput.value);
    });

    test(assert(TestNames.PresetDuplex), function() {
      toggleMoreSettings();
      const optionsElement = page.$$('print-preview-other-options-settings');
      assertFalse(optionsElement.hidden);

      // Default value is on, so turn it off
      page.setSetting('duplex', false);
      const checkbox = optionsElement.$$('#duplex');
      assertFalse(checkbox.checked);
      assertFalse(page.settings.duplex.value);

      // Send a preset value of LONG_EDGE
      const duplex = print_preview_new.DuplexMode.LONG_EDGE;
      cr.webUIListenerCallback('print-preset-options', false, 1, duplex);
      assertTrue(page.settings.duplex.value);
      assertTrue(checkbox.checked);
    });

    test(assert(TestNames.ColorManaged), function() {
      const colorElement = page.$$('print-preview-color-settings');
      assertFalse(colorElement.hidden);

      // Remove color capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.color;

      [{
        // Policy has no effect.
        colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
        colorPolicy: print_preview.ColorMode.COLOR,
        expectedValue: true,
        expectedHidden: true,
      },
       {
         // Policy contradicts actual capabilities and should be ignored.
         colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
         colorPolicy: print_preview.ColorMode.GRAY,
         expectedValue: true,
         expectedHidden: true,
       },
       {
         // Policy overrides default.
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME', is_default: true},
             {type: 'STANDARD_COLOR'}
           ]
         },
         colorPolicy: print_preview.ColorMode.COLOR,
         expectedValue: true,
         expectedHidden: false,
         expectedManaged: true,
       }].forEach(subtestParams => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.color = subtestParams.colorCap;
        const policies = {allowedColorModes: subtestParams.colorPolicy};
        // In practice |capabilities| are always set after |policies| and
        // observers only check for |capabilities|, so the order is important.
        page.set('destination_.policies', policies);
        page.set('destination_.capabilities', capabilities);
        assertEquals(
            subtestParams.expectedValue, page.getSettingValue('color'));
        assertEquals(subtestParams.expectedHidden, colorElement.hidden);
        if (!subtestParams.expectedHidden) {
          const selectElement = colorElement.$$('select');
          assertEquals(
              subtestParams.expectedValue ? 'color' : 'bw',
              selectElement.value);
          assertEquals(
              subtestParams.expectedManaged,
              colorElement.$$('print-preview-settings-section').managed);
          assertEquals(subtestParams.expectedManaged, selectElement.disabled);
        }
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
