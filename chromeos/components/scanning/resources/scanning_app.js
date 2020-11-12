// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './file_path.mojom-lite.js';
import './color_mode_select.js';
import './file_type_select.js';
import './icons.js';
import './page_size_select.js';
import './resolution_select.js';
import './scan_preview.js';
import './scan_to_select.js';
import './scanner_select.js';
import './scanning_fonts_css.js';
import './scanning_shared_css.js';
import './source_select.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getScanService} from './mojo_interface_provider.js';
import {AppState, ScannerArr} from './scanning_app_types.js';
import {colorModeFromString, fileTypeFromString, pageSizeFromString, tokenToString} from './scanning_app_util.js';

/**
 * The default save directory for completed scans.
 * @const {string}
 */
const DEFAULT_SAVE_DIRECTORY = '/home/chronos/user/MyFiles';

/**
 * @fileoverview
 * 'scanning-app' is used to interact with connected scanners.
 */
Polymer({
  is: 'scanning-app',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * Receives scan job notifications.
   * @private {?chromeos.scanning.mojom.ScanJobObserverReceiver}
   */
  scanJobObserverReceiver_: null,

  /** @private {?chromeos.scanning.mojom.ScanServiceInterface} */
  scanService_: null,

  /** @private {!Map<string, !mojoBase.mojom.UnguessableToken>} */
  scannerIds_: new Map(),

  properties: {
    /** @private {!ScannerArr} */
    scanners_: {
      type: Array,
      value: () => [],
    },

    /** @type {string} */
    selectedScannerId: {
      type: String,
      observer: 'onSelectedScannerIdChange_',
    },

    /** @private {?chromeos.scanning.mojom.ScannerCapabilities} */
    capabilities_: Object,

    /** @type {string} */
    selectedSource: String,

    /** @type {string} */
    selectedFileType: String,

    /** @type {string} */
    selectedFilePath: String,

    /** @type {string} */
    selectedColorMode: String,

    /** @type {string} */
    selectedPageSize: String,

    /** @type {string} */
    selectedResolution: String,

    /**
     * Used to determine when certain parts of the app should be shown or hidden
     * and enabled or disabled.
     * @private {!AppState}
     */
    appState_: {
      type: Number,
      value: AppState.GETTING_SCANNERS,
    },

    /** @private {!Array<string>} */
    objectUrls_: {
      type: Array,
      value: () => [],
    },

    /**
     * Used to display which page is being scanned during a scan.
     * @private {number}
     */
    pageNumber_: {
      type: Number,
      value: 1,
    },

    /**
     * Used to display a page's scan progress during a scan.
     * @private {number}
     */
    progressPercent_: {
      type: Number,
      value: 0,
    },

    /** @private {!Array<chromeos.scanning.mojom.PageSize>} */
    selectedSourcePageSizes_: {
      type: Array,
      value: () => [],
      computed: 'computePageSizes_(selectedSource)',
    },

    /** @private {string} */
    statusText_: String,
  },

  /** @override */
  created() {
    this.scanService_ = getScanService();
    this.selectedFilePath = DEFAULT_SAVE_DIRECTORY;
  },

  /** @override */
  ready() {
    window.addEventListener('beforeunload', event => {
      // When the user tries to close the app while a scan is in progress,
      // show the 'Leave site' dialog.
      if (this.appState_ === AppState.SCANNING) {
        event.preventDefault();
        event.returnValue = '';
      }
    });

    this.scanService_.getScanners().then(
        /*@type {!{scanners: !ScannerArr}}*/ (response) => {
          this.onScannersReceived_(response);
        });
  },

  /** @override */
  detached() {
    // TODO(jschettler): Cancel any ongoing scan jobs.
    if (this.scanJobObserverReceiver_) {
      this.scanJobObserverReceiver_.$.close();
    }
  },

  /**
   * Overrides chromeos.scanning.mojom.ScanJobObserverInterface.
   * @param {number} pageNumber
   * @param {number} progressPercent
   */
  onPageProgress(pageNumber, progressPercent) {
    this.pageNumber_ = pageNumber;
    this.progressPercent_ = progressPercent;
  },

  /**
   * Overrides chromeos.scanning.mojom.ScanJobObserverInterface.
   * @param {!Array<number>} pageData
   */
  onPageComplete(pageData) {
    // TODO(jschettler): Display the scanned images in the preview area when the
    // scan is complete.
    const blob = new Blob([Uint8Array.from(pageData)], {'type': 'image/png'});
    this.push('objectUrls_', URL.createObjectURL(blob));
  },

  /**
   * Overrides chromeos.scanning.mojom.ScanJobObserverInterface.
   * @param {boolean} success
   */
  onScanComplete(success) {
    if (success) {
      this.setAppState_(AppState.DONE);
      return;
    }

    this.statusText_ = 'Scan failed.';
    this.setAppState_(AppState.READY);
  },

  /**
   * @param {string} selectedSource
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

    // TODO(jschettler): Change default file type back to PDF when it's
    // supported.
    this.selectedFileType = chromeos.scanning.mojom.FileType.kPng.toString();

    this.setAppState_(AppState.READY);
  },

  /**
   * @param {!{scanners: !ScannerArr}} response
   * @private
   */
  onScannersReceived_(response) {
    this.setAppState_(AppState.GOT_SCANNERS);
    for (const scanner of response.scanners) {
      this.scannerIds_.set(tokenToString(scanner.id), scanner.id);
    }

    this.scanners_ = response.scanners;
  },

  /** @private */
  onSelectedScannerIdChange_() {
    if (!this.scannerIds_.has(this.selectedScannerId)) {
      return;
    }

    this.setAppState_(AppState.GETTING_CAPS);

    this.scanService_
        .getScannerCapabilities(this.scannerIds_.get(this.selectedScannerId))
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

    // TODO(jschettler): Remove this once ScanService supports PDF.
    if (this.selectedFileType ==
        chromeos.scanning.mojom.FileType.kPdf.toString()) {
      this.statusText_ = 'PDF is not a supported file type.';
      return;
    }

    this.statusText_ = '';
    this.objectUrls_ = [];

    const settings = {
      'sourceName': this.selectedSource,
      'scanToPath': {'path': this.selectedFilePath},
      'fileType': fileTypeFromString(this.selectedFileType),
      'colorMode': colorModeFromString(this.selectedColorMode),
      'pageSize': pageSizeFromString(this.selectedPageSize),
      'resolutionDpi': Number(this.selectedResolution),
    };

    if (!this.scanJobObserverReceiver_) {
      this.scanJobObserverReceiver_ =
          new chromeos.scanning.mojom.ScanJobObserverReceiver(
              /**
               * @type {!chromeos.scanning.mojom.ScanJobObserverInterface}
               */
              (this));
    }

    this.scanService_
        .startScan(
            this.scannerIds_.get(this.selectedScannerId), settings,
            this.scanJobObserverReceiver_.$.bindNewPipeAndPassRemote())
        .then(
            /*@type {!{success: boolean}}*/ (response) => {
              this.onStartScanResponse_(response);
            });
  },

  /** @private */
  onDoneClick_() {
    this.setAppState_(AppState.READY);
  },

  /**
   * @param {!{success: boolean}} response
   * @private
   */
  onStartScanResponse_(response) {
    if (!response.success) {
      this.statusText_ = 'Failed to start scan.';
      return;
    }

    this.setAppState_(AppState.SCANNING);
    this.pageNumber_ = 1;
    this.progressPercent_ = 0;
  },

  /** @private */
  toggleClicked_() {
    this.$$('#collapse').toggle();
  },

  /**
   * @param {boolean} opened Whether the section is expanded or not.
   * @return {string} Icon name.
   * @private
   */
  getArrowIcon_(opened) {
    return opened ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * @return {boolean}
   * @private
   */
  areScannersLoaded_() {
    return this.appState_ !== AppState.GETTING_SCANNERS;
  },

  /**
   * Determines whether settings should be disabled based on the current app
   * state. Settings should be disabled until after the selected scanner's
   * capabilities are fetched since the capabilities determine what options are
   * available in the settings. They should also be disabled while scanning
   * since settings cannot be changed while a scan is in progress.
   * @return {boolean}
   * @private
   */
  areSettingsDisabled_() {
    return this.appState_ !== AppState.READY;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowDonePage_() {
    return this.appState_ === AppState.DONE;
  },

  /**
   * @return {string}
   * @private
   */
  getFileSavedText_() {
    const fileSavedText =
        this.pageNumber_ > 1 ? 'fileSavedTextPlural' : 'fileSavedText';
    return this.i18n(fileSavedText);
  },

  /**
   * Sets the app state if the state transition is allowed.
   * @param {!AppState} newState
   * @private
   */
  setAppState_(newState) {
    switch (newState) {
      case (AppState.GETTING_SCANNERS):
        assert(this.appState_ === AppState.GETTING_SCANNERS);
        break;
      case (AppState.GOT_SCANNERS):
        assert(this.appState_ === AppState.GETTING_SCANNERS);
        break;
      case (AppState.GETTING_CAPS):
        assert(
            this.appState_ === AppState.GOT_SCANNERS ||
            this.appState_ === AppState.READY);
        break;
      case (AppState.READY):
        assert(
            this.appState_ === AppState.GETTING_CAPS ||
            this.appState_ === AppState.SCANNING ||
            this.appState_ === AppState.DONE);
        break;
      case (AppState.SCANNING):
        assert(this.appState_ === AppState.READY);
        break;
      case (AppState.DONE):
        assert(this.appState_ === AppState.SCANNING);
        break;
    }

    this.appState_ = newState;
  },
});
