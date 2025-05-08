// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONTEXT_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONTEXT_DELEGATE_H_

#include "base/memory/scoped_refptr.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace client_certificates {

// Delegate used to execute logic within a specific scope (e.g. a whole Browser,
// a specific Profile).
class ContextDelegate {
 public:
  virtual ~ContextDelegate() = default;

  // Will notify the rest of the current context that `certificate` should no
  // longer be used and be considered as deleted.
  virtual void OnClientCertificateDeleted(
      scoped_refptr<net::X509Certificate> certificate) = 0;

  // Returns a identity name for the current context.
  virtual std::string GetIdentityName() = 0;

  // Returns a temporary identity name for the current context.
  virtual std::string GetTemporaryIdentityName() = 0;

  // Returns the pref associated with a policy for the current context.
  virtual std::string GetPolicyPref() = 0;

  // Returns the logging context used by histogram variants.
  virtual std::string GetLoggingContext() = 0;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CONTEXT_DELEGATE_H_
