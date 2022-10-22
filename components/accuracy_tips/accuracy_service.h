// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_ACCURACY_SERVICE_H_
#define COMPONENTS_ACCURACY_TIPS_ACCURACY_SERVICE_H_

#include <map>
#include <memory>
#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace base {
class TimeTicks;
}

namespace content {
class WebContents;
}

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace accuracy_tips {

class AccuracyTipSafeBrowsingClient;

// Checks if URL is news-related for AccuracyTips.
// Handles rate-limiting and feature checks.
class AccuracyService : public KeyedService, history::HistoryServiceObserver {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // The site_engagement_service is currently not available on iOS
  // (crbug.com/775390), so it is supplied through a delegate.
  class Delegate {
   public:
    // Returns true if the engagement of this site is too high to show an
    // accuracy tip.
    virtual bool IsEngagementHigh(const GURL& url) = 0;

    // Shows AccuracyTip UI using the specified information if it is not already
    // showing. |close_callback| will be called when the dialog is closed.
    // The argument indicates the action that the user took to close the dialog.
    virtual void ShowAccuracyTip(
        content::WebContents* web_contents,
        AccuracyTipStatus type,
        bool show_opt_out,
        base::OnceCallback<void(AccuracyTipInteraction)> close_callback) = 0;

    // Launches accuracy tips survey with the product specific data.
    virtual void ShowSurvey(
        const std::map<std::string, bool>& product_specific_bits_data,
        const std::map<std::string, std::string>&
            product_specific_string_data) = 0;

    // Returns whether the security level of |web_contents| is secure.
    virtual bool IsSecureConnection(content::WebContents* web_contents) = 0;

    virtual ~Delegate() = default;
  };

  class Observer {
   public:
    // Called when an accuracy tip was shown.
    virtual void OnAccuracyTipShown() = 0;

    // Called when an accuracy tip was closed.
    virtual void OnAccuracyTipClosed() = 0;

    virtual ~Observer() = default;
  };

  AccuracyService(
      std::unique_ptr<Delegate> delegate,
      PrefService* pref_service,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database,
      history::HistoryService* history_service,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~AccuracyService() override;

  AccuracyService(const AccuracyService&) = delete;
  AccuracyService& operator=(const AccuracyService&) = delete;

  // Callback for accuracy check result.
  using AccuracyCheckCallback = base::OnceCallback<void(AccuracyTipStatus)>;

  // Returns the accuracy status for |url|. Virtual for testing purposes.
  virtual void CheckAccuracyStatus(const GURL& url,
                                   AccuracyCheckCallback callback);

  // Shows an accuracy tip UI for web_contents after checking rate limits.
  // Virtual for testing purposes.
  virtual void MaybeShowAccuracyTip(content::WebContents* web_contents);

  // Shows a HaTS survey after checking features states and pre-conditions
  // configured by feature params in `CanShowSurvey`.
  void MaybeShowSurvey();

  // TODO(olesiamarukhno): Figure out how to remove content dependencies if we
  // want to use this service on iOS.
  // Returns is the security level of |web_contents| is secure.
  virtual bool IsSecureConnection(content::WebContents* web_contents);

  bool IsShowingAccuracyTip(content::WebContents* web_contents);

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Adds/Removes an Observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

 private:
  void OnAccuracyStatusReceived(const GURL& url,
                                AccuracyCheckCallback callback,
                                AccuracyTipStatus status);

  void OnAccuracyTipClosed(base::TimeTicks time_opened,
                           ukm::SourceId ukm_source_id,
                           AccuracyTipInteraction interaction);

  // Returns if a HaTS survey for accuracy tips can be shown based on feature
  // state and feature params.
  bool CanShowSurvey();

  std::unique_ptr<Delegate> delegate_;
  raw_ptr<PrefService> pref_service_ = nullptr;
  scoped_refptr<AccuracyTipSafeBrowsingClient> sb_client_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  GURL url_for_last_shown_tip_;

  // Feature params:
  const GURL sample_url_;
  const base::TimeDelta time_between_prompts_;
  const bool disable_ui_ = false;

  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

  raw_ptr<content::WebContents, DanglingUntriaged>
      web_contents_showing_accuracy_tip_ = nullptr;

  base::ObserverList<Observer>::Unchecked observers_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<AccuracyService> weak_factory_{this};
};

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_SERVICE_H_
