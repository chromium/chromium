// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CapabilitiesResponse, Cdd, DEFAULT_MAX_COPIES, Destination, DestinationCertificateStatus, DestinationConnectionStatus, DestinationOrigin, DestinationStore, DestinationType, LocalDestinationInfo, MeasurementSystemUnitType, NativeInitialSettings, SelectOption} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from '../test_util.m.js';

/** @return {!NativeInitialSettings} */
export function getDefaultInitialSettings() {
  return {
    isInKioskAutoPrintMode: false,
    isInAppKioskMode: false,
    pdfPrinterDisabled: false,
    thousandsDelimiter: ',',
    decimalDelimiter: '.',
    previewIsPdf: false,
    previewModifiable: true,
    documentTitle: 'title',
    documentHasSelection: true,
    shouldPrintSelectionOnly: false,
    previewIsFromArc: false,
    printerName: 'FooDevice',
    serializedAppStateStr: null,
    serializedDefaultDestinationSelectionRulesStr: null,
    destinationsManaged: false,
    syncAvailable: false,
    uiLocale: 'en-us',
    unitType: MeasurementSystemUnitType.IMPERIAL,
    isDriveMounted: true,
  };
}

/**
 * @param {string} printerId
 * @param {string=} opt_printerName Defaults to an empty string.
 * @return {!{printer: !LocalDestinationInfo, capabilities: !Cdd}}
 */
export function getCddTemplate(printerId, opt_printerName) {
  const template = {
    printer: {
      deviceName: printerId,
      printerName: opt_printerName || '',
    },
    capabilities: {
      version: '1.0',
      printer: {
        supported_content_type: [{content_type: 'application/pdf'}],
        collate: {default: true},
        copies: {default: 1, max: DEFAULT_MAX_COPIES},
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
              name: 'CUSTOM',
              width_microns: 215900,
              height_microns: 215900,
              custom_display_name: 'CUSTOM_SQUARE',
            }
          ]
        }
      }
    }
  };
  if (isChromeOS) {
    template.capabilities.printer.pin = {supported: true};
  }
  return template;
}

/**
 * Gets a CDD template and adds some dummy vendor capabilities. For select
 * capabilities, the values of these options are arbitrary. These values are
 * provided and read by the destination, so there are no fixed options like
 * there are for margins or color.
 * @param {number} numSettings
 * @param {string} printerId
 * @param {string=} opt_printerName Defaults to an empty string.
 * @return {!CapabilitiesResponse}
 */
export function getCddTemplateWithAdvancedSettings(
    numSettings, printerId, opt_printerName) {
  const template = getCddTemplate(printerId, opt_printerName);
  if (numSettings < 1) {
    return template;
  }

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

  if (numSettings < 2) {
    return template;
  }

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

  if (numSettings < 3) {
    return template;
  }

  template.capabilities.printer.vendor_capability.push({
    display_name: 'Watermark',
    id: 'watermark',
    type: 'TYPED_VALUE',
    typed_value_cap: {
      default: '',
    }
  });

  if (numSettings < 4) {
    return template;
  }

  template.capabilities.printer.vendor_capability.push({
    display_name: 'Staple',
    id: 'finishings/4',
    type: 'TYPED_VALUE',
    typed_value_cap: {
      default: '',
      value_type: 'BOOLEAN',
    }
  });

  return template;
}

/**
 * Creates a destination with a certificate status tag.
 * @param {string} id Printer id
 * @param {string} name Printer display name
 * @param {boolean} invalid Whether printer has an invalid certificate.
 * @return {!Destination}
 */
export function createDestinationWithCertificateStatus(id, name, invalid) {
  const tags = {
    certificateStatus: invalid ? DestinationCertificateStatus.NO :
                                 DestinationCertificateStatus.UNKNOWN,
    account: 'foo@chromium.org',
  };
  const dest = new Destination(
      id, DestinationType.GOOGLE, DestinationOrigin.COOKIES, name,
      DestinationConnectionStatus.ONLINE, tags);
  return dest;
}

/**
 * @return {!CapabilitiesResponse} The capabilities of
 *     the Save as PDF destination.
 */
