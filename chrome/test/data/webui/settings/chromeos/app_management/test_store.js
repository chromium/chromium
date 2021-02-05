// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestStore} from 'chrome://test/test_store.m.js';
// #import {AppManagementStore, createEmptyState, reduceAction} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

cr.define('app_management', function() {
  /* #export */ class TestAppManagementStore extends cr.ui.TestStore {
    constructor(data) {
      super(
          data, app_management.AppManagementStore,
          app_management.util.createEmptyState(), app_management.reduceAction);
    }
  }

  // #cr_define_end
  return {
    TestAppManagementStore: TestAppManagementStore,
  };
});
