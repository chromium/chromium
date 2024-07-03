// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_ROTATION_IWA_KEY_ROTATION_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_ROTATION_IWA_KEY_ROTATION_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_rotation/proto/key_rotation.pb.h"
#include "components/web_package/signed_web_bundles/key_rotation/key_rotation_info_provider.h"

namespace web_app {

// This class is a singleton responsible for processing Key Rotation Component
// data.
class IwaKeyRotationInfoProvider : public web_package::KeyRotationInfoProvider {
 public:
  enum class ComponentUpdateError {
    kStaleVersion,
    kFileNotFound,
    kProtoParsingFailure,
    kMalformedBase64Key,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnComponentUpdateSuccess(
        const base::Version& component_version) {}
    virtual void OnComponentUpdateError(const base::Version& component_version,
                                        ComponentUpdateError error) {}
  };

  static IwaKeyRotationInfoProvider* GetInstance();

  IwaKeyRotationInfoProvider(const IwaKeyRotationInfoProvider&) = delete;
  IwaKeyRotationInfoProvider& operator=(const IwaKeyRotationInfoProvider&) =
      delete;

  // web_package::KeyRotationInfoProvider:
  web_package::KeyRotationInfoProvider::KeyLookupResult GetExpectedSigningKey(
      std::string_view web_bundle_id) const override;

  // Asynchronously loads new component data and replaces the current `data_`
  // upon success and if `component_version` is greater than the stored one, and
  // informs observers about the operation result.
  void LoadKeyRotationData(const base::Version& component_version,
                           const base::FilePath& file_path);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend struct base::DefaultSingletonTraits<IwaKeyRotationInfoProvider>;

  struct ComponentData {
    base::Version version;
    IwaKeyRotations proto;
  };

  IwaKeyRotationInfoProvider();
  ~IwaKeyRotationInfoProvider() override;

  void OnKeyRotationDataLoaded(
      const base::Version& version,
      base::expected<IwaKeyRotations, ComponentUpdateError>);

  void DispatchComponentUpdateSuccess(
      const base::Version& component_version) const;

  void DispatchComponentUpdateError(const base::Version& component_version,
                                    ComponentUpdateError error) const;

  std::optional<ComponentData> data_;
  base::ObserverList<Observer> observers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_ROTATION_IWA_KEY_ROTATION_PROVIDER_H_
