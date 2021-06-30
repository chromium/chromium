// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview imports javascript files that have side effects. Makes sure
 * they are imported in the correct order.
 */

// Import polymer before common/styles.js because of an implicit dependency.
import 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';
import '../common/styles.js';
// Import load_time_data.js first because strings.js requires
// window.load_time_data to be initialized.
import '/load_time_data.js';
import '/strings.js';
