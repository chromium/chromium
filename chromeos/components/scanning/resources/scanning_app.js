// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './scanner_select.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getScanService} from './mojo_interface_provider.js';
import {ScannerArr} from './scanning_app_types.js';
import {tokenToString} from './scanning_app_util.js';

/**
 * @fileoverview
 * 'scanning-app' is used to interact with connected scanners.
 */
Polymer({
  is: 'scanning-app',

  _template: html`{__html_template__}`,

  /** @private {?chromeos.scanning.mojom.ScanServiceInterface} */
  scanService_: null,

  /** @private {!Map<!string, !mojoBase.mojom.UnguessableToken>} */
  scannerIds_: new Map(),

  properties: {
    /**
     * @type {!ScannerArr}
     * @private
     */
    scanners_: {
      type: Array,
      value: () => [],
    },

    /**
     * @type {?mojoBase.mojom.UnguessableToken}
     * @private
     */
    selectedScannerId_: Object,

    /** @private */
    loaded_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'selected-scanner-change': 'onSelectedScannerChange_',
  },

  /** @override */
  created() {
    this.scanService_ = getScanService();
  },

  /** @override */
  ready() {
    this.scanService_.getScanners().then(this.onScannersReceived_.bind(this));
  },

  /**
   * @param {!{scanners: !ScannerArr}} response
   * @private
   */
  onScannersReceived_(response) {
    this.loaded_ = true;
    this.scanners_ = response.scanners;
    for (const scanner of this.scanners_) {
      this.scannerIds_.set(tokenToString(scanner.id), scanner.id);
    }

    if (!this.scanners_.length) {
      return;
    }

    // Since the first scanner is the default option in the dropdown, set the
    // selected ID to the fist scanner's ID until a different scanner is
    // selected.
    this.selectedScannerId_ = this.scanners_[0].id;
    // TODO(jschettler): Get capabilities for the scanner.
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelectedScannerChange_(event) {
    const value = event.detail.value;
    if (!this.scannerIds_.has(value)) {
      return;
    }

    this.selectedScannerId_ = this.scannerIds_.get(value);
    // TODO(jschettler): Get capabilities for the selected scanner.
  },
});
