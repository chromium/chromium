// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_

class MediaNotificationContainerObserver;

class MediaNotificationContainerImpl {
 public:
  virtual void AddObserver(MediaNotificationContainerObserver* observer) = 0;
  virtual void RemoveObserver(MediaNotificationContainerObserver* observer) = 0;

 protected:
  virtual ~MediaNotificationContainerImpl() = default;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
