// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './scanner_select.js';
import './source_select.js';

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

    /** @type (?string) */
    selectedScannerId: String,

    /**
     * @type {?chromeos.scanning.mojom.ScannerCapabilities}
     * @private
     */
    capabilities_: Object,

    /** @type {?string} */
    selectedSource: String,

    /**
     * @type {?string}
     * @private
     */
    statusText_: String,

    /** @private */
    settingsDisabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    scanButtonDisabled_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    loaded_: {
      type: Boolean,
      value: false,
    },
  },

  observers: ['onSelectedScannerIdChange_(selectedScannerId)'],

  /** @override */
  created() {
    this.scanService_ = getScanService();
  },

  /** @override */
  ready() {
    this.scanService_.getScanners().then(this.onScannersReceived_.bind(this));
  },

  /**
   * @param {!{capabilities: !chromeos.scanning.mojom.ScannerCapabilities}}
   *     response
   * @private
   */
  onCapabilitiesReceived_(response) {
    this.capabilities_ = response.capabilities;

    // Set the first source as the selected source since it will be the first
    // option in the dropdown.
    this.selectedSource = this.capabilities_.sources[0].name;

    this.scanButtonDisabled_ = false;
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
    this.selectedScannerId = tokenToString(this.scanners_[0].id);
  },

  /**
   * @param {!string} selectedScannerId
   * @private
   */
  onSelectedScannerIdChange_(selectedScannerId) {
    if (!this.scannerIds_.has(selectedScannerId)) {
      return;
    }

    this.scanButtonDisabled_ = true;

    this.scanService_
        .getScannerCapabilities(this.scannerIds_.get(selectedScannerId))
        .then(this.onCapabilitiesReceived_.bind(this));
  },

  /** @private */
  onScanClick_() {
    if (!this.selectedScannerId || !this.selectedSource) {
      // TODO(jschettler): Replace status text with finalized i18n strings.
      this.statusText_ = 'Failed to start scan.';
      return;
    }

    this.statusText_ = 'Scanning...';
    this.settingsDisabled_ = true;
    this.scanButtonDisabled_ = true;

    // TODO(jschettler): Set color mode and resolution using the selected values
    // when the corresponding dropdowns are added.
    const settings = {
      'sourceName': this.selectedSource,
      'colorMode': chromeos.scanning.mojom.ColorMode.kColor,
      'resolutionDpi': 100,
    };
    this.scanService_
        .scan(this.scannerIds_.get(this.selectedScannerId), settings)
        .then(
            /*@type {!{success: boolean}}*/ (response) => {
                this.onScanCompleted_(response)});
  },

  /**
   * @param {!{success: boolean}} response
   * @private
   */
  onScanCompleted_(response) {
    if (response.success) {
      this.statusText_ = 'Scan complete! File(s) saved to My files.';
    } else {
      this.statusText_ = 'Scan failed.';
    }

    this.settingsDisabled_ = false;
    this.scanButtonDisabled_ = false;
  },
});
