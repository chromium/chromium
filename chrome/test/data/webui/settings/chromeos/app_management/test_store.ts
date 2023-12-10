// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppManagementActions, AppManagementPageState, AppManagementStore, reduceAction} from 'chrome://os-settings/os_settings.js';
import {createEmptyState} from 'chrome://resources/cr_components/app_management/util.js';
import {TestStore} from 'chrome://webui-test/test_store.js';

export class TestAppManagementStore extends
    TestStore<AppManagementPageState, AppManagementActions> {
  constructor(data: Partial<AppManagementPageState>) {
    super(data, createEmptyState(), reduceAction);
  }

  /**
   * Replaces the global store instance with this TestStore. Overrides the
   * default implementation by using the setInstance() static method
   */
  replaceSingleton() {
    AppManagementStore.setInstanceForTesting(this);
  }
}
