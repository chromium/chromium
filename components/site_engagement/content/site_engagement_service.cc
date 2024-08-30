// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_engagement/content/site_engagement_service.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/permissions/permissions_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_metrics.h"
#include "components/site_engagement/content/site_engagement_observer.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/core/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webapps/browser/webapps_client.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/site_engagement/content/android/site_engagement_service_android.h"
#endif

namespace site_engagement {

namespace {

// Global bool to ensure we only update the parameters from variations once.
bool g_updated_from_variations = false;

SiteEngagementService::ServiceProvider* g_service_provider = nullptr;

// Length of time between metrics logging.
const int kMetricsIntervalInMinutes = 60;

// A clock that keeps showing the time it was constructed with.
class StoppedClock : public base::Clock {
 public:
  explicit StoppedClock(base::Time time) : time_(time) {}

  StoppedClock(const StoppedClock&) = delete;
  StoppedClock& operator=(const StoppedClock&) = delete;

  ~StoppedClock() override = default;

 protected:
  // base::Clock:
  base::Time Now() const override { return time_; }

 private:
  const base::Time time_;
};

// Helper for fetching content settings for one type.
ContentSettingsForOneType GetContentSettingsFromBrowserContext(
    content::BrowserContext* browser_context,
    ContentSettingsType type) {
  return permissions::PermissionsClient::Get()
      ->GetSettingsMap(browser_context)
      ->GetSettingsForOneType(type);
}

// Returns the combined list of origins which either have site engagement
// data stored, or have other settings that would provide a score bonus.
std::set<GURL> GetEngagementOriginsFromContentSettings(
    HostContentSettingsMap* map) {
  std::set<GURL> urls;

  // Fetch URLs of sites with engagement details stored.
  for (const auto& site :
       map->GetSettingsForOneType(ContentSettingsType::SITE_ENGAGEMENT)) {
    urls.insert(GURL(site.primary_pattern.ToString()));
  }

  return urls;
}

SiteEngagementScore CreateEngagementScoreImpl(base::Clock* clock,
                                              const GURL& origin,
                                              HostContentSettingsMap* map) {
  return SiteEngagementScore(clock, origin, map);
}

mojom::SiteEngagementDetails GetDetailsImpl(base::Clock* clock,
                                            const GURL& origin,
                                            HostContentSettingsMap* map) {
  return CreateEngagementScoreImpl(clock, origin, map).GetDetails();
}

bool IsUrlInUrlSet(const GURL& url,
                   SiteEngagementService::URLSets::Type url_set) {
  if ((url_set & SiteEngagementService::URLSets::HTTP) &&
      url.SchemeIsHTTPOrHTTPS()) {
    return true;
  }

  if ((url_set & SiteEngagementService::URLSets::WEB_UI) &&
      (url.SchemeIs(content::kChromeUIScheme) ||
       url.SchemeIs(content::kChromeUIUntrustedScheme))) {
    return true;
  }

  return false;
}

std::vector<mojom::SiteEngagementDetails> GetAllDetailsImpl(
    base::Clock* clock,
    HostContentSettingsMap* map,
    SiteEngagementService::URLSets::Type url_set) {
  std::set<GURL> origins = GetEngagementOriginsFromContentSettings(map);

  std::vector<mojom::SiteEngagementDetails> details;
  details.reserve(origins.size());

  for (const GURL& origin : origins) {
    if (!origin.is_valid())
      continue;
    if (IsUrlInUrlSet(origin, url_set)) {
      details.push_back(GetDetailsImpl(clock, origin, map));
    }
  }

  return details;
}

// Only accept a navigation event for engagement if it is one of:
//  a. direct typed navigation
//  b. clicking on an omnibox suggestion brought up by typing a keyword
//  c. clicking on a bookmark or opening a bookmark app
//  d. a custom search engine keyword search (e.g. Wikipedia search box added as
//  search engine)
//  e. an automatically generated top level navigation (e.g. command line
//  navigation, in product help link).
bool IsEngagementNavigation(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_GENERATED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_KEYWORD_GENERATED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
}

}  // namespace

const char SiteEngagementService::kEngagementParams[] = "SiteEngagement";

// static
void SiteEngagementService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kSiteEngagementLastUpdateTime, 0,
                              PrefRegistry::LOSSY_PREF);
}

// static
SiteEngagementService* SiteEngagementService::Get(
    content::BrowserContext* context) {
  DCHECK(g_service_provider);
  return g_service_provider->GetSiteEngagementService(context);
}

