// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionResult, CertificateManagerPageHandlerInterface, CertificateManagerPageRemote, CertificateSource, CertManagementMetadata, SummaryCertInfo} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificateManagerPageCallbackRouter} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class FakePageHandler extends TestBrowserProxy implements
    CertificateManagerPageHandlerInterface {
  // TODO(hchao): try to mess with structure so the auto formatter makes this
  // less confusing. See:
  // https://crrev.com/c/5577174/8/chrome/test/data/webui/cr_components/certificate_manager_v2_test_support.ts
  private getCertificatesCallback_: (source: CertificateSource) => {
    certs:
      SummaryCertInfo[],
  } = (_) => {
    return {certs: []};
  };

  private importCertificateCallback_: (source: CertificateSource) => {
    result:
      ActionResult,
  } = (_) => {
    return {result: {error: 'default implementation called'}};
  };

  private importAndBindCertificateCallback_: (source: CertificateSource) => {
    result:
      ActionResult,
  } = (_) => {
    return {result: {error: 'default implementation called'}};
  };

  private deleteCertificateCallback_:
      (source: CertificateSource, sha256hashHex: string) => {
        result:
          ActionResult,
      } = (_) => {
        return {result: {error: 'default implementation called'}};
      };

  private metadata_: CertManagementMetadata = {
    includeSystemTrustStore: true,
    numUserAddedSystemCerts: 0,
    // <if expr="not is_chromeos">
    isIncludeSystemTrustStoreManaged: false,
    // </if>
    numPolicyCerts: 0,
  };

  constructor() {
    super([
      'getCertificates',
      'getCertManagementMetadata',
      'viewCertificate',
      'exportCertificates',
      'importCertificate',
      'importAndBindCertificate',
      'deleteCertificate',
      'showNativeManageCertificates',
      'setIncludeSystemTrustStore',
    ]);
  }

  getCertificates(source: CertificateSource):
      Promise<{certs: SummaryCertInfo[]}> {
    this.methodCalled('getCertificates', source);
    return Promise.resolve(this.getCertificatesCallback_(source));
  }

  getCertManagementMetadata(): Promise<{metadata: CertManagementMetadata}> {
    this.methodCalled('getCertManagementMetadata');
    return Promise.resolve({metadata: this.metadata_});
  }

  viewCertificate(source: CertificateSource, sha256hashHex: string) {
    this.methodCalled('viewCertificate', source, sha256hashHex);
  }

  exportCertificates(source: CertificateSource) {
    this.methodCalled('exportCertificates', source);
  }

  importCertificate(source: CertificateSource) {
    this.methodCalled('importCertificate', source);
    return Promise.resolve(this.importCertificateCallback_(source));
  }

  importAndBindCertificate(source: CertificateSource) {
    this.methodCalled('importAndBindCertificate', source);
    return Promise.resolve(this.importAndBindCertificateCallback_(source));
  }

  deleteCertificate(source: CertificateSource, sha256hashHex: string) {
    this.methodCalled('deleteCertificate', source, sha256hashHex);
    return Promise.resolve(
        this.deleteCertificateCallback_(source, sha256hashHex));
  }

  setCertificatesCallback(callbackFn: (source: CertificateSource) => {
    certs: SummaryCertInfo[],
  }) {
    this.getCertificatesCallback_ = callbackFn;
  }

  setDeleteCertificateCallback(
      callbackFn: (source: CertificateSource, sha256hashHex: string) => {
        result: ActionResult,
      }) {
    this.deleteCertificateCallback_ = callbackFn;
  }

  setCertManagementMetadata(metadata: CertManagementMetadata) {
    this.metadata_ = metadata;
  }

  // <if expr="is_win or is_macosx">
  showNativeManageCertificates() {
    this.methodCalled('showNativeManageCertificates');
  }
  // </if>

  // <if expr="not is_chromeos">
  setIncludeSystemTrustStore(include: boolean) {
    this.methodCalled('setIncludeSystemTrustStore', include);
  }
  // </if>
}

export class TestCertificateManagerProxy {
  callbackRouter: CertificateManagerPageCallbackRouter;
  callbackRouterRemote: CertificateManagerPageRemote;
  handler: FakePageHandler;

  constructor() {
    this.callbackRouter = new CertificateManagerPageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.handler = new FakePageHandler();
  }
}
