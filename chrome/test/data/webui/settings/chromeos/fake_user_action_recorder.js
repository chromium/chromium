// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of
 * ash.settings.mojom.UserActionRecorderRemote for testing.
 */

/**
 * Fake implementation of ash.settings.mojom.UserActionRecorderRemote.
 *
 * @implements {ash.settings.mojom.UserActionRecorderInterface}
 */
export class FakeUserActionRecorder {
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
