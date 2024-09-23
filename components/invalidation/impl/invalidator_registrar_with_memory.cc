// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace invalidation {

namespace {

constexpr char kTopicsToHandler[] = "invalidation.per_sender_topics_to_handler";
constexpr char kDeprecatedSyncInvalidationGCMSenderId[] = "8181035976";

constexpr char kHandler[] = "handler";
constexpr char kIsPublic[] = "is_public";

std::string DumpRegisteredHandlers(
    const base::ObserverList<InvalidationHandler, true>& handlers) {
  if (handlers.empty()) {
    return "empty";
  }

  std::vector<std::string> handler_names;
  for (const auto& handler : handlers) {
    handler_names.emplace_back(handler.GetOwnerName());
  }

  return base::JoinString(handler_names, ",");
}

std::string DumpRegisteredHandlersToTopics(
    const std::map<InvalidationHandler*, TopicMap, std::less<>>&
        registered_handler_to_topics_map) {
  if (registered_handler_to_topics_map.empty()) {
    return "empty";
  }

  std::vector<std::string> handler_names;
  for (const auto& [handler, topics] : registered_handler_to_topics_map) {
    handler_names.emplace_back(handler->GetOwnerName());
  }

  return base::JoinString(handler_names, ",");
}

}  // namespace

BASE_FEATURE(kRestoreInterestingTopicsFeature,
             "InvalidatorRestoreInterestingTopics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// static
void InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTopicsToHandler);
}

// static
void InvalidatorRegistrarWithMemory::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // For local state, we want to register exactly the same prefs as for profile
  // prefs; see comment in the header.
  RegisterProfilePrefs(registry);
}

// static
void InvalidatorRegistrarWithMemory::ClearDeprecatedPrefs(PrefService* prefs) {
  if (prefs->HasPrefPath(kTopicsToHandler)) {
    ScopedDictPrefUpdate update(prefs, kTopicsToHandler);
    update->Remove(kDeprecatedSyncInvalidationGCMSenderId);
  }
}

InvalidatorRegistrarWithMemory::InvalidatorRegistrarWithMemory(
    PrefService* prefs,
    const std::string& sender_id)
    : state_(InvalidatorState::kDisabled),
      prefs_(prefs),
      sender_id_(sender_id) {
  CHECK(!sender_id_.empty());
  const base::Value::Dict* pref_data =
      prefs_->GetDict(kTopicsToHandler).FindDict(sender_id_);
  if (!pref_data) {
    ScopedDictPrefUpdate update(prefs_, kTopicsToHandler);
    update->Set(sender_id_, base::Value::Dict());
    return;
  }
  // Restore |handler_name_to_subscribed_topics_map_| from prefs.
  if (!base::FeatureList::IsEnabled(kRestoreInterestingTopicsFeature))
    return;
  for (auto it : *pref_data) {
    const std::string& topic_name = it.first;
    if (it.second.is_dict()) {
      const base::Value::Dict& second_dict = it.second.GetDict();
      const std::string* handler = second_dict.FindString(kHandler);
      const std::optional<bool> is_public = second_dict.FindBool(kIsPublic);
      if (!handler || !is_public) {
        continue;
      }
      handler_name_to_subscribed_topics_map_[*handler][topic_name] =
          TopicMetadata{.is_public = *is_public};
    } else if (it.second.is_string()) {
      handler_name_to_subscribed_topics_map_[it.second.GetString()]
                                            [topic_name] = TopicMetadata{
                                                .is_public = false};
    }
  }
}

InvalidatorRegistrarWithMemory::~InvalidatorRegistrarWithMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!registered_handler_to_topics_map_.empty() || !handlers_.empty()) {
    // TODO(crbug.com/40070281) figure out with these logs.
    // Note: This can't be just a `CHECK(...) << ...` because `CHECK` eats the
    // message in production builds.
    LOG(ERROR) << "Registered handlers during destruction: "
               << DumpRegisteredHandlers(handlers_)
               << ". Handlers listening to topics: "
               << DumpRegisteredHandlersToTopics(
                      registered_handler_to_topics_map_)
               << ".";
    NOTREACHED();
  }
}

void InvalidatorRegistrarWithMemory::AddObserver(InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(handler);
  CHECK(!handlers_.HasObserver(handler));
  handlers_.AddObserver(handler);
}

bool InvalidatorRegistrarWithMemory::HasObserver(
    const InvalidationHandler* handler) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handlers_.HasObserver(handler);
}

void InvalidatorRegistrarWithMemory::RemoveObserver(
    const InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));
  handlers_.RemoveObserver(handler);
  if (const auto it = registered_handler_to_topics_map_.find(handler);
      it != registered_handler_to_topics_map_.end()) {
    registered_handler_to_topics_map_.erase(it);
  }
  // Note: Do *not* remove the entry from
  // |handler_name_to_subscribed_topics_map_| - we haven't actually unsubscribed
  // from any of the topics on the server, so GetAllSubscribedTopics() should
  // still return the topics.
}

