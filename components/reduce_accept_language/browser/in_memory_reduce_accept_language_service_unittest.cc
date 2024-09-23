// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/reduce_accept_language/browser/in_memory_reduce_accept_language_service.h"

#include <optional>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace reduce_accept_language {

TEST(InMemoryReduceAcceptLanguageServiceTests, ValidTests) {
  const struct {
    std::vector<std::string> user_accept_language;
    std::string persist_language;
  } tests[] = {
      {{}, "zh"},
      {{"en-us"}, "zh"},
      {{"en-us", "zh"}, "zh"},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    InMemoryReduceAcceptLanguageService in_memory_service(
        tests[i].user_accept_language);

    std::vector<std::string> actual_accept_languages =
        in_memory_service.GetUserAcceptLanguages();
    EXPECT_EQ(tests[i].user_accept_language, actual_accept_languages);

    // Persist language.
    url::Origin origin = url::Origin::Create(GURL("https://example.com"));
    in_memory_service.PersistReducedLanguage(origin, tests[i].persist_language);
    // Get persist language.
    std::optional<std::string> actual_persist_language =
        in_memory_service.GetReducedLanguage(origin);
    EXPECT_EQ(tests[i].persist_language, actual_persist_language)
        << "Test case " << i << ": expected persist language "
        << tests[i].persist_language << " but got "
        << (actual_persist_language ? actual_persist_language.value()
                                    : "nullopt")
        << ".";
    // Clear persist language.
    in_memory_service.ClearReducedLanguage(origin);
    actual_persist_language = in_memory_service.GetReducedLanguage(origin);
    EXPECT_FALSE(actual_persist_language.has_value());
  }
}

TEST(InMemoryReduceAcceptLanguageServiceTests, InvalidUrlTests) {
  InMemoryReduceAcceptLanguageService in_memory_service({"en"});
  url::Origin origin = url::Origin::Create(GURL("ws://example.com"));

  in_memory_service.PersistReducedLanguage(origin, "en");
  std::optional<std::string> actual_persist_language =
      in_memory_service.GetReducedLanguage(origin);
  EXPECT_FALSE(actual_persist_language.has_value());

  // Clear persist language.
  in_memory_service.ClearReducedLanguage(origin);
  actual_persist_language = in_memory_service.GetReducedLanguage(origin);
  EXPECT_FALSE(actual_persist_language.has_value());
}

}  // namespace reduce_accept_language
