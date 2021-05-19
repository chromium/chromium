// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConnectionType, GetConnectedDevicesResponse, InputDataProviderInterface, KeyboardInfo, TouchDeviceInfo, TouchDeviceType} from './diagnostics_types.js';
import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

/**
 * @fileoverview
 * Implements a fake version of the InputDataProvider Mojo interface.
 */

/** @implements {InputDataProviderInterface} */
export class FakeInputDataProvider {
  constructor() {
    this.methods_ = new FakeMethodResolver();

    this.registerMethods();
  }

  /**
   * Setup method resolvers.
   */
  registerMethods() {
    this.methods_.register('getConnectedDevices');
  }

  /**
   * @return {!Promise<!GetConnectedDevicesResponse>}
   */
  getConnectedDevices() {
    return this.methods_.resolveMethod('getConnectedDevices');
  }

  /**
   * Sets the values that will be returned when calling getConnectedDevices().
   * @param {!Array<!KeyboardInfo>} keyboards
   * @param {!Array<!TouchDeviceInfo>} touchDevices
   */
  setFakeConnectedDevices(keyboards, touchDevices) {
    this.methods_.setResult('getConnectedDevices',
                            {keyboards: keyboards, touchDevices: touchDevices});
  }
}
