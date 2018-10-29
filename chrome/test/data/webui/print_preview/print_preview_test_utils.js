// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview_test_utils', function() {
  /** @return {!print_preview.NativeInitialSettings} */
  function getDefaultInitialSettings() {
    return {
      isInKioskAutoPrintMode: false,
      isInAppKioskMode: false,
      thousandsDelimeter: ',',
      decimalDelimeter: '.',
      unitType: 1,
      previewModifiable: true,
      documentTitle: 'title',
      documentHasSelection: true,
      shouldPrintSelectionOnly: false,
      isHeaderFooterManaged: false,
      printerName: 'FooDevice',
      serializedAppStateStr: null,
      serializedDefaultDestinationSelectionRulesStr: null
    };
  }

  /**
   * @param {string} printerId
   * @param {string=} opt_printerName Defaults to an empty string.
   * @return {!print_preview.PrinterCapabilitiesResponse}
   */
  function getCddTemplate(printerId, opt_printerName) {
    return {
      printer: {
        deviceName: printerId,
        printerName: opt_printerName || '',
      },
      capabilities: {
        version: '1.0',
        printer: {
          supported_content_type: [{content_type: 'application/pdf'}],
          collate: {default: true},
          copies: {default: 1, max: 1000},
          color: {
            option: [
              {type: 'STANDARD_COLOR', is_default: true},
              {type: 'STANDARD_MONOCHROME'}
            ]
          },
          dpi: {
            option: [
              {horizontal_dpi: 200, vertical_dpi: 200, is_default: true},
              {horizontal_dpi: 100, vertical_dpi: 100},
            ]
          },
          duplex: {
            option: [
              {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
              {type: 'SHORT_EDGE'}
            ]
          },
          page_orientation: {
            option: [
              {type: 'PORTRAIT', is_default: true}, {type: 'LANDSCAPE'},
              {type: 'AUTO'}
            ]
          },
          media_size: {
            option: [
              {
                name: 'NA_LETTER',
                width_microns: 215900,
                height_microns: 279400,
                is_default: true,
                custom_display_name: 'Letter',
              },
              {
                name: 'CUSTOM_SQUARE',
                width_microns: 215900,
                height_microns: 215900,
                custom_display_name: 'CUSTOM_SQUARE',
              }
            ]
          }
        }
      }
    };
  }

  /**
   * Gets a CDD template and adds some dummy vendor capabilities. For select
   * capabilities, the values of these options are arbitrary. These values are
   * provided and read by the destination, so there are no fixed options like
   * there are for margins or color.
   * @param {number} numSettings
   * @param {string} printerId
   * @param {string=} opt_printerName Defaults to an empty string.
   * @return {!print_preview.PrinterCapabilitiesResponse}
   */
  function getCddTemplateWithAdvancedSettings(
      numSettings, printerId, opt_printerName) {
    const template =
        print_preview_test_utils.getCddTemplate(printerId, opt_printerName);
    if (numSettings < 1)
      return template;

    template.capabilities.printer.vendor_capability = [{
      display_name: 'Print Area',
      id: 'printArea',
      type: 'SELECT',
      select_cap: {
        option: [
          {display_name: 'A4', value: 4, is_default: true},
          {display_name: 'A6', value: 6},
          {display_name: 'A7', value: 7},
        ],
      },
    }];

    if (numSettings < 2)
      return template;

    // Add new capability.
    template.capabilities.printer.vendor_capability.push({
      display_name: 'Paper Type',
      id: 'paperType',
      type: 'SELECT',
      select_cap: {
        option: [
          {display_name: 'Standard', value: 0, is_default: true},
          {display_name: 'Recycled', value: 1},
          {display_name: 'Special', value: 2}
        ]
      }
    });

    if (numSettings < 3)
      return template;

    template.capabilities.printer.vendor_capability.push({
      display_name: 'Watermark',
      id: 'watermark',
      type: 'TYPED_VALUE',
      typed_value_cap: {
        default: '',
      }
    });

    return template;
  }

  /**
   * Creates a destination with a certificate status tag.
   * @param {string} id Printer id
   * @param {string} name Printer display name
   * @param {boolean} invalid Whether printer has an invalid certificate.
   * @return {!print_preview.Destination}
   */
  function createDestinationWithCertificateStatus(id, name, invalid) {
    const tags = {
      certificateStatus: invalid ?
          print_preview.DestinationCertificateStatus.NO :
          print_preview.DestinationCertificateStatus.UNKNOWN,
    };
    const dest = new print_preview.Destination(
        id, print_preview.DestinationType.GOOGLE,
        print_preview.DestinationOrigin.COOKIES, name, true /* isRecent */,
        print_preview.DestinationConnectionStatus.ONLINE, tags);
    return dest;
  }

  /**
   * @return {!print_preview.PrinterCapabilitiesResponse} The capabilities of
   *     the Save as PDF destination.
   */
  function getPdfPrinter() {
    return {
      printer: {
        deviceName: 'Save as PDF',
      },
      capabilities: {
        version: '1.0',
        printer: {
          page_orientation: {
            option: [
              {type: 'AUTO', is_default: true}, {type: 'PORTRAIT'},
              {type: 'LANDSCAPE'}
            ]
          },
          color: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
          media_size: {
            option: [{
              name: 'NA_LETTER',
              width_microns: 0,
              height_microns: 0,
              is_default: true
            }]
          }
        }
      }
    };
  }

  /**
   * Get the default media size for |device|.
   * @param {!print_preview.PrinterCapabilitiesResponse} device
   * @return {{width_microns: number,
   *           height_microns: number}} The width and height of the default
   *     media.
   */
  function getDefaultMediaSize(device) {
    const size = device.capabilities.printer.media_size.option.find(
        opt => opt.is_default);
    return {
      width_microns: size.width_microns,
      height_microns: size.height_microns
    };
  }

  /**
   * Get the default page orientation for |device|.
   * @param {!print_preview.PrinterCapabilitiesResponse} device
   * @return {string} The default orientation.
   */
  function getDefaultOrientation(device) {
    return device.capabilities.printer.page_orientation.option
        .find(opt => opt.is_default)
        .type;
  }

  /**
   * Creates 5 local destinations, adds them to |localDestinations| and
   * sets the capabilities in |nativeLayer|.
   * @param {!print_preview.NativeLayerStub} nativeLayer
   * @param {!Array<!print_preview.LocalDestinationInfo>} localDestinations
   * @return {!Array<!print_preview.Destination>}
   */
  function getDestinations(nativeLayer, localDestinations) {
    let destinations = [];
    const origin = cr.isChromeOS ? print_preview.DestinationOrigin.CROS :
                                   print_preview.DestinationOrigin.LOCAL;
    // Five destinations. FooDevice is the system default.
    [{id: 'ID1', name: 'One'}, {id: 'ID2', name: 'Two'},
     {id: 'ID3', name: 'Three'}, {id: 'ID4', name: 'Four'},
     {id: 'FooDevice', name: 'FooName'}]
        .forEach((info, index) => {
          const destination = new print_preview.Destination(
              info.id, print_preview.DestinationType.LOCAL, origin, info.name,
              false, print_preview.DestinationConnectionStatus.ONLINE);
          nativeLayer.setLocalDestinationCapabilities(
              print_preview_test_utils.getCddTemplate(info.id, info.name));
          localDestinations.push({printerName: info.name, deviceName: info.id});
          destinations.push(destination);
        });
    return destinations;
  }

  /**
   * Returns a media size capability with custom and localized names.
   * @return {!{ option: Array<!print_preview_new.SelectOption> }}
   */
  function getMediaSizeCapabilityWithCustomNames() {
    const customLocalizedMediaName = 'Vendor defined localized media name';
    const customMediaName = 'Vendor defined media name';

    return {
      option: [
        {
          name: 'CUSTOM',
          width_microns: 15900,
          height_microns: 79400,
          is_default: true,
          custom_display_name_localized:
              [{locale: navigator.language, value: customLocalizedMediaName}]
        },
        {
          name: 'CUSTOM',
          width_microns: 15900,
          height_microns: 79400,
          custom_display_name: customMediaName
        }
      ]
    };
  }

  /**
   * @param {!HTMLInputElement} element
   * @param {!string} input The value to set for the input element.
   */
  function triggerInputEvent(element, input) {
    element.value = input;
    element.dispatchEvent(
        new CustomEvent('input', {composed: true, bubbles: true}));
  }

  return {
    getDefaultInitialSettings: getDefaultInitialSettings,
    getCddTemplate: getCddTemplate,
    getCddTemplateWithAdvancedSettings: getCddTemplateWithAdvancedSettings,
    getDefaultMediaSize: getDefaultMediaSize,
    getDefaultOrientation: getDefaultOrientation,
    createDestinationWithCertificateStatus:
        createDestinationWithCertificateStatus,
    getDestinations: getDestinations,
    getMediaSizeCapabilityWithCustomNames:
        getMediaSizeCapabilityWithCustomNames,
    getPdfPrinter: getPdfPrinter,
    triggerInputEvent: triggerInputEvent,
  };
});
