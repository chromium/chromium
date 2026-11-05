// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"

#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace password_manager {

namespace {

std::string_view GetActorTypeHistogramToken(RemoteActorType actor_type) {
  // LINT.IfChange(RemoteActorTypeVariants)
  switch (actor_type) {
    case RemoteActorType::kGemini:
      return "Gemini";
    case RemoteActorType::kSearch:
      return "Search";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/password/histograms.xml:RemoteActorTypeVariants)
  NOTREACHED();
}

}  // namespace

void LogRemoteActorCredentialSharingResult(
    RemoteActorCredentialSharingResult result,
    std::optional<RemoteActorType> actor_type) {
  base::UmaHistogramEnumeration(
      "PasswordManager.RemoteActorCredentialSharing.Result", result);
  if (actor_type.has_value()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"PasswordManager.RemoteActorCredentialSharing.",
                      GetActorTypeHistogramToken(*actor_type), ".Result"}),
        result);
  }
}

}  // namespace password_manager
