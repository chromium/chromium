// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/certificate_store.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/mock_private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "net/cert/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

using InitStatus = leveldb_proto::Enums::InitStatus;

class CertificateStoreTest : public testing::Test {
 protected:
  using ProtoMap =
      std::map<std::string, client_certificates_pb::ClientIdentity>;

  CertificateStoreTest() {
    auto db = std::make_unique<
        leveldb_proto::test::FakeDB<client_certificates_pb::ClientIdentity>>(
        &database_entries_);
    fake_db_ = db.get();

    auto mock_key_factory =
        std::make_unique<testing::StrictMock<MockPrivateKeyFactory>>();
    mock_key_factory_ = mock_key_factory.get();

    store_ = CertificateStore::CreateForTesting(std::move(db),
                                                std::move(mock_key_factory));
  }

  std::unique_ptr<CertificateStore> store_;

  ProtoMap database_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<client_certificates_pb::ClientIdentity>>
      fake_db_;
  raw_ptr<MockPrivateKeyFactory> mock_key_factory_;
};

// Tests that creating the store also starts the initialization of the database.
TEST_F(CertificateStoreTest, Create_InitializesDatabase_Success) {
  fake_db_->InitStatusCallback(InitStatus::kOK);
}

}  // namespace client_certificates
