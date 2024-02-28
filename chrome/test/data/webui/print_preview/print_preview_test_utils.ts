// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CapabilitiesResponse, Cdd, ColorOption, DpiOption, DuplexOption, ExtensionDestinationInfo, LocalDestinationInfo, MediaSizeCapability, MediaSizeOption, MediaTypeOption, NativeInitialSettings, PageOrientationOption} from 'chrome://print/print_preview.js';
import {DEFAULT_MAX_COPIES, Destination, DestinationOrigin, DestinationStore, GooglePromotedDestinationId, MeasurementSystemUnitType, VendorCapabilityValueType} from 'chrome://print/print_preview.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

export function getDefaultInitialSettings(isPdf: boolean = false):
    NativeInitialSettings {
  return {
    isInKioskAutoPrintMode: false,
    isInAppKioskMode: false,
    pdfPrinterDisabled: false,
    thousandsDelimiter: ',',
    decimalDelimiter: '.',
    previewModifiable: !isPdf,
    documentTitle: 'title',
    documentHasSelection: true,
    shouldPrintSelectionOnly: false,
    previewIsFromArc: false,
    printerName: 'FooDevice',
    serializedAppStateStr: null,
    serializedDefaultDestinationSelectionRulesStr: null,
    destinationsManaged: false,
    uiLocale: 'en-us',
    unitType: MeasurementSystemUnitType.IMPERIAL,
    isDriveMounted: true,
  };
}

export function getCddTemplate(
    printerId: string, printerName?: string): CapabilitiesResponse {
  const template: CapabilitiesResponse = {
    printer: {
      deviceName: printerId,
      printerName: printerName || '',
    },
    capabilities: {
      version: '1.0',
      printer: {
        collate: {default: true},
        copies: {default: 1, max: DEFAULT_MAX_COPIES},
        color: {
          option: [
            {type: 'STANDARD_COLOR', is_default: true},
            {type: 'STANDARD_MONOCHROME'},
          ] as ColorOption[],
        },
        dpi: {
          option: [
            {horizontal_dpi: 200, vertical_dpi: 200, is_default: true},
            {horizontal_dpi: 100, vertical_dpi: 100},
          ] as DpiOption[],
        },
        duplex: {
          option: [
            {type: 'NO_DUPLEX', is_default: true},
            {type: 'LONG_EDGE'},
            {type: 'SHORT_EDGE'},
          ] as DuplexOption[],
        },
        page_orientation: {
          option: [
            {type: 'PORTRAIT', is_default: true},
            {type: 'LANDSCAPE'},
            {type: 'AUTO'},
          ] as PageOrientationOption[],
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
              has_borderless_variant: true,
            },
            {
              name: 'LEGAL',
              width_microns: 215900,
              height_microns: 355600,
              custom_display_name: 'Legal',
              imageable_area_left_microns: 5000,
              imageable_area_bottom_microns: 5000,
              imageable_area_right_microns: 5000,
              imageable_area_top_microns: 5000,
              has_borderless_variant: false,
            },
            {
              name: '4x6',
              width_microns: 101600,
              height_microns: 152400,
              custom_display_name: '4 x 6 in',
              imageable_area_left_microns: 0,
              imageable_area_bottom_microns: 0,
              imageable_area_right_microns: 101600,
              imageable_area_top_microns: 152400,
            },
          ] as MediaSizeOption[],
        },
        media_type: {
          option: [
            {
              vendor_id: 'stationery',
              custom_display_name: 'Plain',
              is_default: true,
            },
            {
              vendor_id: 'photographic',
              custom_display_name: 'Photo',
            },
          ] as MediaTypeOption[],
        },
      },
    },
  };
  // <if expr="is_chromeos">
  template.capabilities!.printer.pin = {supported: true};
  // </if>
  return template;
}

/**
 * Gets a CDD template and adds some dummy vendor capabilities. For select
 * capabilities, the values of these options are arbitrary. These values are
 * provided and read by the destination, so there are no fixed options like
 * there are for margins or color.
 * @param printerName Defaults to an empty string.
 */
