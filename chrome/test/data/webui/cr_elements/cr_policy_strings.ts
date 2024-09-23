// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrPolicyStringsType} from 'chrome://resources/cr_elements/policy/cr_policy_types.js';

/** @fileoverview Sets up strings used by policy indicator elements. */
export const CrPolicyStrings: CrPolicyStringsType = {
  controlledSettingExtension: 'extension: $1',
  controlledSettingExtensionWithoutName: 'extension',
  controlledSettingPolicy: 'policy',
  controlledSettingRecommendedMatches: 'matches',
  controlledSettingRecommendedDiffers: 'differs',
  controlledSettingParent: 'parent',
  controlledSettingChildRestriction: 'Restricted for child',

  // <if expr="chromeos_ash">
  controlledSettingShared: 'shared: $1',
  controlledSettingWithOwner: 'owner: $1',
  controlledSettingNoOwner: 'owner',
  // </if>
};

// Necessary for tests residing within a JS module.
Object.assign(window, {CrPolicyStrings});
