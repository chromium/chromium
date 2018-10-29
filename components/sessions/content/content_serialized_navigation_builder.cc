// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_builder.h"

#include "base/logging.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/sessions/content/content_serialized_navigation_driver.h"
#include "components/sessions/content/extended_info_handler.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/replaced_navigation_entry_data.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"

namespace sessions {
namespace {

base::Optional<SerializedNavigationEntry::ReplacedNavigationEntryData>
ConvertReplacedEntryData(
    const base::Optional<content::ReplacedNavigationEntryData>& input_data) {
  if (!input_data.has_value())
    return base::nullopt;

  SerializedNavigationEntry::ReplacedNavigationEntryData output_data;
  output_data.first_committed_url = input_data->first_committed_url;
  output_data.first_timestamp = input_data->first_timestamp;
  output_data.first_transition_type = input_data->first_transition_type;
  return output_data;
}

}  // namespace

// static
SerializedNavigationEntry
ContentSerializedNavigationBuilder::FromNavigationEntry(
    int index,
    const content::NavigationEntry& entry,
    SerializationOptions serialization_options) {
  SerializedNavigationEntry navigation;
  navigation.index_ = index;
  navigation.unique_id_ = entry.GetUniqueID();
  navigation.referrer_url_ = entry.GetReferrer().url;
  navigation.referrer_policy_ = static_cast<int>(entry.GetReferrer().policy);
  navigation.virtual_url_ = entry.GetVirtualURL();
  navigation.title_ = entry.GetTitle();
  if (!(serialization_options & SerializationOptions::EXCLUDE_PAGE_STATE))
    navigation.encoded_page_state_ = entry.GetPageState().ToEncodedData();
  navigation.transition_type_ = entry.GetTransitionType();
  navigation.has_post_data_ = entry.GetHasPostData();
  navigation.post_id_ = entry.GetPostID();
  navigation.original_request_url_ = entry.GetOriginalRequestURL();
  navigation.is_overriding_user_agent_ = entry.GetIsOverridingUserAgent();
  navigation.timestamp_ = entry.GetTimestamp();
  navigation.is_restored_ = entry.IsRestored();
  if (entry.GetFavicon().valid)
    navigation.favicon_url_ = entry.GetFavicon().url;
  navigation.http_status_code_ = entry.GetHttpStatusCode();
  navigation.redirect_chain_ = entry.GetRedirectChain();
  navigation.replaced_entry_data_ =
      ConvertReplacedEntryData(entry.GetReplacedEntryData());
  navigation.password_state_ = GetPasswordStateFromNavigation(entry);

  for (const auto& handler_entry :
       ContentSerializedNavigationDriver::GetInstance()
           ->GetAllExtendedInfoHandlers()) {
    ExtendedInfoHandler* handler = handler_entry.second.get();
    DCHECK(handler);
    std::string value = handler->GetExtendedInfo(entry);
    if (!value.empty())
      navigation.extended_info_map_[handler_entry.first] = value;
  }

  return navigation;
}

// static
std::unique_ptr<content::NavigationEntry>
ContentSerializedNavigationBuilder::ToNavigationEntry(
    const SerializedNavigationEntry* navigation,
    content::BrowserContext* browser_context) {
  network::mojom::ReferrerPolicy policy =
      static_cast<network::mojom::ReferrerPolicy>(navigation->referrer_policy_);
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationController::CreateNavigationEntry(
          navigation->virtual_url_,
          content::Referrer::SanitizeForRequest(
              navigation->virtual_url_,
              content::Referrer(navigation->referrer_url_, policy)),
          // Use a transition type of reload so that we don't incorrectly
          // increase the typed count.
          ui::PAGE_TRANSITION_RELOAD, false,
          // The extra headers are not sync'ed across sessions.
          std::string(), browser_context,
          nullptr /* blob_url_loader_factory */));

  entry->SetTitle(navigation->title_);
  entry->SetPageState(content::PageState::CreateFromEncodedData(
      navigation->encoded_page_state_));
  entry->SetHasPostData(navigation->has_post_data_);
  entry->SetPostID(navigation->post_id_);
  entry->SetOriginalRequestURL(navigation->original_request_url_);
  entry->SetIsOverridingUserAgent(navigation->is_overriding_user_agent_);
  entry->SetTimestamp(navigation->timestamp_);
  entry->SetHttpStatusCode(navigation->http_status_code_);
  entry->SetRedirectChain(navigation->redirect_chain_);

  const ContentSerializedNavigationDriver::ExtendedInfoHandlerMap&
      extended_info_handlers = ContentSerializedNavigationDriver::GetInstance()
                                   ->GetAllExtendedInfoHandlers();
  for (const auto& extended_info_entry : navigation->extended_info_map_) {
    const std::string& key = extended_info_entry.first;
    if (!extended_info_handlers.count(key))
      continue;
    ExtendedInfoHandler* extended_info_handler =
        extended_info_handlers.at(key).get();
    DCHECK(extended_info_handler);
    extended_info_handler->RestoreExtendedInfo(extended_info_entry.second,
                                               entry.get());
  }

  // These fields should have default values.
  DCHECK_EQ(SerializedNavigationEntry::STATE_INVALID,
            navigation->blocked_state_);
  DCHECK_EQ(0u, navigation->content_pack_categories_.size());

  return entry;
}

// static
std::vector<std::unique_ptr<content::NavigationEntry>>
ContentSerializedNavigationBuilder::ToNavigationEntries(
    const std::vector<SerializedNavigationEntry>& navigations,
    content::BrowserContext* browser_context) {
  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.reserve(navigations.size());
  for (const auto& navigation : navigations)
    entries.push_back(ToNavigationEntry(&navigation, browser_context));
  return entries;
}

}  // namespace sessions