export function getCddTemplateWithAdvancedSettings(
    numSettings: number, printerId: string,
    printerName?: string): CapabilitiesResponse {
  const template = getCddTemplate(printerId, printerName);
  if (numSettings < 1) {
    return template;
  }

  template.capabilities!.printer.vendor_capability = [{
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
  template.capabilities!.printer.vendor_capability!.push({
    display_name: 'Paper Type',
    id: 'paperType',
    type: 'SELECT',
    select_cap: {
      option: [
        {display_name: 'Standard', value: 0, is_default: true},
        {display_name: 'Recycled', value: 1},
        {display_name: 'Special', value: 2},
      ],
    },
  });

  if (numSettings < 3) {
    return template;
  }

  template.capabilities!.printer.vendor_capability!.push({
    display_name: 'Watermark',
    id: 'watermark',
    type: 'TYPED_VALUE',
    typed_value_cap: {
      default: '',
    },
  });

  if (numSettings < 4) {
    return template;
  }

  template.capabilities!.printer.vendor_capability.push({
    display_name: 'Staple',
    id: 'finishings/4',
    type: 'TYPED_VALUE',
    typed_value_cap: {
      default: '',
      value_type: VendorCapabilityValueType.BOOLEAN,
    },
  });

  return template;
}

/**
 * @return The capabilities of the Save as PDF destination.
 */
export function getPdfPrinter(): {capabilities: Cdd} {
  return {
    capabilities: {
      version: '1.0',
      printer: {
        page_orientation: {
          option: [
            {type: 'AUTO', is_default: true},
            {type: 'PORTRAIT'},
            {type: 'LANDSCAPE'},
          ] as PageOrientationOption[],
        },
        color: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
        media_size: {
          option: [{
            name: 'NA_LETTER',
            width_microns: 0,
            height_microns: 0,
            is_default: true,
          }],
        },
      },
    },
  };
}

/**
 * Get the default media size for |device|.
 * @return The width and height of the default media.
 */
export function getDefaultMediaSize(device: CapabilitiesResponse):
    MediaSizeOption {
  const size = device.capabilities!.printer.media_size!.option!.find(
      opt => !!opt.is_default);
  return {
    width_microns: size!.width_microns,
    height_microns: size!.height_microns,
  };
}

/**
 * Get the default page orientation for |device|.
 * @return The default orientation.
 */
export function getDefaultOrientation(device: CapabilitiesResponse): string {
  const options = device.capabilities!.printer.page_orientation!.option;
  const orientation = options!.find(opt => !!opt.is_default)!.type;
  assert(orientation);
  return orientation;
}

interface ExtensionPrinters {
  destinations: Destination[];
  infoLists: ExtensionDestinationInfo[][];
}

export function getExtensionDestinations(): ExtensionPrinters {
  const destinations: Destination[] = [];
  const infoLists: ExtensionDestinationInfo[][] = [];
  infoLists.push([]);
  infoLists.push([]);
  [{
    id: 'IDA',
    name: 'PrinterA',
    extensionId: 'ext1',
    extensionName: 'ExtensionOne',
  },
   {
     id: 'IDB',
     name: 'PrinterB',
     extensionId: 'ext1',
     extensionName: 'ExtensionOne',
   },
   {
     id: 'IDC',
     name: 'PrinterC',
     extensionId: 'ext2',
     extensionName: 'ExtensionTwo',
   },
  ].forEach(info => {
    const destination =
        new Destination(info.id, DestinationOrigin.EXTENSION, info.name, {
          extensionId: info.extensionId,
          extensionName: info.extensionName,
        });
    if (info.extensionId === 'ext1') {
      infoLists[0]!.push(info);
    } else {
      infoLists[1]!.push(info);
    }
    destinations.push(destination);
  });
  return {destinations, infoLists};
}

/**
 * Creates 5 local destinations, adds them to |localDestinations|.
 */
export function getDestinations(localDestinations: LocalDestinationInfo[]):
    Destination[] {
  const destinations: Destination[] = [];
  // <if expr="not is_chromeos">
  const origin = DestinationOrigin.LOCAL;
  // </if>
  // <if expr="is_chromeos">
  const origin = DestinationOrigin.CROS;
  // </if>
  // Five destinations. FooDevice is the system default.
  [{deviceName: 'ID1', printerName: 'One'},
   {deviceName: 'ID2', printerName: 'Two'},
   {deviceName: 'ID3', printerName: 'Three'},
   {deviceName: 'ID4', printerName: 'Four'},
   {deviceName: 'FooDevice', printerName: 'FooName'}]
      .forEach(info => {
        const destination =
            new Destination(info.deviceName, origin, info.printerName);
        localDestinations.push(info);
        destinations.push(destination);
      });
  return destinations;
}

/**
 * Returns a media size capability with custom and localized names.
 */
export function getMediaSizeCapabilityWithCustomNames(): MediaSizeCapability {
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
            [{locale: navigator.language, value: customLocalizedMediaName}],
      },
      {
        name: 'CUSTOM',
        width_microns: 15900,
        height_microns: 79400,
        custom_display_name: customMediaName,
      },
    ] as MediaSizeOption[],
  };
}