// static
void SiteEngagementService::SetServiceProvider(ServiceProvider* provider) {
  DCHECK(provider);
  DCHECK(!g_service_provider);
  g_service_provider = provider;
}

// static
void SiteEngagementService::ClearServiceProvider(ServiceProvider* provider) {
  DCHECK(provider);
  DCHECK_EQ(provider, g_service_provider);
  g_service_provider = nullptr;
}

// static
double SiteEngagementService::GetMaxPoints() {
  return SiteEngagementScore::kMaxPoints;
}

// static
bool SiteEngagementService::IsEnabled() {
  const std::string group_name =
      base::FieldTrialList::FindFullName(kEngagementParams);
  return !base::StartsWith(group_name, "Disabled",
                           base::CompareCase::SENSITIVE);
}

// static
double SiteEngagementService::GetScoreFromSettings(
    HostContentSettingsMap* settings,
    const GURL& origin) {
  return SiteEngagementScore(base::DefaultClock::GetInstance(), origin,
                             settings)
      .GetTotalScore();
}

// static
std::vector<mojom::SiteEngagementDetails>
SiteEngagementService::GetAllDetailsInBackground(
    base::Time now,
    scoped_refptr<HostContentSettingsMap> map,
    URLSets::Type url_set) {
  StoppedClock clock(now);
  base::AssertLongCPUWorkAllowed();
  return GetAllDetailsImpl(&clock, map.get(), url_set);
}

// static
bool SiteEngagementService::IsEngagementAtLeast(
    double score,
    blink::mojom::EngagementLevel level) {
  DCHECK_LT(SiteEngagementScore::GetMediumEngagementBoundary(),
            SiteEngagementScore::GetHighEngagementBoundary());
  switch (level) {
    case blink::mojom::EngagementLevel::NONE:
      return true;
    case blink::mojom::EngagementLevel::MINIMAL:
      return score > 0;
    case blink::mojom::EngagementLevel::LOW:
      return score >= 1;
    case blink::mojom::EngagementLevel::MEDIUM:
      return score >= SiteEngagementScore::GetMediumEngagementBoundary();
    case blink::mojom::EngagementLevel::HIGH:
      return score >= SiteEngagementScore::GetHighEngagementBoundary();
    case blink::mojom::EngagementLevel::MAX:
      return score == SiteEngagementScore::kMaxPoints;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

SiteEngagementService::SiteEngagementService(content::BrowserContext* context)
    : browser_context_(context), clock_(base::DefaultClock::GetInstance()) {
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&SiteEngagementService::AfterStartupTask,
                                weak_factory_.GetWeakPtr()));

  if (!g_updated_from_variations) {
    SiteEngagementScore::UpdateFromVariations(kEngagementParams);
    g_updated_from_variations = true;
  }
}

SiteEngagementService::~SiteEngagementService() {
  // Clear any observers to avoid dangling pointers back to this object.
  for (auto& observer : observer_list_)
    observer.Observe(nullptr);
}

blink::mojom::EngagementLevel SiteEngagementService::GetEngagementLevel(
    const GURL& url) const {
  if (IsLastEngagementStale())
    CleanupEngagementScores(true);

  return CreateEngagementScore(url).GetEngagementLevel();
}

std::vector<mojom::SiteEngagementDetails> SiteEngagementService::GetAllDetails(
    URLSets::Type url_set) const {
  if (IsLastEngagementStale())
    CleanupEngagementScores(true);
  return GetAllDetailsImpl(
      clock_,
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context_),
      url_set);
}

void SiteEngagementService::HandleNotificationInteraction(const GURL& url) {
  if (!ShouldRecordEngagement(url))
    return;

  double old_score = GetScore(url);
  AddPoints(url, SiteEngagementScore::GetNotificationInteractionPoints());

  MaybeRecordMetrics();
  OnEngagementEvent(nullptr /* web_contents */, url,
                    EngagementType::kNotificationInteraction, old_score);
}

bool SiteEngagementService::IsBootstrapped() const {
  return GetTotalEngagementPoints() >=
         SiteEngagementScore::GetBootstrapPoints();
}

