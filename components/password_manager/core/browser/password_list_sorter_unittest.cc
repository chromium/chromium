// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_list_sorter.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

struct SortEntry {
  const char* const origin;
  const char* const username;
  const char* const password;
  const char* const app_display_name;
  const char* const federation;
  const bool is_blocked;
  const int expected_position;
};

void SortAndCheckPositions(const std::vector<SortEntry>& test_entries) {
  std::vector<std::unique_ptr<PasswordForm>> list;
  size_t expected_number_of_unique_entries = 0;
  for (const SortEntry& entry : test_entries) {
    auto form = std::make_unique<PasswordForm>();
    form->signon_realm = entry.origin;
    form->url = GURL(entry.origin);
    form->blocked_by_user = entry.is_blocked;
    if (!entry.is_blocked) {
      form->username_value = base::ASCIIToUTF16(entry.username);
      form->password_value = base::ASCIIToUTF16(entry.password);
      if (entry.federation != nullptr)
        form->federation_origin = url::Origin::Create(GURL(entry.federation));
    }
    if (entry.app_display_name)
      form->app_display_name = entry.app_display_name;
    list.push_back(std::move(form));
    if (entry.expected_position >= 0)
      expected_number_of_unique_entries++;
  }

  DuplicatesMap duplicates;
  password_manager::SortEntriesAndHideDuplicates(&list, &duplicates);

  ASSERT_EQ(expected_number_of_unique_entries, list.size());
  ASSERT_EQ(test_entries.size() - expected_number_of_unique_entries,
            duplicates.size());
  for (const SortEntry& entry : test_entries) {
    if (entry.expected_position >= 0) {
      SCOPED_TRACE(testing::Message("position in sorted list: ")
                   << entry.expected_position);
      EXPECT_EQ(GURL(entry.origin), list[entry.expected_position]->url);
      if (!entry.is_blocked) {
        EXPECT_EQ(base::ASCIIToUTF16(entry.username),
                  list[entry.expected_position]->username_value);
        EXPECT_EQ(base::ASCIIToUTF16(entry.password),
                  list[entry.expected_position]->password_value);
        if (entry.federation != nullptr)
          EXPECT_EQ(url::Origin::Create(GURL(entry.federation)),
                    list[entry.expected_position]->federation_origin);
      }
    }
  }
}

}  // namespace