export function getPdfPrinter() {
  return {
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
 * @param {!CapabilitiesResponse} device
 * @return {{width_microns: number,
 *           height_microns: number}} The width and height of the default
 *     media.
 */
export function getDefaultMediaSize(device) {
  const size = device.capabilities.printer.media_size.option.find(
      opt => !!opt.is_default);
  return {
    width_microns: size.width_microns,
    height_microns: size.height_microns
  };
}

/**
 * Get the default page orientation for |device|.
 * @param {!CapabilitiesResponse} device
 * @return {string} The default orientation.
 */
export function getDefaultOrientation(device) {
  const options = device.capabilities.printer.page_orientation.option;
  return assert(options.find(opt => !!opt.is_default).type);
}

/**
 * Creates 5 local destinations, adds them to |localDestinations|.
 * @param {!Array<!LocalDestinationInfo>} localDestinations
 * @return {!Array<!Destination>}
 */
export function getDestinations(localDestinations) {
  const destinations = [];
  const origin = isChromeOS ? DestinationOrigin.CROS : DestinationOrigin.LOCAL;
  // Five destinations. FooDevice is the system default.
  [{deviceName: 'ID1', printerName: 'One'},
   {deviceName: 'ID2', printerName: 'Two'},
   {deviceName: 'ID3', printerName: 'Three'},
   {deviceName: 'ID4', printerName: 'Four'},
   {deviceName: 'FooDevice', printerName: 'FooName'}]
      .forEach((info, index) => {
        const destination = new Destination(
            info.deviceName, DestinationType.LOCAL, origin, info.printerName,
            DestinationConnectionStatus.ONLINE);
        localDestinations.push(info);
        destinations.push(destination);
      });
  return destinations;
}

/**
 * Returns a media size capability with custom and localized names.
 * @return {!{ option: Array<!SelectOption> }}
 */
export function getMediaSizeCapabilityWithCustomNames() {
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
 * @param {!HTMLInputElement} inputElement
 * @param {!string} input The value to set for the input element.
 * @param {!HTMLElement} parentElement The element that receives the
 *     input-change event.
 * @return {!Promise} Promise that resolves when the input-change event has
 *     fired.
 */
export function triggerInputEvent(inputElement, input, parentElement) {
  inputElement.value = input;
  inputElement.dispatchEvent(
      new CustomEvent('input', {composed: true, bubbles: true}));
  return eventToPromise('input-change', parentElement);
}

export function setupTestListenerElement() {
  Polymer({
    is: 'test-listener-element',
    behaviors: [WebUIListenerBehavior],
  });
  const testElement = document.createElement('test-listener-element');
  document.body.appendChild(testElement);
}

/** @return {!DestinationStore} */
export function createDestinationStore() {
  const testListenerElement = document.createElement('test-listener-element');
  document.body.appendChild(testListenerElement);
  return new DestinationStore(
      testListenerElement.addWebUIListener.bind(testListenerElement));
}

/**
 * @param {string} account The user account the destination should be
 *     associated with.
 * @return {!Destination} The Google Drive destination.
 */
export function getGoogleDriveDestination(account) {
  return new Destination(
      Destination.GooglePromotedId.DOCS, DestinationType.GOOGLE,
      DestinationOrigin.COOKIES, Destination.GooglePromotedId.DOCS,
      DestinationConnectionStatus.ONLINE, {account: account});
}

/** @return {!Destination} The Save as PDF destination. */
export function getSaveAsPdfDestination() {
  return new Destination(
      Destination.GooglePromotedId.SAVE_AS_PDF, DestinationType.LOCAL,
      DestinationOrigin.LOCAL, loadTimeData.getString('printToPDF'),
      DestinationConnectionStatus.ONLINE);
}

/**
 * @param {!HTMLElement} section The settings section that contains the
 *    select to toggle.
 * @param {string} option The option to select.
 * @return {!Promise} Promise that resolves when the option has been
 *     selected and the process-select-change event has fired.
 */
export function selectOption(section, option) {
  const select = section.$$('select');
  select.value = option;
  select.dispatchEvent(new CustomEvent('change'));
  return eventToPromise('process-select-change', section);
}
