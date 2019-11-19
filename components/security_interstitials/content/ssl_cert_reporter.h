// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_CERT_REPORTER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_CERT_REPORTER_H_

#include <string>

// An interface used by interstitial pages to send reports of invalid
// certificate chains.
class SSLCertReporter {
 public:
  virtual ~SSLCertReporter() {}

  // Sends a serialized certificate report to the report collection
  // endpoint.
  virtual void ReportInvalidCertificateChain(
      const std::string& serialized_report) = 0;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_CERT_REPORTER_H_
