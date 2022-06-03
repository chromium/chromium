// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_serialized_navigation_builder.h"

#include "components/sessions/core/serialized_navigation_entry.h"
#include "ios/web/public/favicon/favicon_status.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/session/crw_navigation_item_storage.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace sessions {

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
  if (item.GetFavicon().valid)
    navigation.favicon_url_ = item.GetFavicon().url;

  return navigation;
}

SerializedNavigationEntry
IOSSerializedNavigationBuilder::FromNavigationStorageItem(
    int index,
    CRWNavigationItemStorage* item) {
  // Create a NavigationItem to reserve a UniqueID.
  auto navigation_item = web::NavigationItem::Create();
  SerializedNavigationEntry navigation;
  navigation.index_ = index;
  navigation.unique_id_ = navigation_item->GetUniqueID();
  navigation.referrer_url_ = item.referrer.url;
  navigation.referrer_policy_ = item.referrer.policy;
  navigation.virtual_url_ = item.virtualURL;
  navigation.title_ = item.title;
  // Use reload transition type to avoid incorrect increase for typed count.
  navigation.transition_type_ = ui::PAGE_TRANSITION_RELOAD;
  navigation.timestamp_ = item.timestamp;

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
    item->GetFavicon().url = navigation->favicon_url_;
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
