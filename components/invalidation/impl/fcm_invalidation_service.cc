// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service.h"

#include <set>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace invalidation {
namespace {
constexpr char kApplicationName[] = "com.google.chrome.fcm.invalidations";
constexpr char kDisableFcmInvalidationsSwitch[] = "disable-fcm-invalidations";
constexpr char kDeprecatedSyncInvalidationGCMSenderId[] = "8181035976";
}  // namespace

FCMInvalidationService::FCMInvalidationService(
    IdentityProvider* identity_provider,
    FCMNetworkHandlerCallback fcm_network_handler_callback,
    FCMInvalidationListenerCallback fcm_invalidation_listener_callback,
    PerUserTopicSubscriptionManagerCallback
        per_user_topic_subscription_manager_callback,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service,
    const std::string& sender_id)
    : sender_id_(sender_id),
      invalidator_registrar_(pref_service, sender_id_),
      fcm_network_handler_callback_(std::move(fcm_network_handler_callback)),
      fcm_invalidation_listener_callback_(
          std::move(fcm_invalidation_listener_callback)),
      per_user_topic_subscription_manager_callback_(
          std::move(per_user_topic_subscription_manager_callback)),
      instance_id_driver_(instance_id_driver),
      pref_service_(pref_service),
      identity_provider_(identity_provider) {
  CHECK(!sender_id_.empty());
}

FCMInvalidationService::~FCMInvalidationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsStarted()) {
    StopInvalidator();
  }

  identity_provider_->RemoveObserver(this);
}

void FCMInvalidationService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsReadyToStart()) {
    StartInvalidator();
  }

  identity_provider_->AddObserver(this);
}

// static
void FCMInvalidationService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kInvalidationClientIDCache);
}

// static
void FCMInvalidationService::ClearDeprecatedPrefs(PrefService* prefs) {
  if (prefs->HasPrefPath(prefs::kInvalidationClientIDCache)) {
    ScopedDictPrefUpdate update(prefs, prefs::kInvalidationClientIDCache);
    update->Remove(kDeprecatedSyncInvalidationGCMSenderId);
  }
}

void FCMInvalidationService::AddObserver(InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Registering an invalidation handler";
  invalidator_registrar_.AddObserver(handler);
}

bool FCMInvalidationService::HasObserver(
    const InvalidationHandler* handler) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return invalidator_registrar_.HasObserver(handler);
}

bool FCMInvalidationService::UpdateInterestedTopics(
    InvalidationHandler* handler,
    const TopicSet& topic_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_was_requested_ = true;
  DVLOG(2) << "Subscribing to topics: " << topic_set.size();
  TopicMap topic_map;
  for (const auto& topic_name : topic_set) {
    topic_map[topic_name] = TopicMetadata(handler->IsPublicTopic(topic_name));
  }
  // TODO(crbug.com/40675708): UpdateRegisteredTopics() should be renamed to
  // clarify that it actually updates whether topics need subscription (aka
  // interested).
  if (!invalidator_registrar_.UpdateRegisteredTopics(handler, topic_map)) {
    return false;
  }
  DoUpdateSubscribedTopicsIfNeeded();
  return true;
}

void FCMInvalidationService::RemoveObserver(
    const InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Unregistering";
  invalidator_registrar_.RemoveObserver(handler);
}

InvalidatorState FCMInvalidationService::GetInvalidatorState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (invalidation_listener_) {
    DVLOG(2) << "GetInvalidatorState returning "
             << InvalidatorStateToString(
                    invalidator_registrar_.GetInvalidatorState());
    return invalidator_registrar_.GetInvalidatorState();
  }
  DVLOG(2) << "Invalidator currently stopped";
  return InvalidatorState::kDisabled;
}

std::string FCMInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

void FCMInvalidationService::OnActiveAccountLogin() {
  if (IsStarted()) {
    return;
  }
  if (IsReadyToStart()) {
    StartInvalidator();
  }
}

void FCMInvalidationService::OnActiveAccountRefreshTokenUpdated() {
  if (!IsStarted() && IsReadyToStart()) {
    StartInvalidator();
  }
}

void FCMInvalidationService::OnActiveAccountLogout() {
  if (IsStarted()) {
    StopInvalidatorPermanently();
  }
}

std::optional<Invalidation> FCMInvalidationService::OnInvalidate(
    const Invalidation& invalidation) {
  return invalidator_registrar_.DispatchInvalidationToHandlers(invalidation);
}

void FCMInvalidationService::OnInvalidatorStateChange(InvalidatorState state) {
  invalidator_registrar_.UpdateInvalidatorState(state);
}

void FCMInvalidationService::OnSuccessfullySubscribed(const Topic& topic) {
  invalidator_registrar_.DispatchSuccessfullySubscribedToHandlers(topic);
}

bool FCMInvalidationService::IsStarted() const {
  return invalidation_listener_ != nullptr;
}

