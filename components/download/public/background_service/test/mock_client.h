// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_MOCK_CLIENT_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_MOCK_CLIENT_H_

#include "base/macros.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/download_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace download {
namespace test {

class MockClient : public Client {
 public:
  MockClient();
  ~MockClient() override;

  // Client implementation.
  MOCK_METHOD2(OnServiceInitialized,
               void(bool, const std::vector<DownloadMetaData>&));
  MOCK_METHOD0(OnServiceUnavailable, void());
  MOCK_METHOD3(OnDownloadStarted,
               void(const std::string&,
                    const std::vector<GURL>&,
                    const scoped_refptr<const net::HttpResponseHeaders>&));
  MOCK_METHOD3(OnDownloadUpdated, void(const std::string&, uint64_t, uint64_t));
  MOCK_METHOD3(OnDownloadFailed,
               void(const std::string&, const CompletionInfo&, FailureReason));
  MOCK_METHOD2(OnDownloadSucceeded,
               void(const std::string&, const CompletionInfo&));
  MOCK_METHOD2(CanServiceRemoveDownloadedFile, bool(const std::string&, bool));
  void GetUploadData(const std::string& guid,
                     GetUploadDataCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClient);
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_MOCK_CLIENT_H_
