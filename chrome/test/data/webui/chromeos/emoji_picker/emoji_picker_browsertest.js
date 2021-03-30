// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests for the <emoji-picker> Polymer component.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

// This file bootstraps the other tests written in Javascript.
class EmojiPickerBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kImeSystemEmojiPicker']};
  }
}

// Tests behaviour of <emoji-picker> component.
var EmojiPickerMainTest = class extends EmojiPickerBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_test.js';
  }
};

TEST_F('EmojiPickerMainTest', 'All', function() {
  mocha.run();
});

// Tests functionality of recently used storage.
var EmojiPickerStoreTest = class extends EmojiPickerBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_store_test.js';
  }
};

// TODO(https://crbug.com/1179762): Re-enable once flakiness is fixed.
TEST_F('EmojiPickerStoreTest', 'DISABLED_All', function() {
  mocha.run();
});
