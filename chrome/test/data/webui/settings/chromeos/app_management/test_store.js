// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  class TestStore extends cr.ui.TestStore {
    constructor(data) {
      super(
          data, app_management.Store, app_management.util.createEmptyState(),
          app_management.reduceAction);
    }
  }

  return {
    TestStore: TestStore,
  };
});
