// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of
 * chromeos.settings.mojom.UserActionRecorderRemote for testing.
 */

cr.define('settings', function() {
  /**
   * Fake implementation of chromeos.settings.mojom.UserActionRecorderRemote.
   *
   * @implements {chromeos.settings.mojom.UserActionRecorderInterface}
   */
  /* #export */ class FakeUserActionRecorder {
    constructor() {
      this.pageFocusCount = 0;
      this.pageBlurCount = 0;
      this.clickCount = 0;
      this.navigationCount = 0;
      this.searchCount = 0;
      this.settingChangeCount = 0;
    }

    recordPageFocus() {
      ++this.pageFocusCount;
    }

    recordPageBlur() {
      ++this.pageBlurCount;
    }

    recordClick() {
      ++this.clickCount;
    }

    recordNavigation() {
      ++this.navigationCount;
    }

    recordSearch() {
      ++this.searchCount;
    }

    recordSettingChange() {
      ++this.settingChangeCount;
    }

    recordSettingChangeWithDetails() {
      ++this.settingChangeCount;
    }
  }

  // #cr_define_end
  return {FakeUserActionRecorder: FakeUserActionRecorder};
});
