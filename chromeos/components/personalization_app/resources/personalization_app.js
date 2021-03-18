// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview the main entry point for the Wallpaper app.
 */

import '/strings.m.js';

// TODO(b/182012641) move wallpaper functionality here from chrome://os-settings
function testing() {
  console.log('Personalization App Loaded');
}

document.addEventListener('DOMContentLoaded', testing);
