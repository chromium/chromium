// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/attestation/attestation.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Keeps arguments for the last AsyncCallStatusWithData().
class TestObserver : public CryptohomeClient::Observer {
 public:
  TestObserver() = default;

  bool return_status() const { return return_status_; }
  const std::string& data() const { return data_; }

  void AsyncCallStatusWithData(int async_id,
                               bool return_status,
                               const std::string& data) override {
    return_status_ = return_status;
    data_ = data;
  }

 private:
  bool return_status_ = false;
  std::string data_;
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class FakeCryptohomeClientTest : public ::testing::Test {
 public:
  FakeCryptohomeClientTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeCryptohomeClient fake_cryptohome_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeCryptohomeClientTest);
};

}  // namespace chromeos
