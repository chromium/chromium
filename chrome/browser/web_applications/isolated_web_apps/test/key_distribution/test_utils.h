// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_KEY_DISTRIBUTION_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_KEY_DISTRIBUTION_TEST_UTILS_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_histograms.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/proto/key_distribution.pb.h"

namespace web_app::test {

struct IwaComponentMetadata {
  base::Version version;
  bool is_preloaded;
};

struct KeyDistributionComponent {
  base::Version version;
  bool is_preloaded;
  IwaKeyDistribution component_data;

  // Uploads the component just by replacing the saved internal data in
  // IwaKeyDistributionInfoProvider.
  void InjectComponentDataDirectly();
  // Uploads the component using more e2e approach. The proto file with the
  // component data is saved and loaded by IwaKeyDistributionInfoProvider.
  base::expected<void, IwaComponentUpdateError> UploadFromComponentFolder();
};

// A builder-style class to help create and update the key distribution
// component in tests. After configuring the desired state using the
// `AddTo...`/`With..` methods, call `Build()` and update the component with one
// of KeyDistributionComponent methods. Unset fields will be set to the default
// value, i.e. the empty set/list.
class KeyDistributionComponentBuilder {
 public:
  struct SpecialAppPermissions {
    bool skip_capture_started_notification;
  };
  // Component update requires the higher component version than the current
  // one.
  explicit KeyDistributionComponentBuilder(
      const base::Version& component_version);
  ~KeyDistributionComponentBuilder();

  KeyDistributionComponentBuilder(const KeyDistributionComponentBuilder&) =
      delete;
  KeyDistributionComponentBuilder& operator=(
      const KeyDistributionComponentBuilder&) = delete;

  // Sets the key rotation entry for the given Web Bundle ID. If `expected_key`
  // is `std::nullopt`, then the IWA will fail signature verification. Can be
  // used many times when several key rotations are required.
  KeyDistributionComponentBuilder& AddToKeyRotations(
      const web_package::SignedWebBundleId& web_bundle_id,
      std::optional<std::vector<uint8_t>> expected_key) &;
  KeyDistributionComponentBuilder&& AddToKeyRotations(
      const web_package::SignedWebBundleId& web_bundle_id,
      std::optional<std::vector<uint8_t>> expected_key) &&;

  // Sets the special permissions for a specific app
  KeyDistributionComponentBuilder& AddToSpecialAppPermissions(
      const web_package::SignedWebBundleId& web_bundle_id,
      SpecialAppPermissions special_app_permissions) &;
  KeyDistributionComponentBuilder&& AddToSpecialAppPermissions(
      const web_package::SignedWebBundleId& web_bundle_id,
      SpecialAppPermissions special_app_permissions) &&;

  // Adds the given Web Bundle IDs to the list of IWAs that are blocked.
  KeyDistributionComponentBuilder& WithBlocklist(
      const std::vector<web_package::SignedWebBundleId>& bundle_ids) &;
  KeyDistributionComponentBuilder&& WithBlocklist(
      const std::vector<web_package::SignedWebBundleId>& bundle_ids) &&;
  KeyDistributionComponentBuilder& AddToBlocklist(
      const web_package::SignedWebBundleId& web_bundle_id) &;
  KeyDistributionComponentBuilder&& AddToBlocklist(
      const web_package::SignedWebBundleId& web_bundle_id) &&;

  // Adds the given Web Bundle IDs to the list of IWAs that are allowed to be
  // installed/updated through enterprise policy.
  KeyDistributionComponentBuilder& WithManagedAllowlist(
      const std::vector<web_package::SignedWebBundleId>& bundle_ids) &;
  KeyDistributionComponentBuilder&& WithManagedAllowlist(
      const std::vector<web_package::SignedWebBundleId>& bundle_ids) &&;
  KeyDistributionComponentBuilder& AddToManagedAllowlist(
      const web_package::SignedWebBundleId& web_bundle_id) &;
  KeyDistributionComponentBuilder&& AddToManagedAllowlist(
      const web_package::SignedWebBundleId& web_bundle_id) &&;

  KeyDistributionComponent Build() &&;

 private:
  KeyDistributionComponent component_;
};

// Synchronously updates the key distribution info provider with data from
// `path`.
base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const base::FilePath& path);

// Synchronously updates the key distribution info provider with the given
// `kd_proto`.
base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const IwaKeyDistribution& kd_proto);

// Synchronously updates the key distribution info provider with a protobuf that
// maps `web_bundle_id` to `expected_key`. If `expected_key` is a nullopt, then
// the IWA with `web_bundle_id` will fail signature verification.
// TODO(crbug.com/460419755): Remove and replace with
// KeyDistributionComponentBuilder.Build().UploadFromComponentFolder()
base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const std::string& web_bundle_id,
    std::optional<base::span<const uint8_t>> expected_key);

// Synchronously updates the key distribution info provider with a protobuf that
// only contains bundle ids in the managed allowlist
// TODO(crbug.com/460419755): Remove and replace with
// KeyDistributionComponentBuilder.Build().UploadFromComponentFolder()
base::expected<void, IwaComponentUpdateError>
UpdateKeyDistributionInfoWithAllowlist(
    const base::Version& version,
    const std::vector<web_package::SignedWebBundleId>& managed_allowlist);

// Writes `kd_proto` into `DIR_COMPONENT_USER/IwaKeyDistribution/{version}` and
// triggers the registration process with the component updater. The directory
// is deleted once IwaKeyDistributionInfoProvider has processed the update
// (regardless of the outcome).
base::expected<void, IwaComponentUpdateError>
InstallIwaKeyDistributionComponent(const base::Version& version,
                                   const IwaKeyDistribution& kd_proto);

// A shortcut for the above function that populates only the key rotation part
// of the proto.
base::expected<void, IwaComponentUpdateError>
InstallIwaKeyDistributionComponent(
    const base::Version& version,
    const std::string& web_bundle_id,
    std::optional<base::span<const uint8_t>> expected_key);

// Synchronously registers the component with the component updater and waits
// for the component updater to pick up the on-disk data in its folder.
base::expected<IwaComponentMetadata, IwaComponentUpdateError>
RegisterIwaKeyDistributionComponentAndWaitForLoad();

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_KEY_DISTRIBUTION_TEST_UTILS_H_