void SiteEngagementService::AddObserver(SiteEngagementObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SiteEngagementService::RemoveObserver(SiteEngagementObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void SiteEngagementService::ResetBaseScoreForURL(const GURL& url,
                                                 double score) {
  SiteEngagementScore engagement_score = CreateEngagementScore(url);
  engagement_score.Reset(score, clock_->Now());
  engagement_score.Commit();
}

void SiteEngagementService::SetLastShortcutLaunchTime(
    content::WebContents* web_contents,
#if !BUILDFLAG(IS_ANDROID)
    const webapps::AppId& app_id,
#endif
    const GURL& url) {
  double old_score = GetScore(url);

  SiteEngagementScore score = CreateEngagementScore(url);

  base::Time now = clock_->Now();

  score.set_last_shortcut_launch_time(now);
  score.Commit();

  std::optional<webapps::AppId> web_app_id;
#if !BUILDFLAG(IS_ANDROID)
  CHECK(!app_id.empty());
  web_app_id = app_id;
#endif

  OnEngagementEvent(web_contents, url, EngagementType::kWebappShortcutLaunch,
                    old_score, web_app_id);
}

double SiteEngagementService::GetScore(const GURL& url) const {
  return GetDetails(url).total_score;
}

mojom::SiteEngagementDetails SiteEngagementService::GetDetails(
    const GURL& url) const {
  // Ensure that if engagement is stale, we clean things up before fetching the
  // score.
  if (IsLastEngagementStale())
    CleanupEngagementScores(true);

  return GetDetailsImpl(
      clock_, url,
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context_));
}

double SiteEngagementService::GetTotalEngagementPoints() const {
  std::vector<mojom::SiteEngagementDetails> details = GetAllDetails();

  double total_score = 0;
  for (const auto& detail : details)
    total_score += detail.total_score;

  return total_score;
}

void SiteEngagementService::AddPointsForTesting(const GURL& url,
                                                double points) {
  AddPoints(url, points);
}

#if BUILDFLAG(IS_ANDROID)
SiteEngagementServiceAndroid* SiteEngagementService::GetAndroidService() const {
  return android_service_.get();
}

void SiteEngagementService::SetAndroidService(
    std::unique_ptr<SiteEngagementServiceAndroid> android_service) {
  android_service_ = std::move(android_service);
}
#endif

void SiteEngagementService::AddPoints(const GURL& url, double points) {
  if (points == 0)
    return;

  // Trigger a cleanup and date adjustment if it has been a substantial length
  // of time since *any* engagement was recorded by the service. This will
  // ensure that we do not decay scores when the user did not use the browser.
  if (IsLastEngagementStale())
    CleanupEngagementScores(true);

  SiteEngagementScore score = CreateEngagementScore(url);
  score.AddPoints(points);
  score.Commit();

  SetLastEngagementTime(score.last_engagement_time());
}

void SiteEngagementService::AfterStartupTask() {
  // Check if we need to reset last engagement times on startup - we want to
  // avoid doing this in AddPoints() if possible. It is still necessary to check
  // in AddPoints for people who never restart Chrome, but leave it open and
  // their computer on standby.
  CleanupEngagementScores(IsLastEngagementStale());
}

void SiteEngagementService::CleanupEngagementScores(
    bool update_last_engagement_time) const {
  TRACE_EVENT0("navigation", "SiteEngagementService::CleanupEngagementScores");

  // We want to rebase last engagement times relative to MaxDecaysPerScore
  // periods of decay in the past.
  base::Time now = clock_->Now();
  base::Time last_engagement_time = GetLastEngagementTime();
  base::Time rebase_time = now - GetMaxDecayPeriod();
  base::Time new_last_engagement_time;

  // If |update_last_engagement_time| is true, we must have either:
  //   a) last_engagement_time is in the future; OR
  //   b) last_engagement_time < rebase_time < now
  DCHECK(!update_last_engagement_time || last_engagement_time >= now ||
         (last_engagement_time < rebase_time && rebase_time < now));

  // Cap |last_engagement_time| at |now| if it is in the future. This ensures
  // that we use sane offsets when a user has adjusted their clock backwards and
  // have a mix of scores prior to and after |now|.
  if (last_engagement_time > now)
    last_engagement_time = now;

  HostContentSettingsMap* settings_map =
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context_);
  for (const auto& site : GetContentSettingsFromBrowserContext(
           browser_context_, ContentSettingsType::SITE_ENGAGEMENT)) {
    GURL origin(site.primary_pattern.ToString());

    if (origin.is_valid()) {
      SiteEngagementScore score = CreateEngagementScore(origin);
      if (update_last_engagement_time) {
        // Catch cases of users moving their clocks, or a potential race where
        // a score content setting is written out to prefs, but the updated
        // |last_engagement_time| was not written, as both are lossy
        // preferences. |rebase_time| is strictly in the past, so any score with
        // a last updated time in the future is caught by this branch.
        if (score.last_engagement_time() > rebase_time) {
          score.SetLastEngagementTime(now);
        } else if (score.last_engagement_time() > last_engagement_time) {
          // This score is newer than |last_engagement_time|, but older than
          // |rebase_time|. It should still be rebased with no offset as we
          // don't accurately know what the offset should be.
          score.SetLastEngagementTime(rebase_time);
        } else {
          // Work out the offset between this score's last engagement time and
          // the last time the service recorded any engagement. Set the score's
          // last engagement time to rebase_time - offset to preserve its state,
          // relative to the rebase date. This ensures that the score will decay
          // the next time it is used, but will not decay too much.
          base::TimeDelta offset =
              last_engagement_time - score.last_engagement_time();
          base::Time rebase_score_time = rebase_time - offset;
          score.SetLastEngagementTime(rebase_score_time);
        }

        if (score.last_engagement_time() > new_last_engagement_time)
          new_last_engagement_time = score.last_engagement_time();
        score.Commit();
      }

      if (score.GetTotalScore() >
          SiteEngagementScore::GetScoreCleanupThreshold())
        continue;
    }

    // This origin has a score of 0. Wipe it from content settings.
    settings_map->SetWebsiteSettingDefaultScope(
        origin, GURL(), ContentSettingsType::SITE_ENGAGEMENT, base::Value());
  }

  // Set the last engagement time to be consistent with the scores. This will
  // only occur if |update_last_engagement_time| is true.
  if (!new_last_engagement_time.is_null())
    SetLastEngagementTime(new_last_engagement_time);
}

