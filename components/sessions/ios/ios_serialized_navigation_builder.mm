// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_serialized_navigation_builder.h"

#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "ios/web/public/favicon/favicon_status.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/session/proto/navigation.pb.h"
#include "ios/web/public/session/proto/proto_util.h"

namespace sessions {
namespace {

// Returns a new unique ID valid for a NavigationItem.
int GetNewNavigationItemUniqueID() {
  // Create a NavigationItem to reserve a UniqueID.
  return web::NavigationItem::Create()->GetUniqueID();
}

}  // anonymous namespace

// static
SerializedNavigationEntry
IOSSerializedNavigationBuilder::FromNavigationItem(
    int index, const web::NavigationItem& item) {
  SerializedNavigationEntry navigation;
  navigation.index_ = index;
  navigation.unique_id_ = item.GetUniqueID();
  navigation.referrer_url_ = item.GetReferrer().url;
  navigation.referrer_policy_ = item.GetReferrer().policy;
  navigation.virtual_url_ = item.GetVirtualURL();
  navigation.title_ = item.GetTitle();
  navigation.transition_type_ = item.GetTransitionType();
  navigation.timestamp_ = item.GetTimestamp();

  const web::FaviconStatus& favicon_status = item.GetFaviconStatus();
  if (favicon_status.valid)
    navigation.favicon_url_ = favicon_status.url;

  return navigation;
}

SerializedNavigationEntry
IOSSerializedNavigationBuilder::FromNavigationStorageItem(
    int index,
    const web::proto::NavigationItemStorage& item) {
  SerializedNavigationEntry navigation;
  navigation.set_index(index);
  navigation.set_unique_id(GetNewNavigationItemUniqueID());
  if (item.has_referrer()) {
    const web::Referrer referrer = web::ReferrerFromProto(item.referrer());
    navigation.set_referrer_url(referrer.url);
    navigation.set_referrer_policy(referrer.policy);
  }
  navigation.set_virtual_url(
      item.virtual_url().empty() ? GURL(item.url()) : GURL(item.virtual_url()));
  navigation.set_title(base::UTF8ToUTF16(item.title()));

  // Use reload transition type to avoid incorrect increase for typed count.
  navigation.set_transition_type(ui::PAGE_TRANSITION_RELOAD);
  navigation.set_timestamp(web::TimeFromProto(item.timestamp()));

  return navigation;
}

// static
std::unique_ptr<web::NavigationItem>
IOSSerializedNavigationBuilder::ToNavigationItem(
    const SerializedNavigationEntry* navigation) {
  std::unique_ptr<web::NavigationItem> item(web::NavigationItem::Create());

  item->SetURL(navigation->virtual_url_);
  item->SetReferrer(web::Referrer(
      navigation->referrer_url_,
      static_cast<web::ReferrerPolicy>(navigation->referrer_policy_)));
  item->SetTitle(navigation->title_);
  item->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
  item->SetTimestamp(navigation->timestamp_);

  if (navigation->favicon_url_.is_valid()) {
    web::FaviconStatus favicon_status = item->GetFaviconStatus();
    favicon_status.url = navigation->favicon_url_;
    item->SetFaviconStatus(favicon_status);
  }

  return item;
}

// static
std::vector<std::unique_ptr<web::NavigationItem>>
IOSSerializedNavigationBuilder::ToNavigationItems(
    const std::vector<SerializedNavigationEntry>& navigations) {
  std::vector<std::unique_ptr<web::NavigationItem>> items;
  for (const auto& navigation : navigations)
    items.push_back(ToNavigationItem(&navigation));

  return items;
}

}  // namespace sessions
