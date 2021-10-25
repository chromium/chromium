// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_OBSERVER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

class MediaSessionNotificationProducerObserver : public base::CheckedObserver {
 public:
  virtual void OnMediaSessionItemCreated(const std::string& id) = 0;

  virtual void OnMediaSessionItemDestroyed(const std::string& id) = 0;

 protected:
  ~MediaSessionNotificationProducerObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_OBSERVER_H_
