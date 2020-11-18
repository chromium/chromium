// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {alphabeticalCompare, getPageSizeString} from './scanning_app_util.js';
import {SelectBehavior} from './select_behavior.js';

/** @type {chromeos.scanning.mojom.PageSize} */
const DEFAULT_PAGE_SIZE = chromeos.scanning.mojom.PageSize.kNaLetter;

/**
 * @fileoverview
 * 'page-size-select' displays the available page sizes in a dropdown.
 */
Polymer({
  is: 'page-size-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  properties: {
    /** @type {!Array<chromeos.scanning.mojom.PageSize>} */
    pageSizes: {
      type: Array,
      value: () => [],
    },

    /** @type {string} */
    selectedPageSize: {
      type: String,
      notify: true,
    },
  },

  observers: ['onPageSizesChange_(pageSizes.*)'],

  /**
   * @param {!chromeos.scanning.mojom.PageSize} pageSize
   * @return {string}
   * @private
   */
  getPageSizeString_(pageSize) {
    return getPageSizeString(pageSize);
  },

  /**
   * Letter should be the default option if it exists. If not, use the first
   * page size in the page sizes array.
   * @return {string}
   * @private
   */
  getDefaultSelectedPageSize_() {
    let defaultPageSizeIndex = this.pageSizes.findIndex((pageSize) => {
      return this.isDefaultPageSize_(pageSize);
    });
    if (defaultPageSizeIndex === -1) {
      defaultPageSizeIndex = 0;
    }

    return this.pageSizes[defaultPageSizeIndex].toString();
  },

  /**
   * Sorts the page sizes and sets the selected page size when the page sizes
   * array changes.
   * @private
   */
  onPageSizesChange_() {
    if (this.pageSizes.length > 1) {
      this.pageSizes = this.customSort(
          this.pageSizes, alphabeticalCompare,
          (pageSize) => getPageSizeString(pageSize));

      // If the fit to scan area option exists, move it to the end of the page
      // sizes array.
      const fitToScanAreaIndex = this.pageSizes.findIndex((pageSize) => {
        return pageSize === chromeos.scanning.mojom.PageSize.kMax;
      });
      if (fitToScanAreaIndex !== -1) {
        this.pageSizes.push(this.pageSizes.splice(fitToScanAreaIndex, 1)[0]);
      }
    }

    if (this.pageSizes.length > 0) {
      this.selectedPageSize = this.getDefaultSelectedPageSize_();
    }
  },

  /**
   * @param {!chromeos.scanning.mojom.PageSize} pageSize
   * @return {boolean}
   * @private
   */
  isDefaultPageSize_(pageSize) {
    return pageSize === DEFAULT_PAGE_SIZE;
  },
});
