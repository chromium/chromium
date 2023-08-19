// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataAccessPolicyState, PeripheralDataAccessBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

const CROS_SETTING_PREF_NAME = 'cros.device.peripheral_data_access_enabled';

export class TestPeripheralDataAccessBrowserProxy extends TestBrowserProxy
    implements PeripheralDataAccessBrowserProxy {
  private policyState: DataAccessPolicyState;
  constructor() {
    super([
      'isThunderboltSupported',
      'getPolicyState',
    ]);

    this.policyState = {
      prefName: CROS_SETTING_PREF_NAME,
      isUserConfigurable: false,
    };
  }

  isThunderboltSupported(): Promise<boolean> {
    this.methodCalled('isThunderboltSupported');
    return Promise.resolve(/*supported=*/ true);
  }

  getPolicyState(): Promise<DataAccessPolicyState> {
    this.methodCalled('getPolicyState');
    return Promise.resolve(this.policyState);
  }

  setPolicyState(prefName: string, isUserConfigurable: boolean): void {
    this.policyState.prefName = prefName;
    this.policyState.isUserConfigurable = isUserConfigurable;
  }
}
