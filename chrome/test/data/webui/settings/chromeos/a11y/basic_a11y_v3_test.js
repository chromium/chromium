// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const ui = document.createElement('os-settings-ui');
document.body.appendChild(ui);
flush();
document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