TEST(PasswordListSorterTest, Sorting_DifferentOrigins) {
  const std::vector<SortEntry> test_cases = {
      {"http://example-b.com", "user_a", "pwd", nullptr, nullptr, false, 2},
      {"http://example-a.com", "user_a1", "pwd", nullptr, nullptr, false, 0},
      {"http://example-a.com", "user_a2", "pwd", nullptr, nullptr, false, 1},
      {"http://example-c.com", "user_a", "pwd", nullptr, nullptr, false, 3}};
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_DifferentUsernames) {
  const std::vector<SortEntry> test_cases = {
      {"http://example.com", "user_a", "pwd", nullptr, nullptr, false, 0},
      {"http://example.com", "user_c", "pwd", nullptr, nullptr, false, 2},
      {"http://example.com", "user_b", "pwd", nullptr, nullptr, false, 1}};
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_DifferentPasswords) {
  const std::vector<SortEntry> test_cases = {
      {"http://example.com", "user_a", "1", nullptr, nullptr, false, 0},
      {"http://example.com", "user_a", "2", nullptr, nullptr, false, 1},
      {"http://example.com", "user_a", "3", nullptr, nullptr, false, 2}};
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_DifferentSchemes) {
  const std::vector<SortEntry> test_cases = {
      {"https://example.com", "user", "1", nullptr, nullptr, false, 1},
      {"https://example.com", "user", "1", nullptr, nullptr, false,
       -1},  // Hide it.
      {"http://example.com", "user", "1", nullptr, nullptr, false, 0}};
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_HideDuplicates) {
  const std::vector<SortEntry> test_cases = {
      {"http://example.com", "user_a", "pwd", nullptr, nullptr, false, 0},
      // Different username.
      {"http://example.com", "user_b", "pwd", nullptr, nullptr, false, 2},
      // Different password.
      {"http://example.com", "user_a", "secret", nullptr, nullptr, false, 1},
      // Different origin.
      {"http://sub1.example.com", "user_a", "pwd", nullptr, nullptr, false, 3},
      {"http://example.com", "user_a", "pwd", nullptr, nullptr, false,
       -1}  // Hide it.
  };
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_Subdomains) {
  const std::vector<SortEntry> test_cases = {
      {"http://example.com", "u", "p", nullptr, nullptr, false, 0},
      {"http://b.example.com", "u", "p", nullptr, nullptr, false, 6},
      {"http://a.example.com", "u", "p", nullptr, nullptr, false, 1},
      {"http://1.a.example.com", "u", "p", nullptr, nullptr, false, 2},
      {"http://2.a.example.com", "u", "p", nullptr, nullptr, false, 3},
      {"http://x.2.a.example.com", "u", "p", nullptr, nullptr, false, 4},
      {"http://y.2.a.example.com", "u", "p", nullptr, nullptr, false, 5},
  };
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_PasswordExceptions) {
  const std::vector<SortEntry> test_cases = {
      {"http://example-b.com", nullptr, nullptr, nullptr, nullptr, true, 1},
      {"http://example-a.com", nullptr, nullptr, nullptr, nullptr, true, 0},
      {"http://example-a.com", nullptr, nullptr, nullptr, nullptr, true,
       -1},  // Hide it.
      {"http://example-c.com", nullptr, nullptr, nullptr, nullptr, true, 2}};
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_AndroidCredentials) {
  const std::vector<SortEntry> test_cases = {
      // Regular Web Credential.
      {"https://alpha.example.com", "user", "secret", nullptr, nullptr, false,
       3},
      // First Android Credential.
      {"android://hash@com.example.alpha", "user", "secret", nullptr, nullptr,
       false, 0},
      // App display name is irrelevant, this should be hidden as a duplicate
      // of the first one.
      {"android://hash@com.example.alpha", "user", "secret", "Example App",
       nullptr, false, -1},
      // Apps with different package names are distinct.
      {"android://hash@com.example.beta", "user", "secret", nullptr, nullptr,
       false, 1},
      // Apps with same package name but different hashes are distinct.
      {"android://hash_different@com.example.alpha", "user", "secret", nullptr,
       nullptr, false, 2}};
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_Federations) {
  const std::vector<SortEntry> test_cases = {
      {"https://example.com", "user", "secret", nullptr, nullptr, false, 0},
      {"https://example.com", "user", "secret", nullptr, "https://fed1.com",
       false, 1},
      {"https://example.com", "user", "secret", nullptr, "https://fed2.com",
       false, 2}};
  SortAndCheckPositions(test_cases);
}

TEST(PasswordListSorterTest, Sorting_SpecialCharacters) {
  // URLs with encoded special characters should not cause crash during sorting.
  const std::vector<SortEntry> test_cases = {
      {"https://xn--bea5m6d.com/", "user_a", "pwd", nullptr, nullptr, false, 4},
      {"https://uoy.com/", "user_a", "pwd", nullptr, nullptr, false, 1},
      {"https://zrc.com/", "user_a", "pwd", nullptr, nullptr, false, 6},
      {"https://abc.com/", "user_a", "pwd", nullptr, nullptr, false, 0},
      {"https://xn--ab-fma.com/", "user_a", "pwd", nullptr, nullptr, false, 2},
      {"https://xn--bc-lia.com/", "user_a", "pwd", nullptr, nullptr, false, 3},
      {"https://xn--ndalk.com/", "user_a", "pwd", nullptr, nullptr, false, 5},
  };
  SortAndCheckPositions(test_cases);
}

}  // namespace password_manager
