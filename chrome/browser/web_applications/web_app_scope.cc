// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_scope.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace web_app {

namespace {

enum class ScoreBehavior { kMaximumScore, kExitEarlyPositiveNumberFirstMatch };

int GetScopeExtensionsScore(
    const webapps::AppId& app_id,
    const GURL& url,
    const base::flat_set<ScopeExtensionInfo>& validated_scope_extensions,
    ScoreBehavior behavior) {
  if (validated_scope_extensions.empty()) {
    return 0;
  }

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque() || origin.scheme() != url::kHttpsScheme) {
    return 0;
  }

  int score = 0;
  std::string origin_string = origin.Serialize();
  for (const ScopeExtensionInfo& scope_extension : validated_scope_extensions) {
    CHECK(scope_extension.scope.is_valid());
    if (base::StartsWith(url.spec(), scope_extension.scope.spec(),
                         base::CompareCase::SENSITIVE)) {
      if (behavior == ScoreBehavior::kExitEarlyPositiveNumberFirstMatch) {
        return 1;
      }
      score = std::max(score, base::saturated_cast<int>(
                                  scope_extension.scope.spec().length()));
    }

    // Origins with wildcard e.g. *.foo are saved as https://foo.
    // Ensure while matching that the origin ends with '.foo' and not 'foo'.
    if (scope_extension.has_origin_wildcard) {
      if (base::EndsWith(origin_string, scope_extension.origin.host(),
                         base::CompareCase::SENSITIVE) &&
          origin_string.size() > scope_extension.origin.host().size() &&
          origin_string[origin_string.size() -
                        scope_extension.origin.host().size() - 1] == '.') {
        if (behavior == ScoreBehavior::kExitEarlyPositiveNumberFirstMatch) {
          return 1;
        }
        score = std::max(score, base::saturated_cast<int>(
                                    scope_extension.scope.spec().length()));
      }
    }
  }
  return score;
}

}  // namespace

WebAppScope::WebAppScope(
    const webapps::AppId& app_id,
    const GURL& scope,
    const base::flat_set<ScopeExtensionInfo>& validated_scope_extensions,
    base::PassKey<WebApp>)
    : app_id_(app_id),
      scope_(scope),
      validated_scope_extensions_(validated_scope_extensions) {
  CHECK(scope_.is_valid());
  CHECK(!app_id_.empty());
}

WebAppScope::~WebAppScope() = default;
WebAppScope::WebAppScope(const WebAppScope&) = default;
WebAppScope::WebAppScope(WebAppScope&&) = default;
WebAppScope& WebAppScope::operator=(const WebAppScope&) = default;
WebAppScope& WebAppScope::operator=(WebAppScope&&) = default;

bool WebAppScope::IsInScope(const GURL& url, WebAppScopeOptions options) const {
  if (!url.is_valid()) {
    return false;
  }

  // DIY apps installed by the user on http sites can have their urls upgraded
  // to https later. While the app is still tied to the http origin, it's nice
  // that the content is now https, and we should consider the https urls
  // in-scope.
  bool origin_matches = url::IsSameOriginWith(scope_, url);
  if (!origin_matches && options.allow_http_to_https_upgrade &&
      scope_.GetScheme() == url::kHttpScheme &&
      url.GetScheme() == url::kHttpsScheme) {
    GURL::Replacements rep;
    rep.SetSchemeStr(url::kHttpsScheme);
    GURL secure_scope = scope_.ReplaceComponents(rep);
    if (url::IsSameOriginWith(secure_scope, url)) {
      origin_matches = true;
    }
  }

  if (origin_matches) {
    // For scopes without paths, return 'true' early (allowing blobs to be in
    // scope).
    if (!scope_.has_path() || scope_.GetPath() == "/") {
      return true;
    }
    if (url.GetScheme() == url::kBlobScheme) {
      // Same-origin blobs can only be in-scope in the above case where the
      // app scope doesn't have a path.
      return false;
    }
    if (base::StartsWith(url.GetPath(), scope_.GetPath(),
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsUploadOfficeToCloudEnabled() &&
      ChromeOsWebAppExperiments::GetExtendedScopeScore(app_id_, url.spec()) >
          0) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Check scope extensions.
  return GetScopeExtensionsScore(
             app_id_, url, validated_scope_extensions_,
             ScoreBehavior::kExitEarlyPositiveNumberFirstMatch) > 0;
}

int WebAppScope::GetScopeScore(const GURL& url,
                               WebAppScopeScoreOptions options) const {
  if (!url.is_valid()) {
    return 0;
  }
  // Note: This code does not do same-origin checks to be compabible with urls
  // like about:blank or blobs, unlike in the IsInScope method above. We could
  // change this in the future if needed.
  int score = 0;
  if (base::StartsWith(url.spec(), scope_.spec(),
                       base::CompareCase::SENSITIVE)) {
    score = base::saturated_cast<int>(scope_.spec().length());
    // A regular scope match is always better than an extended scope match.
    // Add a large constant to the score to ensure this, as the score is
    // simply the length of the scope string, which is capped at
    // url::kMaxURLChars.
    score = base::ClampAdd(score, url::kMaxURLChars);
  }

  // Note: This is considered whether or not extensions are excluded due to
  // historical reasons.
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
    score = std::max(score, ChromeOsWebAppExperiments::GetExtendedScopeScore(
                                app_id_, url.spec()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (options.exclude_scope_extensions) {
    return score;
  }

  return std::max(
      score, GetScopeExtensionsScore(app_id_, url, validated_scope_extensions_,
                                     ScoreBehavior::kMaximumScore));
}

bool WebAppScope::operator==(const WebAppScope& other) const {
  return scope_ == other.scope_ &&
         validated_scope_extensions_ == other.validated_scope_extensions_;
}

}  // namespace web_app
