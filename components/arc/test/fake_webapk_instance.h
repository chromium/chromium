// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_WEBAPK_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_WEBAPK_INSTANCE_H_

#include "components/arc/mojom/webapk.mojom.h"

namespace arc {

class FakeWebApkInstance : public mojom::WebApkInstance {
 public:
  FakeWebApkInstance();
  FakeWebApkInstance(const FakeWebApkInstance&) = delete;
  FakeWebApkInstance& operator=(const FakeWebApkInstance&) = delete;

  ~FakeWebApkInstance() override;

  // mojom::WebApkInstance overrides:
  void InstallWebApk(const std::string& package_name,
                     uint32_t version,
                     const std::string& app_name,
                     const std::string& token,
                     InstallWebApkCallback callback) override;

  const std::vector<std::string>& handled_packages() {
    return handled_packages_;
  }

  void set_install_result(arc::mojom::WebApkInstallResult result) {
    install_result_ = result;
  }

 private:
  std::vector<std::string> handled_packages_;

  arc::mojom::WebApkInstallResult install_result_ =
      arc::mojom::WebApkInstallResult::kSuccess;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_WEBAPK_INSTANCE_H_
