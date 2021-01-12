// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_fonts_css.js';
import './scanning_shared_css.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState} from './scanning_app_types.js';
import {ScanningBrowserProxy, ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/** @type {number} */
const PROGRESS_TIMER_MS = 3000;

/**
 * @fileoverview
 * 'scan-preview' shows a preview of a scanned document.
 */
Polymer({
  is: 'scan-preview',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private {?ScanningBrowserProxy}*/
  browserProxy_: null,

  properties: {
    /** @type {!AppState} */
    appState: {
      type: Number,
      observer: 'onAppStateChange_',
    },

    /**
     * The object URLs of the scanned images.
     * @type {!Array<string>}
     */
    objectUrls: Array,

    /** @type {number} */
    pageNumber: {
      type: Number,
      observer: 'onPageNumberChange_',
    },

    /** @type {number} */
    progressPercent: Number,

    /** @private {boolean} */
    showScannedImages_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showHelperText_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    showScanProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showCancelingProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    progressTextString_: String,

    /** @private {string} */
    previewAriaLabel_: String,

    /** @private {?number} */
    progressTimer_: {
      type: Number,
      value: null,
    },
  },

  observers: [
    'setPreviewAriaLabel_(showScannedImages_, showCancelingProgress_,' +
        ' showHelperText_)',
    'setScanProgressTimer_(showScanProgress_, progressPercent)',
  ],

  /** @override */
  created() {
    // ScanningBrowserProxy is initialized when scanning_app.js is created.
    this.browserProxy_ = ScanningBrowserProxyImpl.getInstance();
  },

  /** @private */
  onAppStateChange_() {
    this.showScannedImages_ = this.appState === AppState.DONE;
    this.showScanProgress_ = this.appState === AppState.SCANNING;
    this.showCancelingProgress_ = this.appState === AppState.CANCELING;
    this.showHelperText_ = !this.showScanProgress_ &&
        !this.showCancelingProgress_ && !this.showScannedImages_;
  },

  /** @private */
  onPageNumberChange_() {
    this.progressTextString_ =
        this.i18n('scanPreviewProgressText', this.pageNumber);
  },

  /**
   * Sets the ARIA label used by the preview area based on the app state and the
   * current page showing. In the initial state, use the scan preview
   * instructions from the page as the label. When the scan completes, announce
   * the total number of pages scanned.
   * @private
   */
  setPreviewAriaLabel_() {
    if (this.showScannedImages_) {
      this.browserProxy_
          .getPluralString('scannedImagesAriaLabel', this.objectUrls.length)
          .then(
              /* @type {string} */ (pluralString) => this.previewAriaLabel_ =
                  pluralString);
      return;
    }

    if (this.showCancelingProgress_) {
      this.previewAriaLabel_ = this.i18n('cancelingScanningText');
      return;
    }

    if (this.showHelperText_) {
      this.previewAriaLabel_ = this.i18n('scanPreviewHelperText');
      return;
    }
  },

  /**
   * When receiving progress updates from an ongoing scan job, only update the
   * preview section aria label after a timer elapses to prevent successive
   * progress updates from spamming ChromeVox.
   * @private
   */
  setScanProgressTimer_() {
    // Only set the timer if scanning is still in progress.
    if (!this.showScanProgress_) {
      return;
    }

    // Always announce when a page is completed. Bypass and clear any existing
    // timer and immediately update the aria label.
    if (this.progressPercent === 100) {
      clearTimeout(this.progressTimer_);
      this.onScanProgressTimerComplete_();
      return;
    }

    // If a timer is already in progress, do not set another timer.
    if (this.progressTimer_) {
      return;
    }

    this.progressTimer_ = setTimeout(
        () => this.onScanProgressTimerComplete_(), PROGRESS_TIMER_MS);
  },

  /** @private */
  onScanProgressTimerComplete_() {
    // Only update the aria label if scanning is still in progress.
    if (!this.showScanProgress_) {
      return;
    }

    this.previewAriaLabel_ = this.i18n(
        'scanningImagesAriaLabel', this.pageNumber, this.progressPercent);
    this.progressTimer_ = null;
  },
});
