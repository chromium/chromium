// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_KEY_UPLOAD_CLIENT_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_KEY_UPLOAD_CLIENT_H_

#include "base/functional/callback.h"
#include "components/enterprise/client_certificates/core/key_upload_client.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "net/cert/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace client_certificates {

class MockKeyUploadClient : public KeyUploadClient {
 public:
  MockKeyUploadClient();
  ~MockKeyUploadClient() override;

  MOCK_METHOD(void,
              CreateCertificate,
              (scoped_refptr<PrivateKey>, CreateCertificateCallback),
              (override));
  MOCK_METHOD(void,
              SyncKey,
              (scoped_refptr<PrivateKey>, SyncKeyCallback),
              (override));
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_KEY_UPLOAD_CLIENT_H_
