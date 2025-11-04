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
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/variations_client.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

std::unique_ptr<ContextualSearchSessionHandle>
ContextualSearchService::CreateSession(
    std::unique_ptr<ContextualSearchContextController::ConfigParams>
        query_controller_config_params,
    const std::string& contextual_search_metric_name) {
  base::UnguessableToken session_id = base::UnguessableToken::Create();
  auto controller = std::make_unique<ComposeboxQueryController>(
      identity_manager_, url_loader_factory_, channel_, locale_,
      template_url_service_, variations_client_,
      std::move(query_controller_config_params));
  auto recorder = std::make_unique<ContextualSearchMetricsRecorder>(
      contextual_search_metric_name);
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

std::string ContextualSearchService::GetSessionMetricsRecorderName(
    const SessionId& session_id) const {
  if (auto it = sessions_.find(session_id); it != sessions_.end()) {
    if (auto metrics_recorder = it->second.metrics_recorder_.get()) {
      return metrics_recorder->GetMetricsRecorderName();
    }
  }
  return "";
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
