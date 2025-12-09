// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_service.h"

#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_session_entry.h"
#include "components/contextual_search/internal/composebox_query_controller.h"
#include "components/contextual_search/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/variations_client.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Reflects the default value for the `kSearchContentSharingSettings` pref.
// Pref value is determined by:
// `SearchContentSharingSettings` policy, `GenAiDefaultSettings` policy if
// `SearchContentSharingSettings` isn't set, or the default pref value (0) if
// neither policy is set. Do not change this value without migrating the
// existing prefs and the policy's prefs mapping.
constexpr contextual_search::SearchContentSharingSettingsValue
    kSearchContentSharingAllowedDefault =
        contextual_search::SearchContentSharingSettingsValue::kEnabled;

}  // namespace

namespace contextual_search {

ContextualSearchService::ContextualSearchService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TemplateURLService* template_url_service,
    variations::VariationsClient* variations_client,
    version_info::Channel channel,
    const std::string& locale)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      template_url_service_(template_url_service),
      variations_client_(variations_client),
      channel_(channel),
      locale_(locale) {}

ContextualSearchService::~ContextualSearchService() = default;

// static
void ContextualSearchService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kSearchContentSharingSettings,
      static_cast<int>(kSearchContentSharingAllowedDefault));
}

// static
bool ContextualSearchService::IsContextSharingEnabled(
    const PrefService* prefs) {
  return prefs->GetInteger(kSearchContentSharingSettings) ==
         static_cast<int>(kSearchContentSharingAllowedDefault);
}

std::unique_ptr<ContextualSearchContextController>
ContextualSearchService::CreateComposeboxQueryController(
    std::unique_ptr<ContextualSearchContextController::ConfigParams>
        query_controller_config_params) {
  return std::make_unique<ComposeboxQueryController>(
      identity_manager_, url_loader_factory_, channel_, locale_,
      template_url_service_, variations_client_,
      std::move(query_controller_config_params));
}

std::unique_ptr<ContextualSearchSessionHandle>
ContextualSearchService::CreateSession(
    std::unique_ptr<ContextualSearchContextController::ConfigParams>
        query_controller_config_params,
    ContextualSearchSource source) {
  base::UnguessableToken session_id = base::UnguessableToken::Create();
  std::unique_ptr<ContextualSearchContextController> controller =
      CreateComposeboxQueryController(
          std::move(query_controller_config_params));
  auto recorder = std::make_unique<ContextualSearchMetricsRecorder>(source);
  sessions_.emplace(
      session_id,
      ContextualSearchSessionEntry(std::move(controller), std::move(recorder)));

  return base::WrapUnique(new ContextualSearchSessionHandle(
      weak_ptr_factory_.GetWeakPtr(), session_id));
}

std::unique_ptr<ContextualSearchSessionHandle>
ContextualSearchService::GetSession(const base::UnguessableToken& session_id) {
  if (auto it = sessions_.find(session_id); it != sessions_.end()) {
    it->second.ref_count_++;
    return base::WrapUnique(new ContextualSearchSessionHandle(
        weak_ptr_factory_.GetWeakPtr(), session_id));
  }
  return nullptr;
}

std::unique_ptr<ContextualSearchSessionHandle>
ContextualSearchService::CreateSessionForTesting(
    std::unique_ptr<ContextualSearchContextController> controller,
    std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder) {
  base::UnguessableToken session_id = base::UnguessableToken::Create();
  sessions_.emplace(session_id,
                    ContextualSearchSessionEntry(std::move(controller),
                                                 std::move(metrics_recorder)));

  return base::WrapUnique(new ContextualSearchSessionHandle(
      weak_ptr_factory_.GetWeakPtr(), session_id));
}

ContextualSearchContextController*
ContextualSearchService::GetSessionController(
    const base::UnguessableToken& session_id) {
  if (auto it = sessions_.find(session_id); it != sessions_.end()) {
    return it->second.controller_.get();
  }
  return nullptr;
}

ContextualSearchMetricsRecorder*
ContextualSearchService::GetSessionMetricsRecorder(
    const base::UnguessableToken& session_id) {
  if (auto it = sessions_.find(session_id); it != sessions_.end()) {
    return it->second.metrics_recorder_.get();
  }
  return nullptr;
}

void ContextualSearchService::ReleaseSession(
    const base::UnguessableToken& session_id) {
  if (auto it = sessions_.find(session_id); it != sessions_.end()) {
    it->second.ref_count_--;
    if (it->second.ref_count_ == 0) {
      sessions_.erase(it);
    }
  }
}

}  // namespace contextual_search
