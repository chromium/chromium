// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/starboard_adapter_factory.h"

chromecast::CastStarboardApiAdapter* CreateCastStarboardApiAdapter() {
  LOG(INFO) << "Loaded Starboard adapter shared library for SB_API_VERSION "
            << SB_API_VERSION;
  return chromecast::CastStarboardApiAdapterImpl::GetInstance();
}

chromecast::media::StarboardApiWrapper* CreateStarboardApiWrapper() {
  return chromecast::media::GetStarboardApiWrapper().release();
}

void ReleaseCastStarboardApiAdapter() {
  LOG(INFO) << "ReleaseCastStarboardApiAdapter invoked";
  // This is only ever registered as a base::AtExitManager task.
  chromecast::CastStarboardApiAdapterImpl::GetInstance()->ReleaseForAtExit();
}
