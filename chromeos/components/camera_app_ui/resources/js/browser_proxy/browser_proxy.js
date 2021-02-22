// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as localStorage from '../models/local_storage.js';

// TODO(b/172343409): Remove this file once the usage in tests are updated.

/**
 * @return {!Promise}
 */
export async function localStorageClear() {
  return localStorage.clear();
}

export const browserProxy = {localStorageClear};
