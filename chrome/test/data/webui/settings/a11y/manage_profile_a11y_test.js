// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Accessibility tests for the MANAGE_PROFILE route.
 * Non-Chrome OS only.
 */

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, routes} from 'chrome://settings/settings.js';

const settingsUi = document.createElement('settings-ui');
document.body.appendChild(settingsUi);
Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
flush();
document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
