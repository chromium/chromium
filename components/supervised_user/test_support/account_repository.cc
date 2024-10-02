// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/account_repository.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_value_converter.h"
#include "base/rand_util.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"

namespace supervised_user {

namespace {
// Account repositories in files are only defined for platforms that support
// end-to-end tests.
#if BUILDFLAG(IS_WIN)
const base::FilePath::StringPieceType kDefaultAccountRepositoryPath =
    FILE_PATH_LITERAL(
        "chrome/browser/internal/resources/supervised_user/"
        "win_test_accounts.json");
#elif BUILDFLAG(IS_LINUX)
const base::FilePath::StringPieceType kDefaultAccountRepositoryPath =
    FILE_PATH_LITERAL(
        "chrome/browser/internal/resources/supervised_user/"
        "linux_test_accounts.json");
#elif BUILDFLAG(IS_IOS)
const base::FilePath::StringPieceType kDefaultAccountRepositoryPath =
    FILE_PATH_LITERAL(
        "chrome/browser/internal/resources/supervised_user/"
        "ios_test_accounts.json");
#else
#error Unsupported platform
#endif

test_accounts::Repository ParseFromFile(const base::FilePath& path) {
  int error_code = 0;
  std::string error_str;
  JSONFileValueDeserializer deserializer(path);
  std::unique_ptr<base::Value> json =
      deserializer.Deserialize(&error_code, &error_str);
  CHECK(error_code == 0) << "Error reading json file at " << path
                         << ". Error code: " << error_code << " " << error_str;
  CHECK(json);

  test_accounts::Repository repository;
  base::JSONValueConverter<test_accounts::Repository> converter;
  CHECK(converter.Convert(*json, &repository));
  return repository;
}
}  // namespace

std::optional<test_accounts::Family>
TestAccountRepository::GetRandomFamilyByFeature(
    test_accounts::Feature feature) {
  std::vector<test_accounts::Family> matching_families;
  for (const auto& family : repository_.families) {
    if (family->feature == feature) {
      matching_families.push_back(*family);
    }
  }
  if (matching_families.empty()) {
    return std::nullopt;
  }
  return matching_families[base::RandInt(0, matching_families.size() - 1)];
}

std::optional<test_accounts::FamilyMember> GetFirstFamilyMemberByRole(
    const test_accounts::Family& family,
    kidsmanagement::FamilyRole role) {
  for (const auto& member : family.members) {
    if (member->role == role) {
      return *member;
    }
  }
  return std::nullopt;
}

TestAccountRepository::TestAccountRepository()
    : TestAccountRepository(base::FilePath(kDefaultAccountRepositoryPath)) {}
TestAccountRepository::TestAccountRepository(const base::FilePath& path)
    : repository_(ParseFromFile(path)) {}
TestAccountRepository::~TestAccountRepository() = default;

namespace test_accounts {
void Repository::RegisterJSONConverter(
    base::JSONValueConverter<Repository>* converter) {
  converter->RegisterRepeatedMessage<Family>("families", &Repository::families);
}
void Family::RegisterJSONConverter(
    base::JSONValueConverter<Family>* converter) {
  converter->RegisterRepeatedMessage<FamilyMember>("members", &Family::members);
  converter->RegisterCustomField<Feature>("feature", &Family::feature,
                                          &Family::ParseFeature);
}
bool Family::ParseFeature(std::string_view value, Feature* feature) {
  static std::map<std::string_view, Feature> features{
      {"REGULAR", Feature::REGULAR},
      {"DMA_ELIGIBLE_WITH_CONSENT", Feature::DMA_ELIGIBLE_WITH_CONSENT},
      {"DMA_ELIGIBLE_WITHOUT_CONSENT", Feature::DMA_ELIGIBLE_WITHOUT_CONSENT},
      {"DMA_INELIGIBLE", Feature::DMA_INELIGIBLE},
  };
  if (features.count(value) == 0) {
    return false;
  }
  *feature = features.at(value);
  return true;
}
void FamilyMember::RegisterJSONConverter(
    base::JSONValueConverter<FamilyMember>* converter) {
  converter->RegisterStringField("name", &FamilyMember::name);
  converter->RegisterCustomField<kidsmanagement::FamilyRole>(
      "role", &FamilyMember::role, &FamilyMember::ParseRole);
  converter->RegisterStringField("username", &FamilyMember::username);
  converter->RegisterStringField("password", &FamilyMember::password);
}
// LINT.IfChange(family_role_parser)
bool FamilyMember::ParseRole(std::string_view value,
                             kidsmanagement::FamilyRole* role) {
  static std::map<std::string_view, kidsmanagement::FamilyRole> roles{
      {"UNKNOWN_FAMILY_ROLE", kidsmanagement::FamilyRole::UNKNOWN_FAMILY_ROLE},
      {"HEAD_OF_HOUSEHOLD", kidsmanagement::FamilyRole::HEAD_OF_HOUSEHOLD},
      {"PARENT", kidsmanagement::FamilyRole::PARENT},
      {"MEMBER", kidsmanagement::FamilyRole::MEMBER},
      {"CHILD", kidsmanagement::FamilyRole::CHILD},
      {"UNCONFIRMED_MEMBER", kidsmanagement::FamilyRole::UNCONFIRMED_MEMBER},
  };
  if (roles.count(value) == 0) {
    return false;
  }
  *role = roles.at(value);
  return true;
}
// LINT.ThenChange(//components/supervised_user/core/browser/proto/families_common.proto::family_role)

FamilyMember::FamilyMember() = default;
FamilyMember::FamilyMember(const FamilyMember&) = default;
FamilyMember& FamilyMember::operator=(const FamilyMember&) = default;
FamilyMember::~FamilyMember() = default;

Family::Family() = default;
Family::Family(const Family& other) {
  *this = other;
}
Family& Family::operator=(const Family& other) {
  if (this != &other) {
    members.clear();
    for (const auto& member : other.members) {
      members.push_back(std::make_unique<FamilyMember>(*member));
    }
    feature = other.feature;
  }
  return *this;
}
Family::~Family() = default;

Repository::Repository() = default;
Repository::Repository(const Repository& other) {
  *this = other;
}
Repository& Repository::operator=(const Repository& other) {
  if (this != &other) {
    families.clear();
    for (const auto& family : other.families) {
      families.push_back(std::make_unique<Family>(*family));
    }
  }
  return *this;
}
Repository::~Repository() = default;
}  // namespace test_accounts

}  // namespace supervised_user