bool InvalidatorRegistrarWithMemory::UpdateRegisteredTopics(
    InvalidationHandler* handler,
    const TopicMap& topics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));

  if (HasDuplicateTopicRegistration(handler, topics)) {
    return false;
  }

  TopicMap old_topics = registered_handler_to_topics_map_[handler];
  if (topics.empty()) {
    registered_handler_to_topics_map_.erase(handler);
  } else {
    registered_handler_to_topics_map_[handler] = topics;
  }

  // This does *not* remove subscribed topics which are not registered. This
  // behaviour is used by some handlers to keep topic subscriptions after
  // browser startup even if they are not included in the first call of this
  // method. It's useful to prevent unsubscribing from and subscribing to the
  // topics on each browser startup.
  //
  // TODO(crbug.com/40674001): make the unsubscription behaviour consistent
  // regardless of browser restart in between.

  ScopedDictPrefUpdate update(prefs_, kTopicsToHandler);
  base::Value::Dict* pref_data = update->FindDict(sender_id_);
  CHECK(pref_data);
  auto& topic_map =
      handler_name_to_subscribed_topics_map_[handler->GetOwnerName()];

  for (const auto& [topic, meta_data] : old_topics) {
    pref_data->Remove(topic);
    topic_map.erase(topic);
  }

  for (const auto& [topic, meta_data] : topics) {
    topic_map[topic] = meta_data;
    pref_data->Set(topic, base::Value::Dict()
                              .Set(kHandler, handler->GetOwnerName())
                              .Set(kIsPublic, meta_data.is_public));
  }
  return true;
}

TopicMap InvalidatorRegistrarWithMemory::GetRegisteredTopics(
    InvalidationHandler* handler) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lookup = registered_handler_to_topics_map_.find(handler);
  return lookup != registered_handler_to_topics_map_.end() ? lookup->second
                                                           : TopicMap();
}

TopicMap InvalidatorRegistrarWithMemory::GetAllSubscribedTopics() const {
  TopicMap subscribed_topics;
  for (const auto& handler_to_topic : handler_name_to_subscribed_topics_map_) {
    subscribed_topics.insert(handler_to_topic.second.begin(),
                             handler_to_topic.second.end());
  }
  return subscribed_topics;
}

std::optional<Invalidation>
InvalidatorRegistrarWithMemory::DispatchInvalidationToHandlers(
    const Invalidation& invalidation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Each handler has a set of registered topics. In order to send the incoming
  // invalidation to the correct handlers we are going through each handler and
  // each of their sets of topics.
  for (const auto& [handler, registered_topics] :
       registered_handler_to_topics_map_) {
    for (const auto& [registered_topic, meta_data] : registered_topics) {
      // If the topic of the invalidation matches a registered topic, we send
      // the invalidation to the respective handler.
      if (invalidation.topic() != registered_topic) {
        continue;
      }
      handler->OnIncomingInvalidation(invalidation);
      return std::nullopt;
    }
  }
  return invalidation;
}

void InvalidatorRegistrarWithMemory::DispatchSuccessfullySubscribedToHandlers(
    const Topic& topic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we have no handlers, there's nothing to do.
  if (handlers_.empty()) {
    return;
  }

  for (const auto& [handler, registered_topics] :
       registered_handler_to_topics_map_) {
    for (const auto& [registered_topic, meta_data] : registered_topics) {
      if (topic != registered_topic) {
        continue;
      }
      handler->OnSuccessfullySubscribed(topic);
    }
  }
}

void InvalidatorRegistrarWithMemory::UpdateInvalidatorState(
    InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "New invalidator state: " << InvalidatorStateToString(state_)
           << " -> " << InvalidatorStateToString(state);
  state_ = state;
  for (auto& observer : handlers_)
    observer.OnInvalidatorStateChange(state);
}

InvalidatorState InvalidatorRegistrarWithMemory::GetInvalidatorState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

bool InvalidatorRegistrarWithMemory::HasDuplicateTopicRegistration(
    InvalidationHandler* handler,
    const TopicMap& new_topics) const {
  for (const auto& [registered_handler, handler_topics] :
       registered_handler_to_topics_map_) {
    if (registered_handler == handler) {
      continue;
    }

    for (const auto& [new_topic, meta_data] : new_topics) {
      if (handler_topics.contains(new_topic)) {
        DVLOG(1) << "Duplicate registration: trying to register " << new_topic
                 << " for " << handler->GetOwnerName()
                 << " when it's already registered for "
                 << registered_handler->GetOwnerName();
        return true;
      }
    }
  }
  return false;
}

}  // namespace invalidation
