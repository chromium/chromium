// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/command_line_config_policy.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "url/gurl.h"

namespace update_client {

bool CommandLineConfigPolicy::BackgroundDownloadsEnabled() const {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

bool CommandLineConfigPolicy::DeltaUpdatesEnabled() const {
  return true;
}

bool CommandLineConfigPolicy::FastUpdate() const {
  return false;
}

bool CommandLineConfigPolicy::PingsEnabled() const {
  return true;
}

bool CommandLineConfigPolicy::TestRequest() const {
  return false;
}

GURL CommandLineConfigPolicy::UrlSourceOverride() const {
  return GURL();
}

base::TimeDelta CommandLineConfigPolicy::InitialDelay() const {
  return base::Seconds(0);
}

}  // namespace update_client
