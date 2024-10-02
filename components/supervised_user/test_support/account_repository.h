// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_ACCOUNT_REPOSITORY_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_ACCOUNT_REPOSITORY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_util.h"
#include "base/json/json_value_converter.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"

namespace supervised_user {

// Entities for test account repository.
// example JSON: {"families": [{"members": [{"role": "PARENT", "username":
// "joe@gmail.com", "password": "123"}], "feature": "REGULAR"}]}
namespace test_accounts {
enum class Feature : int {
  REGULAR,
  DMA_ELIGIBLE_WITH_CONSENT,
  DMA_ELIGIBLE_WITHOUT_CONSENT,
  DMA_INELIGIBLE,
};
struct FamilyMember {
  static void RegisterJSONConverter(
      base::JSONValueConverter<FamilyMember>* converter);
  static bool ParseRole(std::string_view value,
                        kidsmanagement::FamilyRole* role);

  // Struct is complex.
  FamilyMember();
  FamilyMember(const FamilyMember&);
  FamilyMember& operator=(const FamilyMember&);
  ~FamilyMember();

  std::string name;
  kidsmanagement::FamilyRole role;
  std::string username;
  std::string password;
};

struct Family {
  static void RegisterJSONConverter(
      base::JSONValueConverter<Family>* converter);
  static bool ParseFeature(std::string_view value, Feature* feature);

  // Struct is complex.
  Family();
  Family(const Family& other);
  Family& operator=(const Family& other);
  ~Family();

  std::vector<std::unique_ptr<FamilyMember>> members;
  Feature feature;
};

struct Repository {
  static void RegisterJSONConverter(
      base::JSONValueConverter<Repository>* converter);

  // Struct is complex.
  Repository();
  Repository(const Repository& other);
  Repository& operator=(const Repository& other);
  ~Repository();

  std::vector<std::unique_ptr<Family>> families;
};
}  // namespace test_accounts

// Returns first family member with requested role.
std::optional<test_accounts::FamilyMember> GetFirstFamilyMemberByRole(
    const test_accounts::Family& family,
    kidsmanagement::FamilyRole role);

// File-based repository of test accounts. Balances access to the accounts.
class TestAccountRepository {
 public:
  // Crashes if the backing file repository is ill-formatted. See the unit-tests
  // for examples of valid spec.
  TestAccountRepository();
  explicit TestAccountRepository(const base::FilePath& path);
  ~TestAccountRepository();
  TestAccountRepository(const TestAccountRepository&) = delete;
  TestAccountRepository& operator=(const TestAccountRepository&) = delete;

  // Returns random family that satisfies the feature if it exists.
  // Randomness is on purpose: tests can't rely on a specific family by order.
  // Instead, they should pick a family using a required feature.
  std::optional<test_accounts::Family> GetRandomFamilyByFeature(
      test_accounts::Feature feature);

 private:
  test_accounts::Repository repository_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_ACCOUNT_REPOSITORY_H_
