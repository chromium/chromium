// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_SINK_WITH_CAST_MODES_OBSERVER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_SINK_WITH_CAST_MODES_OBSERVER_H_

#include <vector>

#include "base/observer_list_types.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"

namespace media_router {

// This interface can be used for observing updates about what |MediaSink| and
// |MediaCastMode| combinations can be cast to. Classes such as
// |QueryResultManager| or |MediaStartRouter| will alert on these changes.
class MediaSinkWithCastModesObserver : public base::CheckedObserver {
 public:
  ~MediaSinkWithCastModesObserver() override = default;

  virtual void OnSinksUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_SINK_WITH_CAST_MODES_OBSERVER_H_
