// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CertificateProvisioningBrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A test version of CertificateProvisioningBrowserProxy.
 * Provides helper methods for allowing tests to know when a method was called,
 * as well as specifying mock responses.
 */
export class TestCertificateProvisioningBrowserProxy extends TestBrowserProxy
    implements CertificateProvisioningBrowserProxy {
  constructor() {
    super([
      'refreshCertificateProvisioningProcesses',
      'triggerCertificateProvisioningProcessUpdate',
      'triggerCertificateProvisioningProcessReset',
    ]);
  }

  refreshCertificateProvisioningProcesses() {
    this.methodCalled('refreshCertificateProvisioningProcesses');
  }

  triggerCertificateProvisioningProcessUpdate(certProfileId: string) {
    this.methodCalled(
        'triggerCertificateProvisioningProcessUpdate', certProfileId);
  }
  triggerCertificateProvisioningProcessReset(certProfileId: string) {
    this.methodCalled(
        'triggerCertificateProvisioningProcessReset', certProfileId);
  }
}
