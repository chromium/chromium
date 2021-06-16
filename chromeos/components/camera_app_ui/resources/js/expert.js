// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nString} from './i18n_string.js';
import * as localStorage from './models/local_storage.js';
import {DeviceOperator} from './mojo/device_operator.js';
import * as state from './state.js';
import * as toast from './toast.js';

/**
 * Enables or disables expert mode.
 * @param {boolean} enable Whether to enable or disable expert mode
 * @return {!Promise}
 */
export async function setExpertMode(enable) {
  if (!await DeviceOperator.isSupported()) {
    toast.show(I18nString.ERROR_MSG_EXPERT_MODE_NOT_SUPPORTED);
    return;
  }
  state.set(state.State.EXPERT, enable);
  localStorage.set('expert', enable);
}

/**
 * Toggles expert mode.
 * @return {!Promise}
 */
export async function toggleExpertMode() {
  const newState = !state.get(state.State.EXPERT);
  await setExpertMode(newState);
}
