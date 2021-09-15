// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as barcodeChip from '../../barcode_chip.js';
import {assert, assertInstanceof} from '../../chrome_util.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../../device/device_info_updater.js';
import * as dom from '../../dom.js';
import {sendBarcodeEnabledEvent} from '../../metrics.js';
import {BarcodeScanner} from '../../models/barcode.js';
import {ChromeHelper} from '../../mojo/chrome_helper.js';
import * as state from '../../state.js';
import {Mode} from '../../type.js';

import {DocumentCornerOverlay} from './document_corner_overlay.js';

/**
 * @enum {string}
 */
const ScanType = {
  BARCODE: 'barcode',
  DOCUMENT: 'document',
};

const scanTypeValues = new Set(Object.values(ScanType));

/**
 * @param {!HTMLInputElement} el
 * @return {!ScanType}
 */
function getScanTypeFromElement(el) {
  const s = el.dataset['scantype'];
  assert(scanTypeValues.has(s), `No such scantype: ${s}`);
  return /** @type {!ScanType} */ (s);
}

/**
 * @param {!ScanType} type
 * @return {!HTMLInputElement}
 */
function getElemetFromScanType(type) {
  return dom.get(`input[data-scantype=${type}]`, HTMLInputElement);
}

const DEFAULT_SCAN_TYPE = ScanType.DOCUMENT;

/**
 * Controller for the scan options of Camera view.
 */
export class ScanOptions {
  /**
   * @param {{
   *   doReconfigure: function(): !Promise,
   *   doSwitchDevice: function(string): !Promise,
   *   infoUpdater: !DeviceInfoUpdater,
   * }} params
   */
  constructor({doReconfigure, doSwitchDevice, infoUpdater}) {
    /**
     * @type {function(): !Promise}
     * @private
     */
    this.doReconfigure_ = doReconfigure;

    /**
     * @type {function(string): !Promise}
     * @private
     */
    this.doSwitchDevice_ = doSwitchDevice;

    /**
     * Togglable barcode option in photo mode.
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.photoBarcodeOption_ = dom.get('#toggle-barcode', HTMLInputElement);

    /**
     * @type {!Array<!HTMLInputElement>}
     * @private
     * @const
     */
    this.scanOptions_ =
        [...dom.getAll('#scan-modes-group [data-scantype]', HTMLInputElement)];

    /**
     * Whether preview have attached as scan frame source.
     * @type {boolean}
     * @private
     */
    this.previewAttached_ = false;

    /**
     * May be null if preview is not ready.
     * @type {?BarcodeScanner}
     * @private
     */
    this.barcodeScanner_ = null;

    /**
     * @const {!DocumentCornerOverlay}
     * @private
     */
    this.documentCornerOverylay_ = new DocumentCornerOverlay();

    /**
     * List of device ids of devices supporting document mode.
     * @type {!Array<string>}
     * @private
     */
    this.docModeDevices_ = [];

    /**
     * Called when scan option changed.
     * @type {function(): void}
     * @public
     */
    this.onChange = () => {};

    const updateShowScanMode = async () => {
      const isPlatformSupport =
          await ChromeHelper.getInstance().isDocumentModeSupported();
      state.set(
          state.State.SHOW_SCAN_MODE,
          isPlatformSupport &&
              (this.docModeDevices_.length > 0 ||
               state.get(state.State.ENABLE_DOCUMENT_MODE_ON_ALL_CAMERAS)));
    };
    state.addObserver(state.State.ENABLE_DOCUMENT_MODE_ON_ALL_CAMERAS, () => {
      this.doReconfigure_();
      updateShowScanMode();
    });
    infoUpdater.addDeviceChangeListener(() => {
      const devicesInfo = infoUpdater.getCamera3DevicesInfo();
      if (devicesInfo === null) {
        return;
      }
      this.docModeDevices_ = [];
      for (const {supportDocumentScan, deviceId} of devicesInfo) {
        if (supportDocumentScan) {
          this.docModeDevices_.push(deviceId);
        }
      }
      updateShowScanMode();
    });

    [this.photoBarcodeOption_, ...this.scanOptions_].forEach((opt) => {
      opt.addEventListener('click', (evt) => {
        if (state.get(state.State.CAMERA_CONFIGURING)) {
          evt.preventDefault();
        }
      });
    });
    this.photoBarcodeOption_.addEventListener('change', () => {
      this.updateOption_(
          this.photoBarcodeOption_.checked ? ScanType.BARCODE : null);
    });
    this.scanOptions_.forEach((opt) => {
      opt.addEventListener('change', (evt) => {
        if (opt.checked) {
          this.updateOption_(this.getToggledScanOption_());
        }
      });
    });
  }

