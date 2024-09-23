// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_SERVICE_H_
#define COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_SERVICE_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom.h"
#include "ui/base/page_transition_types.h"

namespace base {
class Clock;
}

namespace webapps {
FORWARD_DECLARE_TEST(AppBannerManagerBrowserTest, WebAppBannerNeedsEngagement);
}

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace web_app {
class WebAppEngagementBrowserTest;
}

class GURL;
class HostContentSettingsMap;
class PrefRegistrySimple;
class NotificationPermissionReviewServiceTest;
class SafetyHubCardDataHelperTest;

namespace site_engagement {

enum class EngagementType;
class SiteEngagementObserver;
class SiteEngagementScore;

#if BUILDFLAG(IS_ANDROID)
class SiteEngagementServiceAndroid;
#endif

// Stores and retrieves the engagement score of an origin.
//
// An engagement score is a non-negative double that represents how much a user
// has engaged with an origin - the higher it is, the more engagement the user
// has had with this site recently.
//
// User activity such as visiting the origin often, interacting with the origin,
// and adding it to the homescreen will increase the site engagement score. If
// a site's score does not increase for some time, it will decay, eventually
// reaching zero with further disuse.
//
// The SiteEngagementService object must be created and used on the UI thread
// only. Engagement scores may be queried in a read-only fashion from other
// threads using SiteEngagementService::GetScoreFromSettings, but use of this
// method is discouraged unless it is not possible to use the UI thread.
class SiteEngagementService : public KeyedService,
                              public SiteEngagementScoreProvider {
 public:
  // Sets of URLs that are used to filter engagement details.
  class URLSets {
   public:
    using Type = uint32_t;
    // Includes http:// and https:// sites.
    static constexpr Type HTTP = 1 << 0;
    // Includes chrome:// and chrome-untrusted:// sites.
    static constexpr Type WEB_UI = 1 << 1;
  };

  // The provider allows code agnostic to the embedder (e.g. in
  // //components) to retrieve the SiteEngagementService. It should be set by
  // each embedder that uses the SiteEngagementService, via SetServiceProvider.
  class ServiceProvider {
   public:
    ~ServiceProvider() = default;

    // Should always return a non null value, creating the service if it does
    // not exist.
    virtual SiteEngagementService* GetSiteEngagementService(
        content::BrowserContext* browser_context) = 0;
  };

  // WebContentsObserver that detects engagement triggering events and notifies
  // the service of them.
  class Helper;

  // The name of the site engagement variation field trial.
  static const char kEngagementParams[];

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets and clears the service provider. These are separate functions to
  // enable better checking.
  static void SetServiceProvider(ServiceProvider* provider);
  static void ClearServiceProvider(ServiceProvider* provider);

  // Returns the site engagement service attached to this Browser Context. The
  // service exists in incognito mode; scores will be initialised using the
  // score from the Browser Context that the incognito session was created from,
  // and will increase and decrease as usual. Engagement earned or decayed in
  // incognito will not be persisted or reflected in the original Browser
  // Context.
  //
  // This method must be called on the UI thread.
  static SiteEngagementService* Get(content::BrowserContext* browser_context);

  // Returns the maximum possible amount of engagement that a site can accrue.
  static double GetMaxPoints();

  // Returns whether or not the site engagement service is enabled.
  static bool IsEnabled();

  // Returns the score for |origin| based on |settings|. Can be called on any
  // thread and does not cause any cleanup, decay, etc.
  //
  // Should only be used if you cannot create a SiteEngagementService (i.e. you
  // cannot run on the UI thread).
  static double GetScoreFromSettings(HostContentSettingsMap* settings,
                                     const GURL& origin);

  // Retrieves all details for origins within `url_set`. Can be called
  // from a background thread. `now` must be the current timestamp. Takes a
  // scoped_refptr to keep HostContentSettingsMap alive. See crbug.com/901287.
  static std::vector<mojom::SiteEngagementDetails> GetAllDetailsInBackground(
      base::Time now,
      scoped_refptr<HostContentSettingsMap> map,
      URLSets::Type url_set = URLSets::HTTP);

  // Returns whether |score| is at least the given |level| of engagement.
  static bool IsEngagementAtLeast(double score,
                                  blink::mojom::EngagementLevel level);

  explicit SiteEngagementService(content::BrowserContext* browser_context);

  SiteEngagementService(const SiteEngagementService&) = delete;
  SiteEngagementService& operator=(const SiteEngagementService&) = delete;

  ~SiteEngagementService() override;

  // Returns the engagement level of |url|.
  blink::mojom::EngagementLevel GetEngagementLevel(const GURL& url) const;

  // Returns an array of engagement score details for all origins that are
  // in `url_set` and have a score. A origin can have a score due to
  // direct engagement, or other factors that cause an engagement bonus to
  // be applied.
  //
  // Note that this method is quite expensive, so try to avoid calling it in
  // performance-critical code.
  std::vector<mojom::SiteEngagementDetails> GetAllDetails(
      URLSets::Type url_set = URLSets::HTTP) const;

  // Update the engagement score of |url| for a notification interaction.
  void HandleNotificationInteraction(const GURL& url);

  // Returns whether the engagement service has enough data to make meaningful
  // decisions. Clients should avoid using engagement in their heuristic until
  // this is true.
  bool IsBootstrapped() const;

  // Resets the base engagement for |url| to |score|, clearing daily limits. Any
  // bonus engagement that |url| has acquired is not affected by this method, so
  // the result of GetScore(|url|) may not be the same as |score|.
  void ResetBaseScoreForURL(const GURL& url, double score);

  // Update the last time |url| was opened from an installed shortcut (hosted in
  // |web_contents|) to be clock_->Now().
  void SetLastShortcutLaunchTime(content::WebContents* web_contents,
#if !BUILDFLAG(IS_ANDROID)
                                 const webapps::AppId& app_id,
#endif
                                 const GURL& url);

  // Returns the site engagement details for the specified |url|.
  mojom::SiteEngagementDetails GetDetails(const GURL& url) const;

  // Overridden from SiteEngagementScoreProvider.
  double GetScore(const GURL& url) const override;
  double GetTotalEngagementPoints() const override;

  // Just forwards calls AddPoints.
  void AddPointsForTesting(const GURL& url, double points);

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

 protected:
  // Retrieves the SiteEngagementScore object for |origin|.
  SiteEngagementScore CreateEngagementScore(const GURL& origin) const;

  void SetLastEngagementTime(base::Time last_engagement_time) const;

  content::BrowserContext* browser_context() { return browser_context_; }
  const base::Clock& clock() { return *clock_; }

 private:
  friend class SiteEngagementObserver;
  friend class SiteEngagementServiceTest;
  friend class web_app::WebAppEngagementBrowserTest;
  friend class ::NotificationPermissionReviewServiceTest;
  friend class ::SafetyHubCardDataHelperTest;
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CheckHistograms);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CleanupEngagementScores);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           CleanupMovesScoreBackToNow);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           CleanupMovesScoreBackToRebase);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           CleanupEngagementScoresProportional);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetTotalNavigationPoints);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetTotalUserInputPoints);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, RestrictedToHTTPAndHTTPS);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, Observers);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, LastEngagementTime);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           IncognitoEngagementService);
  FRIEND_TEST_ALL_PREFIXES(webapps::AppBannerManagerBrowserTest,
                           WebAppBannerNeedsEngagement);
  FRIEND_TEST_ALL_PREFIXES(AppBannerSettingsHelperTest, SiteEngagementTrigger);
  FRIEND_TEST_ALL_PREFIXES(HostedAppPWAOnlyTest, EngagementHistogram);

