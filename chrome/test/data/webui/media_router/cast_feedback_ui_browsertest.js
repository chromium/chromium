// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Code written into generated C++ code.
GEN('#include "content/public/test/browser_test.h"');

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

var CastFeedbackUITest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://cast-feedback/test_loader.html?' +
        'module=media_router/cast_feedback_ui_test.js';
  }
};

TEST_F('CastFeedbackUITest', 'Success', function() {
  runMochaTest('Suite', 'Success');
});

TEST_F('CastFeedbackUITest', 'Failure', function() {
  runMochaTest('Suite', 'Failure');
});
