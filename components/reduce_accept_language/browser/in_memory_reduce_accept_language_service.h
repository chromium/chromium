// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_IN_MEMORY_REDUCE_ACCEPT_LANGUAGE_SERVICE_H_
#define COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_IN_MEMORY_REDUCE_ACCEPT_LANGUAGE_SERVICE_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "url/origin.h"

namespace reduce_accept_language {

// In-memory manager of stores and retrieves the last reduced accept language
// negotiated for each origin.
//
// This cache is not persisted and has the same lifetime as the delegate.
// Differs from ReduceAcceptLanguageService by not relying on ContentSettings to
// store ReduceAcceptLanguage Cache.
class InMemoryReduceAcceptLanguageService
    : public content::ReduceAcceptLanguageControllerDelegate {
 public:
  explicit InMemoryReduceAcceptLanguageService(
      const std::vector<std::string>& accept_languages);

  InMemoryReduceAcceptLanguageService(
      const InMemoryReduceAcceptLanguageService&) = delete;
  InMemoryReduceAcceptLanguageService& operator=(
      const InMemoryReduceAcceptLanguageService&) = delete;

  ~InMemoryReduceAcceptLanguageService() override;

  std::optional<std::string> GetReducedLanguage(
      const url::Origin& origin) override;

  std::vector<std::string> GetUserAcceptLanguages() const override;

  void PersistReducedLanguage(const url::Origin& origin,
                              const std::string& language) override;

  void ClearReducedLanguage(const url::Origin& origin) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Stores reduced accept-language cache for an origin.
  std::map<url::Origin, std::string> accept_language_cache_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // This store user's accept language list.
  const std::vector<std::string> user_accept_languages_;
};

}  // namespace reduce_accept_language

#endif  // COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_IN_MEMORY_REDUCE_ACCEPT_LANGUAGE_SERVICE_H_
