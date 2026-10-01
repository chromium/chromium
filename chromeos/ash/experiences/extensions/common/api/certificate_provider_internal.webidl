// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[ExternalExtensionType="certificateProvider.CertificateInfo"]
typedef object CertificateProviderCertificateInfo;

// Internal API backing the chrome.certificateProvider API events.
// The internal API associates events with replies to these events using request
// IDs. A custom binding is used to hide these IDs from the public API.
// Before an event hits the extension, the request ID is removed and instead a
// callback is added to the event arguments. On the way back, when the extension
// runs the callback to report its results, the callback magically prepends the
// request ID to the results and calls the respective internal report function
// (reportSignature or reportCertificates).
[implemented_in = "chrome/browser/extensions/api/certificate_provider/certificate_provider_api.h"]
interface CertificateProviderInternal {
  // Matches certificateProvider.SignCallback. Must be called without the
  // signature to report an error.
  static Promise<undefined> reportSignature(
      long requestId,
      optional ArrayBuffer signature);

  // Matches certificateProvider.CertificatesCallback. Must be called without
  // the certificates argument to report an error.
  // |PromiseValue|: rejectedCertificates
  static Promise<sequence<ArrayBuffer>> reportCertificates(
      long requestId,
      optional sequence<CertificateProviderCertificateInfo> certificates);
};

partial interface Browser {
  static attribute CertificateProviderInternal certificateProviderInternal;
};
