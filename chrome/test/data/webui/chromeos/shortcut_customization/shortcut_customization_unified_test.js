// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {acceleratorEditDialogTest} from './accelerator_edit_dialog_test.js';
import {acceleratorEditViewTest} from './accelerator_edit_view_test.js';
import {acceleratorLookupManagerTest} from './accelerator_lookup_manager_test.js';
import {acceleratorRowTest} from './accelerator_row_test.js';
import {acceleratorSubsectionTest} from './accelerator_subsection_test.js';
import {acceleratorViewTest} from './accelerator_view_test.js';
import {fakeShortcutProviderTest} from './fake_shortcut_provider_test.js';
import {shortcutCustomizationAppTest} from './shortcut_customization_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('AcceleratorEditViewTest', acceleratorEditViewTest);
runSuite('AcceleratorLookupManagerTest', acceleratorLookupManagerTest);
runSuite('AcceleratorViewTest', acceleratorViewTest);
runSuite('AcceleratorRowTest', acceleratorRowTest);
runSuite('AcceleratorEditDialogTest', acceleratorEditDialogTest);
runSuite('AcceleratorSubsectionTest', acceleratorSubsectionTest);
runSuite('FakeShortcutProviderTest', fakeShortcutProviderTest);
runSuite('ShortcutCustomizationApp', shortcutCustomizationAppTest);
