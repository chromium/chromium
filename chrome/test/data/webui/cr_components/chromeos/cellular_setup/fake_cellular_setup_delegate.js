// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Remove the class declaration once migrate the test to TS.
class CellularSetupDelegate {
  /**
   * @return {boolean} true if base page title text should be visible.
   */
  shouldShowPageTitle() {}

  /**
   * @return {boolean} true if cancel button should be visible.
   */
  shouldShowCancelButton() {}
}

/** @implements {CellularSetupDelegate} */
export class FakeCellularSetupDelegate {
  /** @override */
  shouldShowPageTitle() {
    return false;
  }

  /** @override */
  shouldShowCancelButton() {
    return true;
  }
}
