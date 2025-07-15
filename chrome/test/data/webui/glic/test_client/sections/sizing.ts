// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getBrowser} from '../client.js';
import {$} from '../page_element_types.js';

$.enableDragResizeCheckbox.addEventListener('change', () => {
  getBrowser()!.enableDragResize!($.enableDragResizeCheckbox.checked);
});
