// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_URL_LIST_MANAGER_H_
#define COMPONENTS_BLOCKED_CONTENT_URL_LIST_MANAGER_H_

#include <stdint.h>

#include "base/observer_list.h"

class GURL;

namespace blocked_content {

// This class manages lists of blocked URLs in order to drive UI surfaces.
// Currently it is used by the redirect / popup blocked UIs.
//
// TODO(csharrison): Currently this object is composed within the framebust /
// popup tab helpers. Eventually those objects could be replaced almost entirely
// by shared logic here.
class UrlListManager {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void BlockedUrlAdded(int32_t id, const GURL& url) = 0;
  };

  UrlListManager();

  UrlListManager(const UrlListManager&) = delete;
  UrlListManager& operator=(const UrlListManager&) = delete;

  ~UrlListManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyObservers(int32_t id, const GURL& url);

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_URL_LIST_MANAGER_H_
