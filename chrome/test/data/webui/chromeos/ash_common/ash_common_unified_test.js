// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeMethodResolverTestSuite} from './fake_method_resolver_test.js';
import {fakeObservablesTestSuite} from './fake_observables_test.js';
import {keyboardDiagramTestSuite} from './keyboard_diagram_test.js';
import {navigationSelectorTestSuite} from './navigation_selector_test.js';
import {navigationViewPanelTestSuite} from './navigation_view_panel_test.js';
import {pageToolbarTestSuite} from './page_toolbar_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('FakeObservables', fakeObservablesTestSuite);
runSuite('FakeMethodResolver', fakeMethodResolverTestSuite);
runSuite('KeyboardDiagram', keyboardDiagramTestSuite);
runSuite('NavigationSelector', navigationSelectorTestSuite);
runSuite('NavigationViewPanel', navigationViewPanelTestSuite);
runSuite('PageToolbar', pageToolbarTestSuite);
