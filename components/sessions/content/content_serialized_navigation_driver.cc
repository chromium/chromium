// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_driver.h"

#include <utility>

#include "base/memory/singleton.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/common/page_state.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"

namespace sessions {

namespace {

ContentSerializedNavigationDriver* g_instance = nullptr;

}  // namespace

// static
SerializedNavigationDriver* SerializedNavigationDriver::Get() {
  return ContentSerializedNavigationDriver::GetInstance();
}

// static
ContentSerializedNavigationDriver*
ContentSerializedNavigationDriver::GetInstance() {
  if (g_instance)
    return g_instance;

  auto* instance = base::Singleton<
      ContentSerializedNavigationDriver,
      base::LeakySingletonTraits<ContentSerializedNavigationDriver>>::get();
  g_instance = instance;
  return instance;
}

// static
void ContentSerializedNavigationDriver::SetInstance(
    ContentSerializedNavigationDriver* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

ContentSerializedNavigationDriver::ContentSerializedNavigationDriver() {
}

ContentSerializedNavigationDriver::~ContentSerializedNavigationDriver() {
}

int ContentSerializedNavigationDriver::GetDefaultReferrerPolicy() const {
  return static_cast<int>(network::mojom::ReferrerPolicy::kDefault);
}

std::string
ContentSerializedNavigationDriver::GetSanitizedPageStateForPickle(
    const SerializedNavigationEntry* navigation) const {
  if (!navigation->has_post_data())
    return navigation->encoded_page_state();

  content::PageState page_state = content::PageState::CreateFromEncodedData(
      navigation->encoded_page_state());
  return page_state.RemovePasswordData().ToEncodedData();
}

void ContentSerializedNavigationDriver::Sanitize(
    SerializedNavigationEntry* navigation) const {
}

std::string ContentSerializedNavigationDriver::StripReferrerFromPageState(
      const std::string& page_state) const {
  return content::PageState::CreateFromEncodedData(page_state)
      .RemoveReferrer()
      .ToEncodedData();
}

void ContentSerializedNavigationDriver::RegisterExtendedInfoHandler(
    const std::string& key,
    std::unique_ptr<ExtendedInfoHandler> handler) {
  DCHECK(!key.empty());
  DCHECK(!extended_info_handler_map_.count(key));
  DCHECK(handler);
  extended_info_handler_map_[key] = std::move(handler);
}

const ContentSerializedNavigationDriver::ExtendedInfoHandlerMap&
ContentSerializedNavigationDriver::GetAllExtendedInfoHandlers() const {
  return extended_info_handler_map_;
}

}  // namespace sessions
