// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './file_path.mojom-lite.js';
import './color_mode_select.js';
import './file_type_select.js';
import './page_size_select.js';
import './resolution_select.js';
import './scan_done_section.js';
import './scan_preview.js';
import './scan_to_select.js';
import './scanner_select.js';
import './scanning_fonts_css.js';
import './scanning_shared_css.js';
import './source_select.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getScanService} from './mojo_interface_provider.js';
import {AppState, ScannerArr} from './scanning_app_types.js';
import {colorModeFromString, fileTypeFromString, pageSizeFromString, tokenToString} from './scanning_app_util.js';
import {ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

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

  behaviors: [CrContainerShadowBehavior, I18nBehavior],

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
      observer: 'onAppStateChange_',
    },

    /**
     * The object URLs of the scanned images.
     * @private {!Array<string>}
     */
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

    /**
     * Determines whether settings should be disabled based on the current app
     * state. Settings should be disabled until after the selected scanner's
     * capabilities are fetched since the capabilities determine what options
     * are available in the settings. They should also be disabled while
     * scanning since settings cannot be changed while a scan is in progress.
     * @private {boolean}
     */
    settingsDisabled_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    scannersLoaded_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showDoneSection_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showCancelButton_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    cancelButtonDisabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * The file path of the last scanned page of a successful scan job. Used to
     * open the Files app with the correct file highlighted.
     * @private {?mojoBase.mojom.FilePath}
     */
    lastScannedFilePath_: Object,

    /**
     * The key to retrieve the appropriate string to display in the toast.
     * @private {string}
     */
    toastMessageKey_: {
      type: String,
      observer: 'onToastMessageKeyChange_',
    },

    /** @private {boolean} */
    showToastInfoIcon_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showToastHelpLink_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  created() {
    this.scanService_ = getScanService();
    this.selectedFilePath = DEFAULT_SAVE_DIRECTORY;

    ScanningBrowserProxyImpl.getInstance().initialize();
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
    assert(
        this.appState_ === AppState.SCANNING ||
        this.appState_ === AppState.CANCELING);
    this.pageNumber_ = pageNumber;
    this.progressPercent_ = progressPercent;
  },

  /**
   * Overrides chromeos.scanning.mojom.ScanJobObserverInterface.
   * @param {!Array<number>} pageData
   */
  onPageComplete(pageData) {
    assert(
        this.appState_ === AppState.SCANNING ||
        this.appState_ === AppState.CANCELING);
    const blob = new Blob([Uint8Array.from(pageData)], {'type': 'image/png'});
    this.push('objectUrls_', URL.createObjectURL(blob));
  },

  /**
   * Overrides chromeos.scanning.mojom.ScanJobObserverInterface.
   * @param {boolean} success
   * @param {!mojoBase.mojom.FilePath} lastScannedFilePath
   */
  onScanComplete(success, lastScannedFilePath) {
    if (!success || this.objectUrls_.length == 0) {
      this.$.scanFailedDialog.showModal();
      return;
    }

    this.lastScannedFilePath_ = lastScannedFilePath;
    this.setAppState_(AppState.DONE);
  },

  /**
   * Overrides chromeos.scanning.mojom.ScanJobObserverInterface.
   * @param {boolean} success
   */
  onCancelComplete(success) {
    // If the cancel request fails, continue showing the scan progress page.
    if (!success) {
      this.setAppState_(AppState.SCANNING);
      this.showToast_('cancelFailedToastText');
      return;
    }

    this.showToast_('scanCanceledToastText');
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
    this.selectedFileType = chromeos.scanning.mojom.FileType.kPdf.toString();
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
    // Force hide the toast if user attempts a new scan before the toast times
    // out.
    this.$.toast.hide();

    if (!this.selectedScannerId || !this.selectedSource ||
        !this.selectedFileType || !this.selectedColorMode ||
        !this.selectedPageSize || !this.selectedResolution) {
      this.showToast_('startScanFailedToast');
      return;
    }

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
      this.showToast_('startScanFailedToast');
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
   * @return {string}
   * @private
   */
  getFileSavedText_() {
    const fileSavedText =
        this.pageNumber_ > 1 ? 'fileSavedTextPlural' : 'fileSavedText';
    return this.i18n(fileSavedText);
  },

  /** @private */
  onCancelClick_() {
    assert(this.appState_ === AppState.SCANNING);
    this.setAppState_(AppState.CANCELING);
    this.scanService_.cancelScan();
  },

  /**
   * Revokes and removes all of the object URLs.
   * @private
   */
  clearObjectUrls_() {
    for (const url of this.objectUrls_) {
      URL.revokeObjectURL(url);
    }
    this.objectUrls_ = [];
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
            this.appState_ === AppState.DONE ||
            this.appState_ === AppState.CANCELING);
        this.clearObjectUrls_();
        break;
      case (AppState.SCANNING):
        assert(
            this.appState_ === AppState.READY ||
            this.appState_ === AppState.CANCELING);
        break;
      case (AppState.DONE):
        assert(
            this.appState_ === AppState.SCANNING ||
            this.appState_ === AppState.CANCELING);
        break;
      case (AppState.CANCELING):
        assert(this.appState_ === AppState.SCANNING);
        break;
    }

    this.appState_ = newState;
  },

  /** @private */
  onAppStateChange_() {
    this.scannersLoaded_ = this.appState_ !== AppState.GETTING_SCANNERS;
    this.settingsDisabled_ = this.appState_ !== AppState.READY;
    this.showCancelButton_ = this.appState_ === AppState.SCANNING ||
        this.appState_ === AppState.CANCELING;
    this.cancelButtonDisabled_ = this.appState_ === AppState.CANCELING;
    this.showDoneSection_ = this.appState_ === AppState.DONE;

    // Need to wait for elements to render after updating their disabled and
    // hidden attributes before they can be focused.
    afterNextRender(this, () => {
      if (this.appState_ === AppState.READY) {
        this.$$('#scannerSelect').$$('#scannerSelect').focus();
      } else if (this.appState_ === AppState.SCANNING) {
        this.$$('#cancelButton').focus();
      } else if (this.appState_ === AppState.DONE) {
        this.$$('#scanPreview').$$('#previewDiv').focus();
      }
    });
  },

  /**
   * @param {string} toastMessageKey
   * @private
   */
  showToast_(toastMessageKey) {
    this.toastMessageKey_ = toastMessageKey;
    this.$.toast.show();
  },

  /** @private */
  onToastMessageKeyChange_() {
    this.showToastInfoIcon_ = this.toastMessageKey_ !== 'scanCanceledToastText';
    this.showToastHelpLink_ =
        this.toastMessageKey_ !== 'scanCanceledToastText' &&
        this.toastMessageKey_ !== 'fileNotFoundToastText';
  },

  /** @private */
  onFileNotFound_() {
    this.showToast_('fileNotFoundToastText');
  },

  /** @private */
  onDialogOkClick_() {
    this.$.scanFailedDialog.close();
    this.setAppState_(AppState.READY);
  },

  /** @private */
  onDialogGetHelpClick_() {
    this.$.scanFailedDialog.close();
    this.setAppState_(AppState.READY);
    window.open('http://support.google.com/chromebook?p=chrome_scanning');
  },
});