#if BUILDFLAG(IS_ANDROID)
  // Shim class to expose the service to Java.
  friend class SiteEngagementServiceAndroid;
  SiteEngagementServiceAndroid* GetAndroidService() const;
  void SetAndroidService(
      std::unique_ptr<SiteEngagementServiceAndroid> android_service);
#endif

  // Adds the specified number of points to the given origin, respecting the
  // maximum limits for the day and overall.
  void AddPoints(const GURL& url, double points);

  // Runs site engagement maintenance tasks.
  void AfterStartupTask();

  // Removes any origins which have decayed to 0 engagement. If
  // |update_last_engagement_time| is true, the last engagement time of all
  // origins is reset by calculating the delta between the last engagement event
  // recorded by the site engagement service and the origin. The origin's last
  // engagement time is then set to clock_->Now() - delta.
  //
  // If a user does not use the browser at all for some period of time,
  // engagement is not decayed, and the state is restored equivalent to how they
  // left it once they return.
  void CleanupEngagementScores(bool update_last_engagement_time) const;

  // Possibly records UMA metrics if we haven't recorded them lately.
  void MaybeRecordMetrics();

  // Actually records metrics for the engagement in |details|.
  void RecordMetrics(std::vector<mojom::SiteEngagementDetails>);

  // Returns true if we should record engagement for this URL. Currently,
  // engagement is only earned for HTTP and HTTPS.
  bool ShouldRecordEngagement(const GURL& url) const;

  // Get and set the last engagement time from prefs.
  base::Time GetLastEngagementTime() const;

  // Get the maximum decay period and the stale period for last engagement
  // times.
  base::TimeDelta GetMaxDecayPeriod() const;
  base::TimeDelta GetStalePeriod() const;

  // Returns the median engagement score of all recorded origins. |details| must
  // be sorted in ascending order of score.
  double GetMedianEngagementFromSortedDetails(
      const std::vector<mojom::SiteEngagementDetails>& details) const;

  // Update the engagement score of the origin loaded in |web_contents| for
  // media playing. The points awarded are discounted if the media is being
  // played in a non-visible tab.
  void HandleMediaPlaying(content::WebContents* web_contents, bool is_hidden);

  // Update the engagement score of the origin loaded in |web_contents| for
  // navigation.
  void HandleNavigation(content::WebContents* web_contents,
                        ui::PageTransition transition);

  // Update the engagement score of the origin loaded in |web_contents| for
  // time-on-site, based on user input.
  void HandleUserInput(content::WebContents* web_contents, EngagementType type);

  // Called when the engagement for |url| loaded in |web_contents| is changed,
  // due to an event of type |type|. Calls OnEngagementEvent in all observers.
  // |web_contents| may be null if the engagement has increased when |url| is
  // not in a tab, e.g. from a notification interaction. Also records
  // engagement-type metrics.
  void OnEngagementEvent(
      content::WebContents* web_contents,
      const GURL& url,
      EngagementType type,
      double old_score,
      const std::optional<webapps::AppId>& app_id_override = std::nullopt);

  // Returns true if the last engagement increasing event seen by the site
  // engagement service was sufficiently long ago that we need to reset all
  // scores to be relative to now. This ensures that users who do not use the
  // browser for an extended period of time do not have their engagement decay.
  bool IsLastEngagementStale() const;

  // Add and remove observers of this service.
  void AddObserver(SiteEngagementObserver* observer);
  void RemoveObserver(SiteEngagementObserver* observer);

  raw_ptr<content::BrowserContext, AcrossTasksDanglingUntriaged>
      browser_context_;

  // The clock used to vend times.
  raw_ptr<base::Clock> clock_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<SiteEngagementServiceAndroid> android_service_;
#endif

  // Metrics are recorded at non-incognito browser startup, and then
  // approximately once per hour thereafter. Store the local time at which
  // metrics were previously uploaded: the first event which affects any
  // origin's engagement score after an hour has elapsed triggers the next
  // upload.
  base::Time last_metrics_time_;

  // A list of observers. When any origin registers an engagement-increasing
  // event, each observer's OnEngagementEvent method will be called.
  base::ObserverList<SiteEngagementObserver>::Unchecked observer_list_;

  base::WeakPtrFactory<SiteEngagementService> weak_factory_{this};
};

}  // namespace site_engagement

#endif  // COMPONENTS_SITE_ENGAGEMENT_CONTENT_SITE_ENGAGEMENT_SERVICE_H_
