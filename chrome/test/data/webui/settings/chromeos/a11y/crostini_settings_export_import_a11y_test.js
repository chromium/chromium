// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the CROSTINI_EXPORT_IMPORT
 * route.
 * Chrome OS only.
 */

import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Router.getInstance().navigateTo(routes.CROSTINI_EXPORT_IMPORT);
flush();
document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
