// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://cloud-upload. Tests the entire page
 * instead of individual components.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

var CloudUploadAppBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://cloud-upload/test_loader.html?module=chromeos/' +
        'cloud_upload/cloud_upload_app_test.js';
  }

  get featureList() {
    return {enabled: ['ash::features::kUploadOfficeToCloud']};
  }
};

TEST_F('CloudUploadAppBrowserTest', 'All', () => mocha.run());
