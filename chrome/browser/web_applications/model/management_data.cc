// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/management_data.h"

#include <ostream>
#include <string>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/web_applications/proto/web_app_management_data.pb.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"
#include "url/gurl.h"

namespace web_app {

namespace {

base::expected<base::flat_set<GURL>, ManagementData::ParseError>
ParseInstallUrls(
    const google::protobuf::RepeatedPtrField<std::string>& proto_urls) {
  base::flat_set<GURL> install_urls;
  for (const std::string& url_str : proto_urls) {
    GURL install_url(url_str);
    if (!install_url.is_valid()) {
      return base::unexpected(ManagementData::ParseError::kInvalidInstallUrl);
    }
    install_urls.insert(std::move(install_url));
  }
  return install_urls;
}

template <typename ProtoSubmessageT>
void WriteInstallUrls(const base::flat_set<GURL>& install_urls,
                      ProtoSubmessageT* proto) {
  for (const GURL& url : install_urls) {
    CHECK(url.is_valid());
    proto->add_install_urls(url.spec());
  }
}

}  // namespace

ManagementData::ManagementData() = default;
ManagementData::~ManagementData() = default;
ManagementData::ManagementData(const ManagementData&) = default;
ManagementData::ManagementData(ManagementData&& other) noexcept
    : entries_(std::move(other.entries_)),
      active_sources_(std::exchange(other.active_sources_, {})) {}
ManagementData& ManagementData::operator=(const ManagementData&) = default;
ManagementData& ManagementData::operator=(ManagementData&& other) noexcept {
  if (this != &other) {
    entries_ = std::move(other.entries_);
    active_sources_ = std::exchange(other.active_sources_, {});
  }
  return *this;
}

bool ManagementData::HasSource(WebAppManagement::Type type) const {
  return active_sources_.Has(type);
}

bool ManagementData::HasAnySources() const {
  return !active_sources_.empty();
}

WebAppManagementTypes ManagementData::GetSources() const {
  return active_sources_;
}

void ManagementData::AddSource(WebAppManagement::Type type) {
  active_sources_.Put(type);
  entries_.try_emplace(type, MakeDefaultSourceData(type));
}

void ManagementData::RemoveSource(WebAppManagement::Type type) {
  active_sources_.Remove(type);
  entries_.erase(type);
}

bool ManagementData::IsPlaceholder(WebAppManagement::Type type) const {
  auto it = entries_.find(type);
  if (it == entries_.end()) {
    return false;
  }
  return std::visit(
      [](const auto& d) -> bool {
        if constexpr (requires { d.is_placeholder; }) {
          return d.is_placeholder;
        } else {
          return false;
        }
      },
      it->second);
}

void ManagementData::SetIsPlaceholder(WebAppManagement::Type type,
                                      bool is_placeholder) {
  CHECK(kSourcesWithPlaceholder.Has(type)) << base::ToString(type);
  std::visit(
      [is_placeholder](auto& d) {
        if constexpr (requires { d.is_placeholder; }) {
          d.is_placeholder = is_placeholder;
        } else {
          NOTREACHED();
        }
      },
      GetEntry(type));
}

const base::flat_set<GURL>& ManagementData::GetInstallUrls(
    WebAppManagement::Type type) const {
  static const base::NoDestructor<base::flat_set<GURL>> kEmptyUrls;
  auto it = entries_.find(type);
  if (it == entries_.end()) {
    return *kEmptyUrls;
  }
  return std::visit(
      [](const auto& d) -> const base::flat_set<GURL>& {
        if constexpr (requires { d.install_urls; }) {
          return d.install_urls;
        } else {
          return *kEmptyUrls;
        }
      },
      it->second);
}

bool ManagementData::HasInstallUrl(const GURL& install_url) const {
  for (const auto& [type, source_data] : entries_) {
    bool found = std::visit(
        [&install_url](const auto& d) -> bool {
          if constexpr (requires { d.install_urls; }) {
            return d.install_urls.contains(install_url);
          } else {
            return false;
          }
        },
        source_data);
    if (found) {
      return true;
    }
  }
  return false;
}

void ManagementData::AddInstallUrl(WebAppManagement::Type type,
                                   GURL install_url) {
  CHECK(install_url.is_valid());
  CHECK(kSourcesWithInstallUrls.Has(type)) << base::ToString(type);
  std::visit(
      [&install_url](auto& d) {
        if constexpr (requires { d.install_urls; }) {
          d.install_urls.insert(std::move(install_url));
        } else {
          NOTREACHED();
        }
      },
      GetEntry(type));
}

bool ManagementData::RemoveInstallUrl(WebAppManagement::Type type,
                                      const GURL& install_url) {
  auto it = entries_.find(type);
  if (it == entries_.end()) {
    return false;
  }
  return std::visit(
      [&install_url](auto& d) -> bool {
        if constexpr (requires { d.install_urls; }) {
          return d.install_urls.erase(install_url) > 0;
        } else {
          return false;
        }
      },
      it->second);
}

void ManagementData::AddAdditionalPolicyId(std::string policy_id) {
  CHECK(!policy_id.empty());
  std::get<WebAppStoreSourceData>(GetEntry(WebAppManagement::kWebAppStore))
      .additional_policy_ids.insert(std::move(policy_id));
}

const base::flat_set<std::string>& ManagementData::GetAdditionalPolicyIds()
    const {
  static const base::NoDestructor<base::flat_set<std::string>> kEmptyPolicyIds;
  auto it = entries_.find(WebAppManagement::kWebAppStore);
  if (it == entries_.end()) {
    return *kEmptyPolicyIds;
  }
  return std::get<WebAppStoreSourceData>(it->second).additional_policy_ids;
}

// static
base::expected<ManagementData, ManagementData::ParseError>
ManagementData::Parse(const proto::ManagementData& proto) {
  ManagementData result;
  auto insert_entry = [&result](WebAppManagement::Type type,
                                ManagementSourceData data) {
    result.active_sources_.Put(type);
    result.entries_.emplace(type, std::move(data));
  };

  auto parse_url_only = []<typename SourceDataT>(const auto& subproto)
      -> base::expected<ManagementSourceData, ParseError> {
    ASSIGN_OR_RETURN(base::flat_set<GURL> install_urls,
                     ParseInstallUrls(subproto.install_urls()));
    return SourceDataT{.install_urls = std::move(install_urls)};
  };

  if (proto.has_system()) {
    ASSIGN_OR_RETURN(
        auto data, parse_url_only.operator()<SystemSourceData>(proto.system()));
    insert_entry(WebAppManagement::kSystem, std::move(data));
  }
  if (proto.has_policy()) {
    ASSIGN_OR_RETURN(base::flat_set<GURL> install_urls,
                     ParseInstallUrls(proto.policy().install_urls()));
    PolicySourceData data;
    data.install_urls = std::move(install_urls);
    data.is_placeholder = proto.policy().is_placeholder();
    insert_entry(WebAppManagement::kPolicy, std::move(data));
  }
  if (proto.has_web_app_store()) {
    ASSIGN_OR_RETURN(base::flat_set<GURL> install_urls,
                     ParseInstallUrls(proto.web_app_store().install_urls()));
    WebAppStoreSourceData data;
    data.install_urls = std::move(install_urls);
    for (const std::string& policy_id :
         proto.web_app_store().additional_policy_ids()) {
      if (policy_id.empty()) {
        return base::unexpected(ParseError::kEmptyPolicyId);
      }
      data.additional_policy_ids.insert(policy_id);
    }
    insert_entry(WebAppManagement::kWebAppStore, std::move(data));
  }
  if (proto.has_sync()) {
    insert_entry(WebAppManagement::kSync, SyncSourceData());
  }
  if (proto.has_default_source()) {
    ASSIGN_OR_RETURN(auto data, parse_url_only.operator()<DefaultSourceData>(
                                    proto.default_source()));
    insert_entry(WebAppManagement::kDefault, std::move(data));
  }
  if (proto.has_sub_app()) {
    ASSIGN_OR_RETURN(auto data, parse_url_only.operator()<SubAppSourceData>(
                                    proto.sub_app()));
    insert_entry(WebAppManagement::kSubApp, std::move(data));
  }
  if (proto.has_kiosk()) {
    ASSIGN_OR_RETURN(base::flat_set<GURL> install_urls,
                     ParseInstallUrls(proto.kiosk().install_urls()));
    KioskSourceData data;
    data.install_urls = std::move(install_urls);
    data.is_placeholder = proto.kiosk().is_placeholder();
    insert_entry(WebAppManagement::kKiosk, std::move(data));
  }
  if (proto.has_oem()) {
    ASSIGN_OR_RETURN(auto data,
                     parse_url_only.operator()<OemSourceData>(proto.oem()));
    insert_entry(WebAppManagement::kOem, std::move(data));
  }
  if (proto.has_one_drive_integration()) {
    ASSIGN_OR_RETURN(auto data,
                     parse_url_only.operator()<OneDriveIntegrationSourceData>(
                         proto.one_drive_integration()));
    insert_entry(WebAppManagement::kOneDriveIntegration, std::move(data));
  }
  if (proto.has_aps_default()) {
    ASSIGN_OR_RETURN(auto data, parse_url_only.operator()<ApsDefaultSourceData>(
                                    proto.aps_default()));
    insert_entry(WebAppManagement::kApsDefault, std::move(data));
  }
  if (proto.has_iwa_shimless_rma()) {
    insert_entry(WebAppManagement::kIwaShimlessRma, IwaShimlessRmaSourceData());
  }
  if (proto.has_iwa_policy()) {
    insert_entry(WebAppManagement::kIwaPolicy, IwaPolicySourceData());
  }
  if (proto.has_iwa_user_installed()) {
    insert_entry(WebAppManagement::kIwaUserInstalled,
                 IwaUserInstalledSourceData());
  }
  if (proto.has_user_installed()) {
    insert_entry(WebAppManagement::kUserInstalled, UserInstalledSourceData());
  }

  return result;
}

proto::ManagementData ManagementData::ToProto() const {
  proto::ManagementData proto;
  for (const auto& [type, source_data] : entries_) {
    std::visit(
        absl::Overload{
            [&](const SystemSourceData& d) {
              WriteInstallUrls(d.install_urls, proto.mutable_system());
            },
            [&](const PolicySourceData& d) {
              auto* subproto = proto.mutable_policy();
              WriteInstallUrls(d.install_urls, subproto);
              subproto->set_is_placeholder(d.is_placeholder);
            },
            [&](const WebAppStoreSourceData& d) {
              auto* subproto = proto.mutable_web_app_store();
              WriteInstallUrls(d.install_urls, subproto);
              for (const std::string& policy_id : d.additional_policy_ids) {
                CHECK(!policy_id.empty());
                subproto->add_additional_policy_ids(policy_id);
              }
            },
            [&](const SyncSourceData&) { proto.mutable_sync(); },
            [&](const DefaultSourceData& d) {
              WriteInstallUrls(d.install_urls, proto.mutable_default_source());
            },
            [&](const SubAppSourceData& d) {
              WriteInstallUrls(d.install_urls, proto.mutable_sub_app());
            },
            [&](const KioskSourceData& d) {
              auto* subproto = proto.mutable_kiosk();
              WriteInstallUrls(d.install_urls, subproto);
              subproto->set_is_placeholder(d.is_placeholder);
            },
            [&](const OemSourceData& d) {
              WriteInstallUrls(d.install_urls, proto.mutable_oem());
            },
            [&](const OneDriveIntegrationSourceData& d) {
              WriteInstallUrls(d.install_urls,
                               proto.mutable_one_drive_integration());
            },
            [&](const ApsDefaultSourceData& d) {
              WriteInstallUrls(d.install_urls, proto.mutable_aps_default());
            },
            [&](const IwaShimlessRmaSourceData&) {
              proto.mutable_iwa_shimless_rma();
            },
            [&](const IwaPolicySourceData&) { proto.mutable_iwa_policy(); },
            [&](const IwaUserInstalledSourceData&) {
              proto.mutable_iwa_user_installed();
            },
            [&](const UserInstalledSourceData&) {
              proto.mutable_user_installed();
            },
        },
        source_data);
  }
  return proto;
}

base::DictValue ManagementData::AsDebugValue() const {
  base::DictValue root;
  for (const auto& [type, source_data] : entries_) {
    base::DictValue entry_dict = std::visit(
        [](const auto& d) -> base::DictValue {
          base::DictValue dict;
          if constexpr (requires { d.install_urls; }) {
            base::ListValue urls;
            for (const GURL& url : d.install_urls) {
              urls.Append(url.spec());
            }
            dict.Set("install_urls", std::move(urls));
          }
          if constexpr (requires { d.is_placeholder; }) {
            dict.Set("is_placeholder", d.is_placeholder);
          }
          if constexpr (requires { d.additional_policy_ids; }) {
            base::ListValue policy_ids;
            for (const std::string& policy_id : d.additional_policy_ids) {
              policy_ids.Append(policy_id);
            }
            dict.Set("additional_policy_ids", std::move(policy_ids));
          }
          return dict;
        },
        source_data);
    root.Set(base::ToString(type), std::move(entry_dict));
  }
  return root;
}

// static
ManagementData::ManagementSourceData ManagementData::MakeDefaultSourceData(
    WebAppManagement::Type type) {
  switch (type) {
    case WebAppManagement::kSystem:
      return SystemSourceData();
    case WebAppManagement::kIwaShimlessRma:
      return IwaShimlessRmaSourceData();
    case WebAppManagement::kKiosk:
      return KioskSourceData();
    case WebAppManagement::kPolicy:
      return PolicySourceData();
    case WebAppManagement::kIwaPolicy:
      return IwaPolicySourceData();
    case WebAppManagement::kOem:
      return OemSourceData();
    case WebAppManagement::kSubApp:
      return SubAppSourceData();
    case WebAppManagement::kWebAppStore:
      return WebAppStoreSourceData();
    case WebAppManagement::kOneDriveIntegration:
      return OneDriveIntegrationSourceData();
    case WebAppManagement::kSync:
      return SyncSourceData();
    case WebAppManagement::kUserInstalled:
      return UserInstalledSourceData();
    case WebAppManagement::kIwaUserInstalled:
      return IwaUserInstalledSourceData();
    case WebAppManagement::kApsDefault:
      return ApsDefaultSourceData();
    case WebAppManagement::kDefault:
      return DefaultSourceData();
  }
}

ManagementData::ManagementSourceData& ManagementData::GetEntry(
    WebAppManagement::Type type) {
  CHECK(HasSource(type)) << base::ToString(type);
  auto it = entries_.find(type);
  CHECK(it != entries_.end());
  return it->second;
}

std::ostream& operator<<(std::ostream& out, const ManagementData& data) {
  return out << data.AsDebugValue().DebugString();
}

}  // namespace web_app