bool FCMInvalidationService::IsReadyToStart() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(kDisableFcmInvalidationsSwitch)) {
    DLOG(WARNING) << "FCM Invalidations are disabled via switch";
    return false;
  }

  bool valid_account_info_available =
      identity_provider_->IsActiveAccountWithRefreshToken();

#if BUILDFLAG(IS_ANDROID)
  // IsReadyToStart checks if account is available (active account logged in
  // and token is available). As currently observed, FCMInvalidationService
  // isn't always notified on Android when token is available.
  valid_account_info_available =
      !identity_provider_->GetActiveAccountId().empty();
#endif

  if (!valid_account_info_available) {
    DVLOG(2) << "Not starting FCMInvalidationService: "
             << "active account is not available";
    return false;
  }

  return true;
}

void FCMInvalidationService::StartInvalidator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!invalidation_listener_);

  auto network =
      fcm_network_handler_callback_.Run(sender_id_, GetApplicationName());
  // The order of calls is important. Do not change.
  // We should start listening before requesting the id, because
  // valid id is only generated, once there is an app handler
  // for the app. StartListening registers the app handler.
  // We should create InvalidationListener first, because it registers the
  // handler for the incoming messages, which is crucial on Android, because on
  // the startup cached messages might exists.
  invalidation_listener_ =
      fcm_invalidation_listener_callback_.Run(std::move(network));
  auto subscription_manager = per_user_topic_subscription_manager_callback_.Run(
      identity_provider_, pref_service_, sender_id_);
  invalidation_listener_->Start(this, std::move(subscription_manager));

  PopulateClientID();
  DoUpdateSubscribedTopicsIfNeeded();
}

void FCMInvalidationService::StopInvalidator() {
  DCHECK(invalidation_listener_);
  invalidation_listener_.reset();
}

void FCMInvalidationService::StopInvalidatorPermanently() {
  // Reset the client ID (aka InstanceID) *before* stopping, so that
  // FCMInvalidationListener gets notified about the cleared ID (the listener
  // gets destroyed during StopInvalidator()).
  if (!client_id_.empty()) {
    ResetClientID();
  }

  StopInvalidator();
}

void FCMInvalidationService::PopulateClientID() {
  // Retrieve any client ID (aka Instance ID) from a previous run, which was
  // cached in prefs.
  const std::string* client_id_pref =
      pref_service_->GetDict(prefs::kInvalidationClientIDCache)
          .FindString(sender_id_);
  client_id_ = client_id_pref ? *client_id_pref : "";

  // Also retrieve a fresh (or validated) client ID. If the |client_id_| just
  // retrieved from prefs is non-empty, then the fresh/validated one will
  // typically be equal to it, but it's not completely guaranteed. OTOH, if
  // |client_id_| is empty, i.e. we didn't have one previously, then this will
  // generate/retrieve a new one.
  instance_id::InstanceID* instance_id =
      instance_id_driver_->GetInstanceID(GetApplicationName());
  instance_id->GetID(base::BindOnce(
      &FCMInvalidationService::OnInstanceIDReceived, base::Unretained(this)));
}

void FCMInvalidationService::ResetClientID() {
  instance_id::InstanceID* instance_id =
      instance_id_driver_->GetInstanceID(GetApplicationName());
  instance_id->DeleteID(
      base::BindOnce(&FCMInvalidationService::OnDeleteInstanceIDCompleted,
                     base::Unretained(this)));

  // Immediately clear our cached values (before we get confirmation of the
  // deletion), since they shouldn't be used anymore. Lower layers are the
  // source of truth, and are responsible for ensuring that the deletion
  // actually happens.
  client_id_.clear();
  ScopedDictPrefUpdate update(pref_service_, prefs::kInvalidationClientIDCache);
  update->Remove(sender_id_);

  // This will also delete all Instance ID *tokens*; we need to let the
  // FCMInvalidationListener know.
  if (invalidation_listener_) {
    invalidation_listener_->ClearInstanceIDToken();
  }
}

void FCMInvalidationService::OnInstanceIDReceived(
    const std::string& instance_id) {
  if (client_id_ != instance_id) {
    client_id_ = instance_id;
    ScopedDictPrefUpdate update(pref_service_,
                                prefs::kInvalidationClientIDCache);
    update->Set(sender_id_, instance_id);
  }
}

void FCMInvalidationService::OnDeleteInstanceIDCompleted(
    instance_id::InstanceID::Result) {
  // Note: |client_id_| and the pref were already cleared when we initiated the
  // deletion.
}

void FCMInvalidationService::DoUpdateSubscribedTopicsIfNeeded() {
  if (!invalidation_listener_ || !update_was_requested_) {
    return;
  }
  auto subscribed_topics = invalidator_registrar_.GetAllSubscribedTopics();
  invalidation_listener_->UpdateInterestedTopics(subscribed_topics);
  update_was_requested_ = false;
}

const std::string FCMInvalidationService::GetApplicationName() {
  return base::StrCat({kApplicationName, "-", sender_id_});
}

}  // namespace invalidation
