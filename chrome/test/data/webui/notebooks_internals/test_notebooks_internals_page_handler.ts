// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FeatureFlagState, PageHandlerInterface, ProfileEligibility} from 'chrome://notebooks-internals/notebooks_internals.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export const TEST_NOTEBOOK_HOME_URL = 'https://example.com';

export class TestNotebooksInternalsPageHandler extends TestBrowserProxy
    implements PageHandlerInterface {
  private featureFlagState: FeatureFlagState = {
    notebooksFeatureEnabled: true,
    notebookHomeUrl: TEST_NOTEBOOK_HOME_URL,
  };

  private profileEligibility: ProfileEligibility = {
    userEligible: true,
  };

  constructor() {
    super([
      'getFeatureFlagState',
      'getProfileEligibility',
    ]);
  }

  setFeatureFlagState(flags: FeatureFlagState) {
    this.featureFlagState = flags;
  }

  setProfileEligibility(eligibility: ProfileEligibility) {
    this.profileEligibility = eligibility;
  }

  getFeatureFlagState(): Promise<{flags: FeatureFlagState}> {
    this.methodCalled('getFeatureFlagState');
    return Promise.resolve({flags: this.featureFlagState});
  }

  getProfileEligibility(): Promise<{eligibility: ProfileEligibility}> {
    this.methodCalled('getProfileEligibility');
    return Promise.resolve({eligibility: this.profileEligibility});
  }
}
