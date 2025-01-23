// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_DWA_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_METRICS_CONTENT_DWA_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace metrics {

// DWA needs to upload the events it records to enable metric collection.
// In accordance with the Chrome Privacy team requirements, we upload all
// collected metrics whenever a tab finishes loading. Specifically, DWA adds
// events in each page load to a collection of "page load events", this is done
// to prevent the ability of deducing a user's browsing history by combining
// metrics from different web pages in the same collection.
// TODO(crbug.com/387232176): The current implementation may mix data from
// different pages in the same pageload event. This is fine for now, but later
// on we want to properly group the data from one page together.
class DwaWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DwaWebContentsObserver> {
 public:
  explicit DwaWebContentsObserver(content::WebContents* web_contents);

  DwaWebContentsObserver(const DwaWebContentsObserver&) = delete;
  DwaWebContentsObserver& operator=(const DwaWebContentsObserver&) = delete;

  ~DwaWebContentsObserver() override;

 private:
  friend class content::WebContentsUserData<DwaWebContentsObserver>;

  // content::WebContentsObserver:
  void DidStopLoading() override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_DWA_WEB_CONTENTS_OBSERVER_H_
