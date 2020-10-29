// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './color_mode_select.js';
import './file_type_select.js';
import './page_size_select.js';
import './resolution_select.js';
import './scan_preview.js';
import './scan_to_select.js';
import './scanner_select.js';
import './scanning_shared_css.js';
import './source_select.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getScanService} from './mojo_interface_provider.js';
import {ScannerArr} from './scanning_app_types.js';
import {colorModeFromString, pageSizeFromString, tokenToString} from './scanning_app_util.js';

/**
 * @fileoverview
 * 'scanning-app' is used to interact with connected scanners.
 */
Polymer({
  is: 'scanning-app',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

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

    /** @type {?string} */
    selectedFileType: String,

    /** @type {?string} */
    selectedColorMode: String,

    /** @type {?string} */
    selectedPageSize: String,

    /** @type {?string} */
    selectedResolution: String,

    /**
     * @type {!Array<chromeos.scanning.mojom.PageSize>}
     * @private
     */
    selectedSourcePageSizes_: {
      type: Array,
      value: () => [],
      computed: 'computePageSizes_(selectedSource)',
    },

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
    this.scanService_.getScanners().then(
        /*@type {!{scanners: !ScannerArr}}*/ (response) => {
          this.onScannersReceived_(response);
        });
  },

  /**
   * @param {?string} selectedSource
   * @return {!Array<chromeos.scanning.mojom.PageSize>}
   * @private
   */
  computePageSizes_(selectedSource) {
    for (const source of this.capabilities_.sources) {
      if (source.name === selectedSource) {
        return source.pageSizes;
      }
    }

    return [];
  },

  /**
   * @param {!{capabilities: !chromeos.scanning.mojom.ScannerCapabilities}}
   *     response
   * @private
   */
  onCapabilitiesReceived_(response) {
    this.capabilities_ = response.capabilities;

    // Set the first options as the selected options since they will be the
    // first options in the dropdowns.
    this.selectedSource = this.capabilities_.sources[0].name;
    this.selectedColorMode = this.capabilities_.colorModes[0].toString();
    this.selectedPageSize =
        this.capabilities_.sources[0].pageSizes[0].toString();
    this.selectedResolution = this.capabilities_.resolutions[0].toString();

    // TODO(jschettler): Change default file type back to PDF when it's
    // supported.
    this.selectedFileType = chromeos.scanning.mojom.FileType.kPng.toString();

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
        .then(
            /*@type {!{capabilities:
                   !chromeos.scanning.mojom.ScannerCapabilities}}*/
            (response) => {
              this.onCapabilitiesReceived_(response);
            });
  },

  /** @private */
  onScanClick_() {
    if (!this.selectedScannerId || !this.selectedSource ||
        !this.selectedFileType || !this.selectedColorMode ||
        !this.selectedPageSize || !this.selectedResolution) {
      // TODO(jschettler): Replace status text with finalized i18n strings.
      this.statusText_ = 'Failed to start scan.';
      return;
    }

    // TODO(jschettler): Remove this once ScanService supports other file types.
    if (this.selectedFileType !==
        chromeos.scanning.mojom.FileType.kPng.toString()) {
      this.statusText_ = 'PNG is the only supported file type.';
      return;
    }

    this.statusText_ = 'Scanning...';
    this.settingsDisabled_ = true;
    this.scanButtonDisabled_ = true;

    // TODO(jschettler): Use the selected file type when ScanService supports
    // it. Use the selected page size when the corresponding dropdown is added.
    const settings = {
      'sourceName': this.selectedSource,
      'fileType': chromeos.scanning.mojom.FileType.kPng,
      'colorMode': colorModeFromString(this.selectedColorMode),
      'pageSize': pageSizeFromString(this.selectedPageSize),
      'resolutionDpi': Number(this.selectedResolution),
    };
    this.scanService_
        .scan(this.scannerIds_.get(this.selectedScannerId), settings)
        .then(
            /*@type {!{success: boolean}}*/ (response) => {
              this.onScanCompleted_(response);
            });
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
