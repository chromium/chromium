// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://eche-app.
 */

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://eche-app';

var EcheAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kEcheSWA',
        'chromeos::features::kPhoneHubRecentApps'
      ]
    };
  }
};

// TODO(samchiu) Temporarily disable browser_test since a future Eche roll
// will break the function of eche window. Test will be enable when we
// phase in http://go/crrev/c/3081307.
