// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NavigationView} from 'chrome://diagnostics/diagnostics_types.js';
import {getNavigationViewForPageId} from 'chrome://diagnostics/diagnostics_utils.js';

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/chromeos/test_browser_proxy.js';
import { DiagnosticsBrowserProxy} from 'chrome://diagnostics/diagnostics_browser_proxy.js';

/** Test version of DiagnosticsBrowserProxy. */
export class TestDiagnosticsBrowserProxy extends TestBrowserProxy implements DiagnosticsBrowserProxy {
  private success = false;
  previousView: NavigationView|null = null;
  constructor() {
    super([
      'initialize',
      'recordNavigation',
      'saveSessionLog',
      'getPluralString',
    ]);
  }

  initialize(): void {
    this.methodCalled('initialize');
  }

  recordNavigation(currentView: string): void {
    this.methodCalled(
        'recordNavigation',
        [this.previousView, getNavigationViewForPageId(currentView)]);
  }

  saveSessionLog(): Promise<boolean> {
    this.methodCalled('saveSessionLog');
    return Promise.resolve(this.success);
  }

  setPreviousView(view: NavigationView): void {
    this.previousView = view;
  }

  setSuccess(success: boolean): void {
    this.success = success;
  }

  getPluralString(name: string, count: number): Promise<string> {
    assertEquals('nameServersText', name);
    return Promise.resolve(`Name Server${count !== 1 ? 's' : ''}`);
  }
}
