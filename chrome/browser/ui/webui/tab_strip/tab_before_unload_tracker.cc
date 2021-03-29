// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_before_unload_tracker.h"
#include <memory>
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace tab_strip_ui {

TabBeforeUnloadTracker::TabBeforeUnloadTracker(
    TabCloseCancelledCallback cancelled_callback)
    : cancelled_callback_(std::move(cancelled_callback)) {}
TabBeforeUnloadTracker::~TabBeforeUnloadTracker() = default;

void TabBeforeUnloadTracker::Observe(content::WebContents* contents) {
  observers_[contents] = std::make_unique<TabObserver>(contents, this);
}

void TabBeforeUnloadTracker::Unobserve(content::WebContents* contents) {
  observers_.erase(contents);
}

void TabBeforeUnloadTracker::OnBeforeUnloadDialogCancelled(
    content::WebContents* contents) {
  cancelled_callback_.Run(contents);
}

class TabBeforeUnloadTracker::TabObserver
    : public content::WebContentsObserver {
 public:
  TabObserver(content::WebContents* contents, TabBeforeUnloadTracker* tracker)
      : content::WebContentsObserver(contents), tracker_(tracker) {}
  ~TabObserver() override = default;

  // content::WebContentsObserver
  void WebContentsDestroyed() override { tracker_->Unobserve(web_contents()); }

  void BeforeUnloadDialogCancelled() override {
    tracker_->OnBeforeUnloadDialogCancelled(web_contents());
  }

 private:
  TabBeforeUnloadTracker* tracker_;
};

}  // namespace tab_strip_ui
