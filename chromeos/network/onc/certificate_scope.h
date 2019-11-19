// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_CERTIFICATE_SCOPE_H_
#define CHROMEOS_NETWORK_ONC_CERTIFICATE_SCOPE_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"

namespace base {
class Value;
}

namespace chromeos {
namespace onc {

// Describes the scope a policy-provided certificate should be applied in.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CertificateScope {
 public:
  CertificateScope(const CertificateScope& other);
  CertificateScope(CertificateScope&& other);
  ~CertificateScope();

  // Creates a CertificateScope for a chrome extension with the id
  // |extension_id|.
  static CertificateScope ForExtension(const std::string& extension_id);

  // Creates a CertificateScope for certificates that should apply in the
  // default scope.
  // For Chrome OS user ONC policy, this means that they apply in the whole user
  // Profile.
  // For Chrome OS device ONC policy, this means that they apply in the context
  // of the sign-in webview and all sign-in screen extensions (however, only
  // certificates without trust are respected as default-scoped device ONC
  // policy specified certificates).
  static CertificateScope Default();

  // Parses a CertificateScope from |scope_dict|, which should be a dictionary
  // containing the ONC "Scope" object.
  static base::Optional<CertificateScope> ParseFromOncValue(
      const base::Value& scope_dict);

  CertificateScope& operator=(const CertificateScope& other);
  bool operator<(const CertificateScope& other) const;
  bool operator==(const CertificateScope& other) const;
  bool operator!=(const CertificateScope& other) const;

  bool is_extension_scoped() const { return !extension_id_.empty(); }
  const std::string& extension_id() const { return extension_id_; }

 private:
  // If |extension_id| is empty, it means that the scope should not be
  // restricted.
  CertificateScope(const std::string& extension_id);

  // If empty, it means that the scope should not be restricted to an extension.
  std::string extension_id_;
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_CERTIFICATE_SCOPE_H_
