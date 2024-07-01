// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reduce_accept_language/browser/in_memory_reduce_accept_language_service.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace reduce_accept_language {

InMemoryReduceAcceptLanguageService::InMemoryReduceAcceptLanguageService(
    const std::vector<std::string>& accept_languages)
    : user_accept_languages_(accept_languages) {}

InMemoryReduceAcceptLanguageService::~InMemoryReduceAcceptLanguageService() =
    default;

std::optional<std::string>
InMemoryReduceAcceptLanguageService::GetReducedLanguage(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const GURL& url = origin.GetURL();

  // Only reduce accept-language in http and https scheme.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }

  const auto& it = accept_language_cache_.find(origin);
  if (it != accept_language_cache_.end()) {
    return std::make_optional(it->second);
  }
  return std::nullopt;
}

std::vector<std::string>
InMemoryReduceAcceptLanguageService::GetUserAcceptLanguages() const {
  return user_accept_languages_;
}

void InMemoryReduceAcceptLanguageService::PersistReducedLanguage(
    const url::Origin& origin,
    const std::string& language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const GURL url = origin.GetURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  accept_language_cache_[origin] = language;
}

void InMemoryReduceAcceptLanguageService::ClearReducedLanguage(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const GURL& url = origin.GetURL();

  // Only reduce accept-language in http and https scheme.
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  accept_language_cache_.erase(origin);
}

}  // namespace reduce_accept_language
