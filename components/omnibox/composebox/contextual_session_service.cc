// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/contextual_session_service.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/variations_client.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// ContextualSessionService ----------------------------------------------------

ContextualSessionService::ContextualSessionService(
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

ContextualSessionService::~ContextualSessionService() = default;

std::unique_ptr<ContextualSessionService::SessionHandle>
ContextualSessionService::CreateSession(
    std::unique_ptr<ComposeboxQueryController::QueryControllerConfigParams>
        query_controller_config_params) {
  base::UnguessableToken session_id = base::UnguessableToken::Create();
  auto controller = std::make_unique<ComposeboxQueryController>(
      identity_manager_, url_loader_factory_, channel_, locale_,
      template_url_service_, variations_client_,
      std::move(query_controller_config_params));
  sessions_.emplace(session_id, SessionEntry(std::move(controller)));

  return base::WrapUnique(
      new SessionHandle(weak_ptr_factory_.GetWeakPtr(), session_id));
}

std::unique_ptr<ContextualSessionService::SessionHandle>
ContextualSessionService::GetSession(const base::UnguessableToken& session_id) {
  if (auto it = sessions_.find(session_id); it != sessions_.end()) {
    it->second.ref_count_++;
    return base::WrapUnique(
        new SessionHandle(weak_ptr_factory_.GetWeakPtr(), session_id));
  }
  return nullptr;
}

std::unique_ptr<ContextualSessionService::SessionHandle>
ContextualSessionService::CreateSessionForTesting(
    std::unique_ptr<ComposeboxQueryController> controller) {
  base::UnguessableToken session_id = base::UnguessableToken::Create();
  sessions_.emplace(session_id, SessionEntry(std::move(controller)));

  return base::WrapUnique(
      new SessionHandle(weak_ptr_factory_.GetWeakPtr(), session_id));
}

ComposeboxQueryController* ContextualSessionService::GetSessionController(
    const base::UnguessableToken& session_id) {
  if (auto it = sessions_.find(session_id); it != sessions_.end()) {
    return it->second.controller_.get();
  }
  return nullptr;
}

void ContextualSessionService::ReleaseSession(
    const base::UnguessableToken& session_id) {
  if (auto it = sessions_.find(session_id); it != sessions_.end()) {
    it->second.ref_count_--;
    if (it->second.ref_count_ == 0) {
      sessions_.erase(it);
    }
  }
}

// ContextualSessionService::SessionHandle -------------------------------------

ContextualSessionService::SessionHandle::SessionHandle(
    base::WeakPtr<ContextualSessionService> service,
    const base::UnguessableToken& session_id)
    : service_(service), session_id_(session_id) {}

ContextualSessionService::SessionHandle::~SessionHandle() {
  if (service_) {
    service_->ReleaseSession(session_id_);
  }
}

ComposeboxQueryController*
ContextualSessionService::SessionHandle::GetController() const {
  return service_ ? service_->GetSessionController(session_id_) : nullptr;
}

// ContextualSessionService::SessionEntry --------------------------------------

ContextualSessionService::SessionEntry::SessionEntry(SessionEntry&&) = default;
ContextualSessionService::SessionEntry&
ContextualSessionService::SessionEntry::operator=(SessionEntry&&) = default;

ContextualSessionService::SessionEntry::SessionEntry(
    std::unique_ptr<ComposeboxQueryController> controller)
    : controller_(std::move(controller)) {}

ContextualSessionService::SessionEntry::~SessionEntry() = default;