void SiteEngagementService::MaybeRecordMetrics() {
  base::Time now = clock_->Now();
  if (browser_context_->IsOffTheRecord() ||
      (!last_metrics_time_.is_null() &&
       (now - last_metrics_time_).InMinutes() < kMetricsIntervalInMinutes)) {
    return;
  }

  // Clean up engagement first before retrieving scores.
  if (IsLastEngagementStale())
    CleanupEngagementScores(true);

  last_metrics_time_ = now;

  // Retrieve details on a background thread as this is expensive. We may end up
  // with minor data inconsistency but this doesn't really matter for metrics
  // purposes.
  //
  // The BrowserContext and its KeyedServices are normally destroyed before the
  // ThreadPool shuts down background threads, so the task needs to hold a
  // strong reference to HostContentSettingsMap (which supports outliving the
  // browser context), and needs to avoid using any members of
  // SiteEngagementService (which does not). See https://crbug.com/900022.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetAllDetailsInBackground, now,
                     base::WrapRefCounted(
                         permissions::PermissionsClient::Get()->GetSettingsMap(
                             browser_context_)),
                     URLSets::HTTP),
      base::BindOnce(&SiteEngagementService::RecordMetrics,
                     weak_factory_.GetWeakPtr()));
}

void SiteEngagementService::RecordMetrics(
    std::vector<mojom::SiteEngagementDetails> details) {
  TRACE_EVENT0("navigation", "SiteEngagementService::RecordMetrics");
  std::sort(details.begin(), details.end(),
            [](const mojom::SiteEngagementDetails& lhs,
               const mojom::SiteEngagementDetails& rhs) {
              return lhs.total_score < rhs.total_score;
            });

  int total_origins = details.size();

  double total_engagement = 0;
  for (const auto& detail : details) {
    total_engagement += detail.total_score;
  }

  double mean_engagement =
      (total_origins == 0 ? 0 : total_engagement / total_origins);

  SiteEngagementMetrics::RecordTotalOriginsEngaged(total_origins);
  SiteEngagementMetrics::RecordMeanEngagement(mean_engagement);
  SiteEngagementMetrics::RecordMedianEngagement(
      GetMedianEngagementFromSortedDetails(details));
  SiteEngagementMetrics::RecordEngagementScores(details);
}

bool SiteEngagementService::ShouldRecordEngagement(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeUIUntrustedScheme);
}

base::Time SiteEngagementService::GetLastEngagementTime() const {
  if (browser_context_->IsOffTheRecord())
    return base::Time();

  return base::Time::FromInternalValue(
      user_prefs::UserPrefs::Get(browser_context_)
          ->GetInt64(prefs::kSiteEngagementLastUpdateTime));
}

void SiteEngagementService::SetLastEngagementTime(
    base::Time last_engagement_time) const {
  if (browser_context_->IsOffTheRecord())
    return;
  user_prefs::UserPrefs::Get(browser_context_)
      ->SetInt64(prefs::kSiteEngagementLastUpdateTime,
                 last_engagement_time.ToInternalValue());
}

