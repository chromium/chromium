// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal version of load_time_data.js for chrome-untrusted://
 * origins. They are sandboxed, so cannot use chrome://resources ("unable to
 * load local resource") and we don't want to maintain a "mirror" of all the
 * module dependencies on each chrome-untrusted:// origin. For simplicity, this
 * version lacks all the validation done by load_time_data.js, and just aims to
 * provide a compatible API.
 */

const impl = {
  data: {},  // Set by strings.js.
  getValue: (id) => impl.data[id],
  getString: (id) => /** @type{string} */ (impl.data[id]),
  getBoolean: (id) => /** @type{boolean} */ (impl.data[id]),
  getInteger: (id) => /** @type{number} */ (impl.data[id]),
  valueExists: (id) => impl.data[id] !== undefined
};
window['loadTimeData'] = impl;
