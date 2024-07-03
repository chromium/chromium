// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/key_rotation/test_utils.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_rotation/iwa_key_rotation_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_rotation/proto/key_rotation.pb.h"

namespace web_app::test {

namespace {

class ComponentUpdateWaiter : public IwaKeyRotationInfoProvider::Observer {
 public:
  using UpdateCallback = base::OnceCallback<void(
      base::expected<void, IwaKeyRotationInfoProvider::ComponentUpdateError>)>;

  explicit ComponentUpdateWaiter(UpdateCallback on_update)
      : on_update_(std::move(on_update)) {
    obs_.Observe(IwaKeyRotationInfoProvider::GetInstance());
  }

  // IwaKeyRotationInfoProvider::Observer:
  void OnComponentUpdateSuccess(const base::Version& version) override {
    std::move(on_update_).Run(base::ok());
    obs_.Reset();
  }
  void OnComponentUpdateError(
      const base::Version& version,
      IwaKeyRotationInfoProvider::ComponentUpdateError error) override {
    std::move(on_update_).Run(base::unexpected(error));
    obs_.Reset();
  }

 private:
  UpdateCallback on_update_;
  base::ScopedObservation<IwaKeyRotationInfoProvider,
                          IwaKeyRotationInfoProvider::Observer>
      obs_{this};
};

}  // namespace

base::expected<void, IwaKeyRotationInfoProvider::ComponentUpdateError>
UpdateKeyRotationInfo(const base::Version& version,
                      const base::FilePath& path) {
  base::test::TestFuture<
      base::expected<void, IwaKeyRotationInfoProvider::ComponentUpdateError>>
      future;
  auto waiter = std::make_unique<ComponentUpdateWaiter>(future.GetCallback());
  IwaKeyRotationInfoProvider::GetInstance()->LoadKeyRotationData(version, path);
  return future.Take();
}

base::expected<void, IwaKeyRotationInfoProvider::ComponentUpdateError>
UpdateKeyRotationInfo(const base::Version& version,
                      const IwaKeyRotations& kr_proto) {
  base::ScopedTempDir component_install_dir;
  CHECK(component_install_dir.CreateUniqueTempDir());
  auto path = component_install_dir.GetPath().AppendASCII("krc");
  CHECK(base::WriteFile(path, kr_proto.SerializeAsString()));
  return UpdateKeyRotationInfo(version, path);
}

base::expected<void, IwaKeyRotationInfoProvider::ComponentUpdateError>
UpdateKeyRotationInfo(const base::Version& version,
                      const std::string& web_bundle_id,
                      std::optional<base::span<const uint8_t>> expected_key) {
  IwaKeyRotations kr_proto;
  IwaKeyRotations::KeyRotationInfo kr_info;
  if (expected_key) {
    kr_info.set_expected_key(base::Base64Encode(*expected_key));
  }
  kr_proto.mutable_key_rotations()->emplace(web_bundle_id, std::move(kr_info));
  return UpdateKeyRotationInfo(version, kr_proto);
}

}  // namespace web_app::test