base::TimeDelta SiteEngagementService::GetMaxDecayPeriod() const {
  return base::Hours(SiteEngagementScore::GetDecayPeriodInHours()) *
         SiteEngagementScore::GetMaxDecaysPerScore();
}

base::TimeDelta SiteEngagementService::GetStalePeriod() const {
  return GetMaxDecayPeriod() +
         base::Hours(
             SiteEngagementScore::GetLastEngagementGracePeriodInHours());
}

double SiteEngagementService::GetMedianEngagementFromSortedDetails(
    const std::vector<mojom::SiteEngagementDetails>& details) const {
  if (details.empty())
    return 0;

  // Calculate the median as the middle value of the sorted engagement scores
  // if there are an odd number of scores, or the average of the two middle
  // scores otherwise.
  size_t mid = details.size() / 2;
  if (details.size() % 2 == 1)
    return details[mid].total_score;
  else
    return (details[mid - 1].total_score + details[mid].total_score) / 2;
}

void SiteEngagementService::HandleMediaPlaying(
    content::WebContents* web_contents,
    bool is_hidden) {
  const GURL& url = web_contents->GetLastCommittedURL();
  if (!ShouldRecordEngagement(url))
    return;

  double old_score = GetScore(url);
  AddPoints(url, is_hidden ? SiteEngagementScore::GetHiddenMediaPoints()
                           : SiteEngagementScore::GetVisibleMediaPoints());

  MaybeRecordMetrics();
  OnEngagementEvent(
      web_contents, url,
      is_hidden ? EngagementType::kMediaHidden : EngagementType::kMediaVisible,
      old_score);
}

void SiteEngagementService::HandleNavigation(content::WebContents* web_contents,
                                             ui::PageTransition transition) {
  const GURL& url = web_contents->GetLastCommittedURL();
  if (!IsEngagementNavigation(transition) || !ShouldRecordEngagement(url))
    return;

  double old_score = GetScore(url);
  AddPoints(url, SiteEngagementScore::GetNavigationPoints());

  MaybeRecordMetrics();
  OnEngagementEvent(web_contents, url, EngagementType::kNavigation, old_score);
}

void SiteEngagementService::HandleUserInput(content::WebContents* web_contents,
                                            EngagementType type) {
  const GURL& url = web_contents->GetLastCommittedURL();
  if (!ShouldRecordEngagement(url))
    return;

  double old_score = GetScore(url);
  AddPoints(url, SiteEngagementScore::GetUserInputPoints());

  MaybeRecordMetrics();
  OnEngagementEvent(web_contents, url, type, old_score);
}

void SiteEngagementService::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    EngagementType type,
    double old_score,
    const std::optional<webapps::AppId>& app_id_override) {
  SiteEngagementMetrics::RecordEngagement(type);

  std::optional<webapps::AppId> app_id = app_id_override;
  if (!app_id && web_contents && webapps::WebappsClient::Get()) {
    app_id =
        webapps::WebappsClient::Get()->GetAppIdForWebContents(web_contents);
  }
  // TODO(crbug.com/358168777): Possibly look up the app_id from the url
  // for the notification use-case, if the notification system doesn't have the
  // app_id to provide to this system.

  double score = GetScore(url);
  for (SiteEngagementObserver& observer : observer_list_)
    observer.OnEngagementEvent(web_contents, url, score, old_score, type,
                               app_id);
}

bool SiteEngagementService::IsLastEngagementStale() const {
  // |last_engagement_time| will be null when no engagement has been recorded
  // (first run or post clearing site data), or if we are running in incognito.
  // Do not regard these cases as stale.
  base::Time last_engagement_time = GetLastEngagementTime();
  if (last_engagement_time.is_null())
    return false;

  // Stale is either too *far* back, or any amount *forward* in time. This could
  // occur due to a changed clock, or extended non-use of the browser.
  base::Time now = clock_->Now();
  return (now - last_engagement_time) >= GetStalePeriod() ||
         (now < last_engagement_time);
}

SiteEngagementScore SiteEngagementService::CreateEngagementScore(
    const GURL& origin) const {
  // If we are in incognito, |settings| will automatically have the data from
  // the original profile migrated in, so all engagement scores in incognito
  // will be initialised to the values from the original profile.
  return CreateEngagementScoreImpl(
      clock_, origin,
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context_));
}

}  // namespace site_engagement
