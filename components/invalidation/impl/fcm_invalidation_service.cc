// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service.h"

#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidator.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {
const char kApplicationName[] = "com.google.chrome.fcm.invalidations";
}

namespace invalidation {

FCMInvalidationService::FCMInvalidationService(
    IdentityProvider* identity_provider,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service,
    const syncer::ParseJSONCallback& parse_json,
    network::mojom::URLLoaderFactory* loader_factory)
    : invalidator_registrar_(pref_service),
      gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      identity_provider_(identity_provider),
      pref_service_(pref_service),
      parse_json_(parse_json),
      loader_factory_(loader_factory),
      update_was_requested_(false) {}

FCMInvalidationService::~FCMInvalidationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidator_registrar_.UpdateInvalidatorState(
      syncer::INVALIDATOR_SHUTTING_DOWN);
  identity_provider_->RemoveObserver(this);
  if (IsStarted())
    StopInvalidator();
}

void FCMInvalidationService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsReadyToStart())
    StartInvalidator();

  identity_provider_->AddObserver(this);
}

void FCMInvalidationService::InitForTest(syncer::Invalidator* invalidator) {
  // Here we perform the equivalent of Init() and StartInvalidator(), but with
  // some minor changes to account for the fact that we're injecting the
  // invalidator.
  invalidator_.reset(invalidator);

  invalidator_->RegisterHandler(this);
  DoUpdateRegisteredIdsIfNeeded();
}

void FCMInvalidationService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Registering an invalidation handler";
  invalidator_registrar_.RegisterHandler(handler);
  logger_.OnRegistration(handler->GetOwnerName());
}

bool FCMInvalidationService::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_was_requested_ = true;
  DVLOG(2) << "Registering ids: " << ids.size();
  syncer::TopicSet topics = ConvertIdsToTopics(ids);
  if (!invalidator_registrar_.UpdateRegisteredTopics(handler, topics))
    return false;
  DoUpdateRegisteredIdsIfNeeded();
  logger_.OnUpdateTopics(invalidator_registrar_.GetSanitizedHandlersIdsMap());
  return true;
}

void FCMInvalidationService::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Unregistering";
  invalidator_registrar_.UnregisterHandler(handler);
  logger_.OnUnregistration(handler->GetOwnerName());
}

syncer::InvalidatorState FCMInvalidationService::GetInvalidatorState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (invalidator_) {
    DVLOG(2) << "GetInvalidatorState returning "
             << invalidator_->GetInvalidatorState();
    return invalidator_->GetInvalidatorState();
  }
  DVLOG(2) << "Invalidator currently stopped";
  return syncer::TRANSIENT_INVALIDATION_ERROR;
}

std::string FCMInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

InvalidationLogger* FCMInvalidationService::GetInvalidationLogger() {
  return &logger_;
}

void FCMInvalidationService::RequestDetailedStatus(
    base::RepeatingCallback<void(const base::DictionaryValue&)> return_callback)
    const {
  if (IsStarted()) {
    invalidator_->RequestDetailedStatus(return_callback);
  }
}

void FCMInvalidationService::OnActiveAccountLogin() {
  if (!IsStarted() && IsReadyToStart())
    StartInvalidator();
}

void FCMInvalidationService::OnActiveAccountRefreshTokenUpdated() {
  if (!IsStarted() && IsReadyToStart())
    StartInvalidator();
}

void FCMInvalidationService::OnActiveAccountLogout() {
  if (IsStarted()) {
    StopInvalidator();
    if (!client_id_.empty())
      ResetClientID();
  }
}

void FCMInvalidationService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  invalidator_registrar_.UpdateInvalidatorState(state);
  logger_.OnStateChange(state);
}

void FCMInvalidationService::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  invalidator_registrar_.DispatchInvalidationsToHandlers(
      ConvertObjectIdInvalidationMapToTopicInvalidationMap(invalidation_map));

  logger_.OnInvalidation(invalidation_map);
}

std::string FCMInvalidationService::GetOwnerName() const {
  return "FCM";
}

bool FCMInvalidationService::IsReadyToStart() {
  if (!identity_provider_->IsActiveAccountAvailable()) {
    DVLOG(2) << "Not starting FCMInvalidationService: "
             << "active account is not available";
    return false;
  }

  return true;
}

bool FCMInvalidationService::IsStarted() const {
  return invalidator_ != nullptr;
}

void FCMInvalidationService::StartInvalidator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!invalidator_);
  DCHECK(IsReadyToStart());

  PopulateClientID();
  auto network = std::make_unique<syncer::FCMNetworkHandler>(
      gcm_driver_, instance_id_driver_);
  network->StartListening();
  invalidator_ = std::make_unique<syncer::FCMInvalidator>(
      std::move(network), identity_provider_, pref_service_, loader_factory_,
      parse_json_);
  invalidator_->RegisterHandler(this);
  DoUpdateRegisteredIdsIfNeeded();
}

void FCMInvalidationService::StopInvalidator() {
  DCHECK(invalidator_);
  // TODO(melandory): reset the network.
  invalidator_->UnregisterHandler(this);
  invalidator_.reset();
}

void FCMInvalidationService::PopulateClientID() {
  client_id_ = pref_service_->GetString(prefs::kFCMInvalidationClientIDCache);
  instance_id::InstanceID* instance_id =
      instance_id_driver_->GetInstanceID(kApplicationName);
  instance_id->GetID(base::Bind(&FCMInvalidationService::OnInstanceIdRecieved,
                                base::Unretained(this)));
}

void FCMInvalidationService::ResetClientID() {
  instance_id::InstanceID* instance_id =
      instance_id_driver_->GetInstanceID(kApplicationName);
  instance_id->DeleteID(base::Bind(&FCMInvalidationService::OnDeleteIDCompleted,
                                   base::Unretained(this)));
  pref_service_->SetString(prefs::kFCMInvalidationClientIDCache, std::string());
}

void FCMInvalidationService::OnInstanceIdRecieved(const std::string& id) {
  if (client_id_ != id) {
    client_id_ = id;
    pref_service_->SetString(prefs::kFCMInvalidationClientIDCache, id);
  }
  // TODO(melandory): Notify profile sync service that the invalidator
  // id has changed;
}

void FCMInvalidationService::OnDeleteIDCompleted(
    instance_id::InstanceID::Result) {
  // TODO(meandory): report metric in case of unsucesfull deletion.
}

void FCMInvalidationService::DoUpdateRegisteredIdsIfNeeded() {
  if (!invalidator_ || !update_was_requested_)
    return;
  auto registered_ids = invalidator_registrar_.GetAllRegisteredIds();
  CHECK(invalidator_->UpdateRegisteredIds(this, registered_ids));
  update_was_requested_ = false;
}

}  // namespace invalidation
