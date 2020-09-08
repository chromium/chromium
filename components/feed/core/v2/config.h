// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_CONFIG_H_
#define COMPONENTS_FEED_CORE_V2_CONFIG_H_

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"

namespace feed {

// The Feed configuration. Default values appear below. Always use
// |GetFeedConfig()| to get the current configuration.
struct Config {
  // Maximum number of FeedQuery or action upload requests per day.
  int max_feed_query_requests_per_day = 20;
  int max_action_upload_requests_per_day = 20;
  // Content older than this threshold will not be shown to the user.
  base::TimeDelta stale_content_threshold = base::TimeDelta::FromHours(48);
  // The time between background refresh attempts. Ignored if a server-defined
  // fetch schedule has been assigned.
  base::TimeDelta default_background_refresh_interval =
      base::TimeDelta::FromHours(24);
  // Maximum number of times to attempt to upload a pending action before
  // deleting it.
  int max_action_upload_attempts = 3;
  // Maximum age for a pending action. Actions older than this are deleted.
  base::TimeDelta max_action_age = base::TimeDelta::FromHours(24);
  // Maximum payload size for one action upload batch.
  size_t max_action_upload_bytes = 20000;
  // If no surfaces are attached, the stream model is unloaded after this
  // timeout.
  base::TimeDelta model_unload_timeout = base::TimeDelta::FromSeconds(1);
  // How far ahead in number of items from last visible item to final item
  // before attempting to load more content.
  int load_more_trigger_lookahead = 5;
  // Whether to attempt uploading actions when Chrome is hidden.
  bool upload_actions_on_enter_background = true;
  // Set of optional capabilities included in requests. See
  // CreateFeedQueryRequest() for required capabilities.
  base::flat_set<feedwire::Capability> experimental_capabilities = {
      feedwire::Capability::REQUEST_SCHEDULE,
      feedwire::Capability::OPEN_IN_TAB,
      feedwire::Capability::DOWNLOAD_LINK,
      feedwire::Capability::INFINITE_FEED,
      feedwire::Capability::DISMISS_COMMAND,
      feedwire::Capability::UI_THEME_V2,
      feedwire::Capability::UNDO_FOR_DISMISS_COMMAND,
      feedwire::Capability::PREFETCH_METADATA,
  };

  Config();
  Config(const Config& other);
  ~Config();
};

// Gets the current configuration.
const Config& GetFeedConfig();

void SetFeedConfigForTesting(const Config& config);
void OverrideConfigWithFinchForTesting();

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_CONFIG_H_
