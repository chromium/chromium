// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {routes} from 'chrome://settings/settings.js';

// Set the URL to be that of specific route to load upon injecting
// settings-ui. Simply calling
// settings.Router.getInstance().navigateTo(route) prevents use of mock APIs
// for fake data.
window.history.pushState('object or string', 'Test', routes.ABOUT.path);
const settingsUi = document.createElement('settings-ui');
document.body.appendChild(settingsUi);
document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