  /**
   * @return {!ScanType} Returns scan type of checked radio buttons in scan type
   *     option groups.
   */
  getToggledScanOption_() {
    const checkedEl = this.scanOptions_.find(({checked}) => checked);
    return checkedEl === undefined ? DEFAULT_SCAN_TYPE :
                                     getScanTypeFromElement(checkedEl);
  }

  /**
   * @param {string} deviceId
   * @return {boolean}
   */
  canScanDocument_(deviceId) {
    if (state.get(state.State.ENABLE_DOCUMENT_MODE_ON_ALL_CAMERAS)) {
      return true;
    }
    return this.docModeDevices_.includes(deviceId);
  }

  /**
   * Attaches to preview video as source of frames to be scanned.
   * @param {!HTMLVideoElement} video
   * @return {!Promise}
   */
  async attachPreview(video) {
    assert(!this.previewAttached_);
    this.barcodeScanner_ = new BarcodeScanner(video, (value) => {
      barcodeChip.show(value);
    });
    const {deviceId} = assertInstanceof(video.srcObject, MediaStream)
                           .getVideoTracks()[0]
                           .getSettings();
    this.documentCornerOverylay_.attach(deviceId);
    this.previewAttached_ = true;
    const scanType = (() => {
      if (!state.get(Mode.SCAN)) {
        return null;
      }
      const type = this.getToggledScanOption_();
      if (type === ScanType.DOCUMENT && !this.canScanDocument_(deviceId)) {
        return ScanType.BARCODE;
      }
      return type;
    })();
    await this.updateOption_(scanType);
  }

  /**
   * @return {boolean}
   */
  isDocumentModeEanbled() {
    return this.documentCornerOverylay_.isEnabled();
  }

  /**
   * @param {?ScanType} scanType Scan type to be enabled, null for no type is
   *     enabled.
   * @private
   */
  async updateOption_(scanType) {
    if (!this.previewAttached_) {
      return;
    }
    assert(this.barcodeScanner_ !== null);

    this.updateOptionsUI_(scanType);
    const mode = state.get(state.State.SHOW_SCAN_MODE) ? Mode.SCAN : Mode.PHOTO;
    if (state.get(mode) && scanType === ScanType.BARCODE) {
      sendBarcodeEnabledEvent();
      this.barcodeScanner_.start();
      state.set(state.State.ENABLE_SCAN_BARCODE, true);
    } else {
      this.stopBarcodeScanner_();
    }

    if (state.get(Mode.SCAN) && scanType === ScanType.DOCUMENT) {
      const currentDeviceId = this.documentCornerOverylay_.getDeviceId();
      if (!this.canScanDocument_(currentDeviceId)) {
        this.doSwitchDevice_(this.docModeDevices_[0]);
        return;
      }
      await this.documentCornerOverylay_.start();
    } else {
      await this.documentCornerOverylay_.stop();
    }

    this.onChange();
  }

  /**
   * @private
   */
  stopBarcodeScanner_() {
    this.barcodeScanner_.stop();
    barcodeChip.dismiss();
    state.set(state.State.ENABLE_SCAN_BARCODE, false);
  }

  /**
   * @param {?ScanType} scanType
   * @private
   */
  updateOptionsUI_(scanType) {
    if (state.get(Mode.SCAN)) {
      assert(scanType !== null);
      getElemetFromScanType(scanType).checked = true;
    } else if (state.get(Mode.PHOTO)) {
      this.photoBarcodeOption_.checked = scanType === ScanType.BARCODE;
    }
  }

  /**
   * Stops all scanner and detach from current preview.
   * @return {!Promise}
   */
  async detachPreview() {
    if (this.barcodeScanner_ !== null) {
      this.stopBarcodeScanner_();
      this.barcodeScanner_ = null;
    }
    await this.documentCornerOverylay_.detach();
    this.previewAttached_ = false;
  }
}