/**
 * @param input The value to set for the input element.
 * @param parentElement The element that receives the input-change event.
 * @return Promise that resolves when the input-change event has fired.
 */
export async function triggerInputEvent(
    inputElement: HTMLInputElement|CrInputElement, input: string,
    parentElement: HTMLElement): Promise<void> {
  inputElement.value = input;
  if (inputElement.tagName === 'CR-INPUT') {
    await (inputElement as CrInputElement).updateComplete;
  }
  inputElement.dispatchEvent(
      new CustomEvent('input', {composed: true, bubbles: true}));
  return await eventToPromise('input-change', parentElement);
}

const TestListenerElementBase = WebUiListenerMixin(PolymerElement);
class TestListenerElement extends TestListenerElementBase {
  static get is() {
    return 'test-listener-element';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-listener-element': TestListenerElement;
  }
}

export function setupTestListenerElement(): void {
  customElements.define(TestListenerElement.is, TestListenerElement);
}

export function createDestinationStore(): DestinationStore {
  const testListenerElement = document.createElement('test-listener-element');
  document.body.appendChild(testListenerElement);
  return new DestinationStore(
      testListenerElement.addWebUiListener.bind(testListenerElement));
}

// <if expr="is_chromeos">
/**
 * @return The Google Drive destination.
 */
export function getGoogleDriveDestination(): Destination {
  return new Destination(
      'Save to Drive CrOS', DestinationOrigin.LOCAL, 'Save to Google Drive');
}
// </if>

/** @return The Save as PDF destination. */
export function getSaveAsPdfDestination(): Destination {
  return new Destination(
      GooglePromotedDestinationId.SAVE_AS_PDF, DestinationOrigin.LOCAL,
      loadTimeData.getString('printToPDF'));
}

/**
 * @param section The settings section that contains the select to toggle.
 * @param option The option to select.
 * @return Promise that resolves when the option has been selected and the
 *     process-select-change event has fired.
 */
export function selectOption(
    section: HTMLElement, option: string): Promise<void> {
  const select = section.shadowRoot!.querySelector('select')!;
  select.value = option;
  select.dispatchEvent(new CustomEvent('change'));
  return eventToPromise('process-select-change', section);
}

// Fake MediaQueryList used in mocking response of |window.matchMedia|.
export class FakeMediaQueryList extends EventTarget implements MediaQueryList {
  private listener_: ((e: MediaQueryListEvent) => any)|null = null;
  private matches_: boolean = false;
  private media_: string;

  constructor(media: string) {
    super();
    this.media_ = media;
  }

  addListener(listener: (e: MediaQueryListEvent) => any) {
    this.listener_ = listener;
  }

  removeListener(listener: (e: MediaQueryListEvent) => any) {
    assertEquals(listener, this.listener_);
    this.listener_ = null;
  }

  onchange() {
    if (this.listener_) {
      this.listener_(new MediaQueryListEvent(
          'change', {media: this.media_, matches: this.matches_}));
    }
  }

  get media(): string {
    return this.media_;
  }

  get matches(): boolean {
    return this.matches_;
  }

  set matches(matches: boolean) {
    if (this.matches_ !== matches) {
      this.matches_ = matches;
      this.onchange();
    }
  }
}
