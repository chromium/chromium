// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {acceleratorEditDialogTest} from './accelerator_edit_dialog_test.js';
import {acceleratorEditViewTest} from './accelerator_edit_view_test.js';
import {acceleratorRowTest} from './accelerator_row_test.js';
import {acceleratorViewTest} from './accelerator_view_test.js';
import {fakeShortcutProviderTest} from './fake_shortcut_provider_test.js';
import {shortcutCustomizationAppTest} from './shortcut_customization_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('ShortcutCustomizationApp', shortcutCustomizationAppTest);
runSuite('AcceleratorEditViewTest', acceleratorEditViewTest);
runSuite('AcceleratorViewTest', acceleratorViewTest);
runSuite('AcceleratorRowTest', acceleratorRowTest);
// TODO(jimmyxgong): Any test that runs after AcceleratorEditDialogTest
// will fail right now. When fixed, alphabetize this list.
runSuite('FakeShortcutProviderTest', fakeShortcutProviderTest);
runSuite('AcceleratorEditDialogTest', acceleratorEditDialogTest);
