// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as barcodeChip from '../../barcode_chip.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../../device/device_info_updater.js';
import * as dom from '../../dom.js';
import {sendBarcodeEnabledEvent} from '../../metrics.js';
import {BarcodeScanner} from '../../models/barcode.js';
import * as state from '../../state.js';
import {Mode} from '../../type.js';

/**
 * Controller for the scanner options of Camera view.
 */
export class ScannerOptions {
  /**
   * @param {function(): !Promise} reconfigure Request camera reconfiguration.
   * @param {!DeviceInfoUpdater} infoUpdater
   */
  constructor(reconfigure, infoUpdater) {
    /**
     * @type {function(): !Promise}
     * @private
     */
    this.reconfigure_ = reconfigure;

    /**
     * Togglable barcode option in photo mode.
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.photoBarcodeOption_ = dom.get('#toggle-barcode', HTMLInputElement);

    /**
     * Barcode scanner type option in scanner mode.
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.scannerBarcodeOption_ = dom.get('#scanner-barcode', HTMLInputElement);

    /**
     * May be null if preview is not ready.
     * @type {?BarcodeScanner}
     * @private
     */
    this.scanner_ = null;

    /**
     * @type {boolean}
     * @private
     */
    this.hasCameraSupportDocumentMode_ = false;

    const updateShowScannerMode = () => {
      state.set(
          state.State.SHOW_SCANNER_MODE,
          this.hasCameraSupportDocumentMode_ ||
              state.get(state.State.ENABLE_DOCUMENT_MODE_ON_ALL_CAMERAS));
    };
    state.addObserver(state.State.ENABLE_DOCUMENT_MODE_ON_ALL_CAMERAS, () => {
      this.reconfigure_();
      updateShowScannerMode();
    });
    infoUpdater.addDeviceChangeListener(async () => {
      const devicesInfo = await infoUpdater.getCamera3DevicesInfo();
      if (devicesInfo === null) {
        return;
      }
      this.hasCameraSupportDocumentMode_ =
          devicesInfo.some(({supportDocumentScan}) => supportDocumentScan);
      updateShowScannerMode();
    });
    [state.State.SHOW_SCANNER_MODE, state.State.SCAN_BARCODE].forEach((s) => {
      state.addObserver(s, (value) => {
        if (state.get(state.State.CAMERA_CONFIGURING)) {
          return;
        }
        this.updateOption_(state.get(state.State.SCAN_BARCODE));
      });
    });
  }

  /**
   * @return {boolean} Whether barcode option is toggled.
   */
  isBarcodeOptionToggled_() {
    if (!state.get(state.State.SCAN_BARCODE)) {
      return false;
    }
    if (state.get(state.State.SHOW_SCANNER_MODE)) {
      return state.get(Mode.SCANNER);
    } else {
      return state.get(Mode.PHOTO);
    }
  }

  /**
   * @param {!HTMLVideoElement} video
   */
  async initialize(video) {
    this.scanner_ = new BarcodeScanner(video, (value) => {
      barcodeChip.show(value);
    });
    this.updateOption_(this.isBarcodeOptionToggled_());
  }

  /**
   * @param {boolean} toggled Whether barcode scanner option is toggled.
   * @private
   */
  updateOption_(toggled) {
    if (this.scanner_ === null) {
      return;
    }

    this.updateOptionsUI_(toggled);
    const mode =
        state.get(state.State.SHOW_SCANNER_MODE) ? Mode.SCANNER : Mode.PHOTO;
    if (state.get(mode) && toggled) {
      sendBarcodeEnabledEvent();
      this.scanner_.start();
      state.set(state.State.ENABLE_SCAN_BARCODE, true);
    } else {
      this.stopBarcodeScanner_();
    }
  }

  /**
   * @private
   */
  stopBarcodeScanner_() {
    this.scanner_.stop();
    barcodeChip.dismiss();
    state.set(state.State.ENABLE_SCAN_BARCODE, false);
  }

  /**
   * @param {boolean} toggled Whether barcode scanner option is toggled.
   * @private
   */
  updateOptionsUI_(toggled) {
    this.photoBarcodeOption_.checked = toggled;
    this.scannerBarcodeOption_.checked = toggled;
    state.set(state.State.SCAN_BARCODE, toggled);
  }

  /**
   * Stops all scanner.
   */
  async uninitialize() {
    if (this.scanner_ !== null) {
      this.stopBarcodeScanner_();
      this.scanner_= null;
    }
  }
}
