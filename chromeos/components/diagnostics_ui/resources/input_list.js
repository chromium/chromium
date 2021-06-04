// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ConnectedDevicesObserverReceiver, ConnectedDevicesObserverInterface, InputDataProviderInterface, KeyboardInfo, TouchDeviceInfo, TouchDeviceType} from './diagnostics_types.js'
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

  /** @private {?ConnectedDevicesObserverReceiver} */
  connectedDevicesObserverReceiver_: null,

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
    this.observeConnectedDevices_();
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

  /** @private */
  observeConnectedDevices_() {
    this.connectedDevicesObserverReceiver_ =
      new ConnectedDevicesObserverReceiver(
        /** @type {!ConnectedDevicesObserverInterface} */(this));
    this.inputDataProvider_.observeConnectedDevices(
      this.connectedDevicesObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardConnected.
   * @param {!KeyboardInfo} newKeyboard
   */
  onKeyboardConnected(newKeyboard) {
    this.push('keyboards_', newKeyboard);
  },

  /**
   * Removes the device with the given evdev ID from one of the device list
   * properties.
   * @param {(string | !Array.<(string | number)>)} path the property's path
   * @param {number} id
   * @private
   */
  removeDeviceById_(path, id) {
    const index = this.get(path).findIndex((device) => device.id === id);
    if (index !== -1) {
      this.splice(path, index, 1);
    }
  },

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardDisconnected.
   * @param {number} id
   */
  onKeyboardDisconnected(id) {
    this.removeDeviceById_('keyboards_', id);
  },

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceConnected.
   * @param {!TouchDeviceInfo} newTouchDevice
   */
  onTouchDeviceConnected(newTouchDevice) {
    if (newTouchDevice.type === TouchDeviceType.kPointer) {
      this.push('touchpads_', newTouchDevice);
    } else {
      this.push('touchscreens_', newTouchDevice);
    }
  },

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceDisconnected.
   * @param {number} id
   */
  onTouchDeviceDisconnected(id) {
    this.removeDeviceById_('touchpads_', id);
    this.removeDeviceById_('touchscreens_', id);
  },
});
