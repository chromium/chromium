// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/privacy_sandbox_handler.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace settings {

namespace {

// Keys of the dictionary returned by getFledgeState.
constexpr char kJoiningSites[] = "joiningSites";
constexpr char kBlockedSites[] = "blockedSites";

// Keys of the dictionary of the CanonicalTopic JS type.
constexpr char kTopicId[] = "topicId";
constexpr char kTaxonomyVersion[] = "taxonomyVersion";
constexpr char kDisplayString[] = "displayString";

// Keys of the dictionary returned by getTopicsState.
constexpr char kTopTopics[] = "topTopics";
constexpr char kBlockedTopics[] = "blockedTopics";

base::Value::Dict ConvertTopicToValue(
    const privacy_sandbox::CanonicalTopic& topic) {
  base::Value::Dict topic_value;
  topic_value.Set(kTopicId, topic.topic_id().value());
  topic_value.Set(kTaxonomyVersion, topic.taxonomy_version());
  topic_value.Set(kDisplayString, topic.GetLocalizedRepresentation());
  return topic_value;
}

}  // namespace

PrivacySandboxHandler::PrivacySandboxHandler() = default;
PrivacySandboxHandler::~PrivacySandboxHandler() = default;

void PrivacySandboxHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setFledgeJoiningAllowed",
      base::BindRepeating(&PrivacySandboxHandler::HandleSetFledgeJoiningAllowed,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getFledgeState",
      base::BindRepeating(&PrivacySandboxHandler::HandleGetFledgeState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setTopicAllowed",
      base::BindRepeating(&PrivacySandboxHandler::HandleSetTopicAllowed,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTopicsState",
      base::BindRepeating(&PrivacySandboxHandler::HandleGetTopicsState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "topicsToggleChanged",
      base::BindRepeating(&PrivacySandboxHandler::HandleTopicsToggleChanged,
                          base::Unretained(this)));
}

void PrivacySandboxHandler::HandleSetFledgeJoiningAllowed(
    const base::Value::List& args) {
  const std::string& site = args[0].GetString();
  const bool enabled = args[1].GetBool();
  GetPrivacySandboxService()->SetFledgeJoiningAllowed(site, enabled);
}

void PrivacySandboxHandler::HandleGetFledgeState(
    const base::Value::List& args) {
  AllowJavascript();
  const std::string& callback_id = args[0].GetString();
  GetPrivacySandboxService()->GetFledgeJoiningEtldPlusOneForDisplay(
      base::BindOnce(&PrivacySandboxHandler::OnFledgeJoiningSitesRecieved,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void PrivacySandboxHandler::HandleSetTopicAllowed(
    const base::Value::List& args) {
  const int topic_id = args[0].GetInt();
  const int taxonomy_version = args[1].GetInt();
  const bool allowed = args[2].GetBool();
  GetPrivacySandboxService()->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(topic_id),
                                      taxonomy_version),
      allowed);
}

void PrivacySandboxHandler::HandleGetTopicsState(
    const base::Value::List& args) {
  AllowJavascript();
  base::Value::List top_topics_list;
  for (const auto& topic : GetPrivacySandboxService()->GetCurrentTopTopics())
    top_topics_list.Append(ConvertTopicToValue(topic));

  base::Value::List blocked_topics_list;
  for (const auto& topic : GetPrivacySandboxService()->GetBlockedTopics())
    blocked_topics_list.Append(ConvertTopicToValue(topic));

  base::Value::Dict topics_state;
  topics_state.Set(kTopTopics, std::move(top_topics_list));
  topics_state.Set(kBlockedTopics, std::move(blocked_topics_list));
  ResolveJavascriptCallback(args[0], std::move(topics_state));
}

void PrivacySandboxHandler::HandleTopicsToggleChanged(
    const base::Value::List& args) {
  AllowJavascript();
  const int toggle_value = args[0].GetBool();

  GetPrivacySandboxService()->TopicsToggleChanged(toggle_value);
}

void PrivacySandboxHandler::OnFledgeJoiningSitesRecieved(
    const std::string& callback_id,
    std::vector<std::string> joining_sites) {
  // Combine |joining_sites| with the blocked FLEDGE sites information. The
  // latter is available synchronously.
  base::Value::List joining_sites_list;
  for (const auto& site : joining_sites)
    joining_sites_list.Append(site);

  const auto blocked_sites =
      GetPrivacySandboxService()->GetBlockedFledgeJoiningTopFramesForDisplay();
  base::Value::List blocked_sites_list;
  for (const auto& site : blocked_sites)
    blocked_sites_list.Append(site);

  base::Value::Dict fledge_state;
  fledge_state.Set(kJoiningSites, std::move(joining_sites_list));
  fledge_state.Set(kBlockedSites, std::move(blocked_sites_list));

  ResolveJavascriptCallback(base::Value(callback_id), std::move(fledge_state));
}

PrivacySandboxService* PrivacySandboxHandler::GetPrivacySandboxService() {
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  DCHECK(privacy_sandbox_service);
  return privacy_sandbox_service;
}

void PrivacySandboxHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace settings
