// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MANAGEMENT_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MANAGEMENT_DATA_H_

#include <iosfwd>
#include <string>
#include <variant>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "url/gurl.h"

namespace web_app {

namespace proto {
class ManagementData;
}  // namespace proto

// Consolidates management source membership and per-source installation
// configuration on a WebApp, enforcing per-source field invariants and sync
// between `entries_` and `active_sources_`.
class ManagementData {
 public:
  enum class ParseError {
    kInvalidInstallUrl,
    kEmptyPolicyId,
  };

  ManagementData();
  ~ManagementData();
  ManagementData(const ManagementData&);
  ManagementData(ManagementData&&) noexcept;
  ManagementData& operator=(const ManagementData&);
  ManagementData& operator=(ManagementData&&) noexcept;
  friend bool operator==(const ManagementData&,
                         const ManagementData&) = default;

  // Key/source membership queries:
  bool HasSource(WebAppManagement::Type type) const;
  bool HasAnySources() const;
  WebAppManagementTypes GetSources() const;

  // Idempotent insertion via `try_emplace` (preserves existing per-source
  // fields).
  void AddSource(WebAppManagement::Type type);
  void RemoveSource(WebAppManagement::Type type);

  // Per-source typed and helper accessors:
  bool IsPlaceholder(WebAppManagement::Type type) const;
  // `CHECK`s `kSourcesWithPlaceholder.Has(type)` and `HasSource(type)`.
  void SetIsPlaceholder(WebAppManagement::Type type, bool is_placeholder);
  const base::flat_set<GURL>& GetInstallUrls(WebAppManagement::Type type) const;
  bool HasInstallUrl(const GURL& install_url) const;
  // `CHECK`s `install_url.is_valid()`, `kSourcesWithInstallUrls.Has(type)`, and
  // `HasSource(type)`.
  void AddInstallUrl(WebAppManagement::Type type, GURL install_url);
  // Removes `install_url` from `type` while leaving `type` active in
  // `GetSources()` even when the last URL is removed.
  bool RemoveInstallUrl(WebAppManagement::Type type, const GURL& install_url);
  // `CHECK`s non-empty `policy_id` and
  // `HasSource(WebAppManagement::kWebAppStore)`.
  void AddAdditionalPolicyId(std::string policy_id);
  // Returns `additional_policy_ids` stored on `WebAppManagement::kWebAppStore`
  // (or an empty set if `kWebAppStore` is inactive).
  const base::flat_set<std::string>& GetAdditionalPolicyIds() const;

  // Parses `proto` into `ManagementData`. An empty `proto` succeeds with no
  // active sources so pending-uninstall apps remain valid; top-level source
  // presence validation stays with `ParseWebAppProto()`.
  static base::expected<ManagementData, ParseError> Parse(
      const proto::ManagementData& proto);
  proto::ManagementData ToProto() const;
  base::DictValue AsDebugValue() const;

 private:
  struct SystemSourceData {
    friend bool operator==(const SystemSourceData&,
                           const SystemSourceData&) = default;

    base::flat_set<GURL> install_urls;
  };

  struct PolicySourceData {
    friend bool operator==(const PolicySourceData&,
                           const PolicySourceData&) = default;

    base::flat_set<GURL> install_urls;
    bool is_placeholder = false;
  };

  struct WebAppStoreSourceData {
    friend bool operator==(const WebAppStoreSourceData&,
                           const WebAppStoreSourceData&) = default;

    base::flat_set<GURL> install_urls;

    // A list of additional terms to use when matching this app against
    // identifiers in admin policies (for shelf pinning, default file handlers,
    // etc). This list is not meant to be an exhaustive enumeration of all
    // possible policy_ids, only a supplement for tricky cases.
    base::flat_set<std::string> additional_policy_ids;
  };

  struct SyncSourceData {
    friend bool operator==(const SyncSourceData&,
                           const SyncSourceData&) = default;
  };

  struct DefaultSourceData {
    friend bool operator==(const DefaultSourceData&,
                           const DefaultSourceData&) = default;

    base::flat_set<GURL> install_urls;
  };

  struct SubAppSourceData {
    friend bool operator==(const SubAppSourceData&,
                           const SubAppSourceData&) = default;

    base::flat_set<GURL> install_urls;
  };

  struct KioskSourceData {
    friend bool operator==(const KioskSourceData&,
                           const KioskSourceData&) = default;

    base::flat_set<GURL> install_urls;
    bool is_placeholder = false;
  };

  struct OemSourceData {
    friend bool operator==(const OemSourceData&,
                           const OemSourceData&) = default;

    base::flat_set<GURL> install_urls;
  };

  struct OneDriveIntegrationSourceData {
    friend bool operator==(const OneDriveIntegrationSourceData&,
                           const OneDriveIntegrationSourceData&) = default;

    base::flat_set<GURL> install_urls;
  };

  struct ApsDefaultSourceData {
    friend bool operator==(const ApsDefaultSourceData&,
                           const ApsDefaultSourceData&) = default;

    base::flat_set<GURL> install_urls;
  };

  struct IwaShimlessRmaSourceData {
    friend bool operator==(const IwaShimlessRmaSourceData&,
                           const IwaShimlessRmaSourceData&) = default;
  };

  struct IwaPolicySourceData {
    friend bool operator==(const IwaPolicySourceData&,
                           const IwaPolicySourceData&) = default;
  };

  struct IwaUserInstalledSourceData {
    friend bool operator==(const IwaUserInstalledSourceData&,
                           const IwaUserInstalledSourceData&) = default;
  };

  struct UserInstalledSourceData {
    friend bool operator==(const UserInstalledSourceData&,
                           const UserInstalledSourceData&) = default;
  };

  using ManagementSourceData = std::variant<SystemSourceData,
                                            PolicySourceData,
                                            WebAppStoreSourceData,
                                            SyncSourceData,
                                            DefaultSourceData,
                                            SubAppSourceData,
                                            KioskSourceData,
                                            OemSourceData,
                                            OneDriveIntegrationSourceData,
                                            ApsDefaultSourceData,
                                            IwaShimlessRmaSourceData,
                                            IwaPolicySourceData,
                                            IwaUserInstalledSourceData,
                                            UserInstalledSourceData>;

  static ManagementSourceData MakeDefaultSourceData(
      WebAppManagement::Type type);

  ManagementSourceData& GetEntry(WebAppManagement::Type type);

  // Keyed by active `WebAppManagement::Type`s. A management type is active on
  // the app if and only if it is present as a key in `entries_` (even when its
  // per-source struct has no populated fields, such as `SyncSourceData` or a
  // URL-bearing source whose last URL was removed).
  base::flat_map<WebAppManagement::Type, ManagementSourceData> entries_;

  // Cached O(1) bitset of active sources; set bits always match the keys of
  // `entries_` 1:1.
  WebAppManagementTypes active_sources_;
};

std::ostream& operator<<(std::ostream& out, const ManagementData& data);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MANAGEMENT_DATA_H_
