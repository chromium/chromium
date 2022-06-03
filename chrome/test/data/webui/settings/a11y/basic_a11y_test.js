// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const ui = document.createElement('settings-ui');
document.body.appendChild(ui);
flush();
document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
