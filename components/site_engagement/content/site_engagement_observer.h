// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_OBSERVER_H_
#define COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_OBSERVER_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/webapps/common/web_app_id.h"

namespace content {
class WebContents;
}

class GURL;

namespace site_engagement {

class SiteEngagementService;
enum class EngagementType;

class SiteEngagementObserver {
 public:
  SiteEngagementObserver(const SiteEngagementObserver&) = delete;
  SiteEngagementObserver& operator=(const SiteEngagementObserver&) = delete;

  // Called when the engagement for `url` loaded in `web_contents` is changed
  // to `score`, due to an event of type `type`. `old_score` is the score prior
  // to the engagement event. This method may be run on user input, so
  // observers *must not* perform any expensive tasks here. `web_contents` may
  // be null if the engagement has increased when `url` is not in a tab, e.g.
  // from a notification interaction. The `app_id` is populated on non-Android
  // platforms from the first non-null/empty value in this list, or std::nullopt
  // otherwise.
  // - The id of the launched app, for the launch engagement type.
  // - The id from the WebAppTabHelper, which is calculated from the last
  //   committed navigation on that web contents (if the web contents exists).
  // - TODO(crbug.com/358168777): Either given from the notification system on a
  //   notification click for an app, or looked up from the app registrar.
  virtual void OnEngagementEvent(content::WebContents* web_contents,
                                 const GURL& url,
                                 double score,
                                 double old_score,
                                 EngagementType type,
                                 const std::optional<webapps::AppId>& app_id) {}

 protected:
  explicit SiteEngagementObserver(SiteEngagementService* service);

  SiteEngagementObserver();

  virtual ~SiteEngagementObserver();

  // Returns the site engagement service which this object is observing.
  SiteEngagementService* GetSiteEngagementService() const;

  // Begin observing `service` for engagement increases.
  // To stop observing, call Observe(nullptr).
  void Observe(SiteEngagementService* service);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, Observers);
  friend class SiteEngagementService;

  raw_ptr<SiteEngagementService> service_;
};

}  // namespace site_engagement

#endif  // COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_OBSERVER_H_
