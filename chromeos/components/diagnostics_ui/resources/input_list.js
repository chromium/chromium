// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InputDataProviderInterface, KeyboardInfo, TouchDeviceInfo, TouchDeviceType} from './diagnostics_types.js'
import {getInputDataProvider} from './mojo_interface_provider.js'

/**
 * @fileoverview
 * 'input-list' is responsible for displaying keyboard, touchpad, and
 * touchscreen cards.
 */
Polymer({
  is: 'input-list',

  _template: html`{__html_template__}`,

  /** @private {?InputDataProviderInterface} */
  inputDataProvider_: null,

  properties: {
    /** @private {!Array<!KeyboardInfo>} */
    keyboards_: {
      type: Array,
      value: () => [],
    },

    /** @private {!Array<!TouchDeviceInfo>} */
    touchpads_: {
      type: Array,
      value: () => [],
    },

    /** @private {!Array<!TouchDeviceInfo>} */
    touchscreens_: {
      type: Array,
      value: () => [],
    },
  },

  /** @override */
  created() {
    this.inputDataProvider_ = getInputDataProvider();
    this.loadInitialDevices_();
  },

  /** @private */
  loadInitialDevices_() {
    this.inputDataProvider_.getConnectedDevices().then((devices) => {
      this.keyboards_ = devices.keyboards;
      this.touchpads_ = devices.touchDevices.filter(
          (device) => device.type === TouchDeviceType.kPointer);
      this.touchscreens_ = devices.touchDevices.filter(
          (device) => device.type === TouchDeviceType.kDirect);
    });
  },
});
