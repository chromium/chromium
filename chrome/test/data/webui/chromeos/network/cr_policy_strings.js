// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Sets up strings used by policy indicator elements. */
if (!window.CrPolicyStrings) {
  // Necessary for tests residing within a JS module.
  window.CrPolicyStrings = {
    controlledSettingPolicy: 'policy',
    controlledSettingRecommendedMatches: 'matches',
    controlledSettingRecommendedDiffers: 'differs',
    controlledSettingShared: 'shared: $1',
    controlledSettingWithOwner: 'owner: $1',
    controlledSettingNoOwner: 'owner',
    controlledSettingExtension: 'extension: $1',
    controlledSettingExtensionWithoutName: 'extension',
    controlledSettingParent: 'parent',
    controlledSettingChildRestriction: 'Restricted for child',
  };
}
