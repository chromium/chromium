// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_H_

#include <functional>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"

// Base class that handles observers for all the download bubble info classes.
// Info classes correspond to a particular view and hold the information
// necessary for that view to display itself. They are responsible for keeping
// that information up to date and notifying observers of changes.
template <typename ObserverType>
class DownloadBubbleInfo {
 public:
  using Observer = ObserverType;

  DownloadBubbleInfo() = default;
  ~DownloadBubbleInfo() = default;

  // This class is neither copyable nor movable due to the `ObserverList`
  // member.
  DownloadBubbleInfo(DownloadBubbleInfo&) = delete;
  DownloadBubbleInfo& operator=(DownloadBubbleInfo&) = delete;

  void AddObserver(Observer* observer) const {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) const {
    observers_.RemoveObserver(observer);
  }

 protected:
  template <typename ObserverMethod, typename... Args>
  void NotifyObservers(ObserverMethod&& method, Args&&... args) {
    for (Observer& obs : observers_) {
      std::invoke(method, obs, std::forward<Args>(args)...);
    }
  }

 private:
  mutable base::ObserverList<Observer> observers_;
};

// A simple observer for views that don't need any kind of incremental updating.
class DownloadBubbleInfoChangeObserver : public base::CheckedObserver {
 public:
  DownloadBubbleInfoChangeObserver();
  ~DownloadBubbleInfoChangeObserver() override;

  virtual void OnInfoChanged() {}
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_INFO_H_
