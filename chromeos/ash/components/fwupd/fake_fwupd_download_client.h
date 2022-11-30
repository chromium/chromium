// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FWUPD_FAKE_FWUPD_DOWNLOAD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_FWUPD_FAKE_FWUPD_DOWNLOAD_CLIENT_H_

#include "ash/public/cpp/fwupd_download_client.h"
#include "base/component_export.h"
#include "services/network/test/test_url_loader_factory.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// A fake implementation of FwupdDownloadClient used for testing.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_FWUPD) FakeFwupdDownloadClient
    : public ash::FwupdDownloadClient {
 public:
  FakeFwupdDownloadClient();
  FakeFwupdDownloadClient(const FwupdDownloadClient&) = delete;
  FakeFwupdDownloadClient& operator=(const FwupdDownloadClient&) = delete;
  ~FakeFwupdDownloadClient() override;

  // ash::FwupdDownloadClient:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

  network::TestURLLoaderFactory& test_url_loader_factory();

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_FWUPD_FAKE_FWUPD_DOWNLOAD_CLIENT_H_
