// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebClientMode} from '/glic/glic_api/glic_api.js';

import {getBrowser, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

$.textmode.addEventListener('change', () => {
  logMessage(
      `Setting interaction mode to: ${WebClientMode[WebClientMode.TEXT]}`);
  getBrowser()!.onModeChange!(WebClientMode.TEXT);
});

$.audiomode.addEventListener('change', () => {
  logMessage(
      `Setting interaction mode to: ${WebClientMode[WebClientMode.AUDIO]}`);
  getBrowser()!.detachPanel!();
  getBrowser()!.onModeChange!(WebClientMode.AUDIO);
});
