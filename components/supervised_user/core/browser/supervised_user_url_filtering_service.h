// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_search_api/url_checker.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "url/gurl.h"

namespace supervised_user {

class SupervisedUserService;
// Forward declarations for delegates.
class UrlFilteringDelegate;
class SupervisedUserUrlFilteringService;

enum class InterstitialMode {
  // Simple interstitial, which only shows a learn more page.
  kLearnMoreInterstitial,
  // Interstitial that allows the user to request parental review.
  kParentalReviewInterstitial,
};

// Represents the result of url filtering request.
struct WebFilteringResult {
  using Callback = base::OnceCallback<void(WebFilteringResult result)>;

  // The URL that was subject to filtering
  GURL url;
  // How the URL should be handled.
  FilteringBehavior behavior;
  // Why the URL is handled as indicated in `behavior`.
  FilteringBehaviorReason reason;
  // Details of asynchronous check if it was performed, otherwise empty.
  std::optional<safe_search_api::ClassificationDetails> async_check_details;
  // The interstitial mode to use for the URL filtering result (with a default).
  InterstitialMode interstitial_mode =
      InterstitialMode::kParentalReviewInterstitial;

  bool IsFromManualList() const {
    return reason == FilteringBehaviorReason::MANUAL;
  }
  bool IsFromDefaultSetting() const {
    return reason == FilteringBehaviorReason::DEFAULT;
  }
  bool IsAllowedBecauseOfDisabledFilter() const {
    return reason == FilteringBehaviorReason::FILTER_DISABLED &&
           behavior == FilteringBehavior::kAllow;
  }

  // True when the result of the classification means that the url is safe.
  // See `::IsClassificationSuccessful` for caveats.
  bool IsAllowed() const { return behavior == FilteringBehavior::kAllow; }
  // True when the result of the classification means that the url is not
  // safe. See `::IsClassificationSuccessful` for caveats.
  bool IsBlocked() const { return behavior == FilteringBehavior::kBlock; }

  // True when remote classification was successful. It's useful to understand
  // if the result is "allowed" because the classification succeeded, or
  // because it failed and the system uses a default classification.
  bool IsClassificationSuccessful() const {
    return !async_check_details.has_value() ||
           async_check_details->reason !=
               safe_search_api::ClassificationDetails::Reason::
                   kFailedUseDefault;
  }

  // Creates a callback for safe search api that will invoke `callback` argument
  // with check result.
  static safe_search_api::URLChecker::CheckCallback BindUrlCheckerCallback(
      Callback callback,
      const GURL& requested_url,
      InterstitialMode interstitial_mode);

  // Serializes this instance as a top level filtering result. Undefined if
  // FilteringBehavior is kInvalid.
  SupervisedUserFilterTopLevelResult ToTopLevelResult() const;
};

// Internal observer interface for communication between delegates and the
// service. Should not be used by consumers of the
// SupervisedUserUrlFilteringService.
class UrlFilteringDelegateObserver : public base::CheckedObserver {
 public:
  ~UrlFilteringDelegateObserver() override;

  // Tells that given `delegate` completed an async check for `url`. Declared as
  // pure virtual so that service must implement it.
  virtual void OnUrlChecked(const UrlFilteringDelegate& delegate,
                            WebFilteringResult result) const = 0;

  // Tells that given `delegate`'s configuration has changed and it might be
  // necessary to re-evaluate urls. Declared as pure virtual so that service
  // must implement it.
  virtual void OnUrlFilteringDelegateChanged(
      const UrlFilteringDelegate& delegate) const = 0;
};

// Interface for actual implementations of URL filtering logic. The outer
// service subscribes to individual delegates and forwards notifications to
// its own subscribers.
class UrlFilteringDelegate {
 public:
  UrlFilteringDelegate();
  virtual ~UrlFilteringDelegate();

  virtual WebFilterType GetWebFilterType() const = 0;

  // TODO(crbug.com/481303877): Reconsider naming of GetFiltering* methods.
  virtual WebFilteringResult GetFilteringBehavior(const GURL& url) const = 0;

  // TODO(crbug.com/478188599): Declare const after url_checker_ clients are
  // owned in this service and passed to delegates.
  virtual void GetFilteringBehavior(const GURL& url,
                                    bool skip_manual_parent_filter,
                                    WebFilteringResult::Callback callback,
                                    const WebFilterMetricsOptions& options) = 0;
  // TODO(crbug.com/478188599): Declare const after url_checker_ clients are
  // owned in this service and passed to delegates.
  virtual void GetFilteringBehaviorForSubFrame(
      const GURL& url,
      const GURL& main_frame_url,
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options) = 0;

  base::WeakPtr<UrlFilteringDelegate> GetWeakPtr();

  // Returns the unique name of the delegate. Used to eg.: generate histogram
  // names.
  virtual std::string_view GetName() const = 0;

  void AddObserver(UrlFilteringDelegateObserver* observer);
  void RemoveObserver(UrlFilteringDelegateObserver* observer);

 protected:
  // Informs the owning service that the delegate's configuration has changed.
  void NotifyUrlFilteringDelegateChanged() const;
  void NotifyUrlChecked(WebFilteringResult result) const;

  // Wraps the callback with a metrics callback (see ::EmitMetrics) that records
  // details about the url filtering result.
  WebFilteringResult::Callback WrapCallbackWithUrlServiceMetrics(
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options) const;

 private:
  base::ObserverList<UrlFilteringDelegateObserver> observers_;
  base::WeakPtrFactory<UrlFilteringDelegate> weak_ptr_factory_{this};
};

// Performs URL filtering workflows for supervised users, aggregating results
// from filtering delegates. Users access the service via its methods and
// subscription mechanism.
class SupervisedUserUrlFilteringService : public KeyedService,
                                          public UrlFilteringDelegateObserver {
 public:
  // External observer interface for the SupervisedUserUrlFilteringService,
  // indented for public users of the service.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    // Called when any of the delegates' configuration has changed.
    virtual void OnUrlFilteringServiceChanged() {}

    // Called when any of the delegates' url filtering result is ready.
    virtual void OnUrlChecked(WebFilteringResult result) {}
  };

  SupervisedUserUrlFilteringService(
      const SupervisedUserService& supervised_user_service,
      std::unique_ptr<UrlFilteringDelegate>
          device_parental_controls_url_filter);

  ~SupervisedUserUrlFilteringService() override;
  SupervisedUserUrlFilteringService(const SupervisedUserUrlFilteringService&) =
      delete;
  SupervisedUserUrlFilteringService& operator=(
      const SupervisedUserUrlFilteringService&) = delete;

  // Returns the type of web filter that is applied to the current profile.
  WebFilterType GetWebFilterType() const;

  // Returns the filtering status for a given URL without any remote checks.
  WebFilteringResult GetFilteringBehavior(const GURL& url) const;

  // Version of the above method that adds asynchronous checks against a
  // remote service if GetFilteringBehavior(.) was inconclusive.
  // `skip_manual_parent_filter` will ignore result from
  // GetFilteringBehavior(url) even if it was conclusive.
  void GetFilteringBehavior(
      const GURL& url,
      bool skip_manual_parent_filter,
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options = WebFilterMetricsOptions()) const;

  // Version of the above method that for use in subframe context.
  void GetFilteringBehaviorForSubFrame(
      const GURL& url,
      const GURL& main_frame_url,
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options = WebFilterMetricsOptions()) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Notifies observers that the url filtering result is ready.
  void NotifyUrlChecked(WebFilteringResult result) const;

  // UrlFilteringDelegateObserver implementation:
  void OnUrlFilteringDelegateChanged(
      const UrlFilteringDelegate& delegate) const override;
  void OnUrlChecked(const UrlFilteringDelegate& delegate,
                    WebFilteringResult result) const override;

  // Provides access to legacy way of resolving URL filtering. Temporarily, also
  // owns one of the delegates (Family Link url filter delegate).
  raw_ref<const SupervisedUserService> supervised_user_service_;
  // Owns the device parental controls url filter delegate.
  std::unique_ptr<UrlFilteringDelegate> device_parental_controls_url_filter_;

  // External observers.
  base::ObserverList<Observer> observer_list_;

  // Own observees.
  base::ScopedObservation<UrlFilteringDelegate, UrlFilteringDelegateObserver>
      family_link_url_filter_observation_{this};
  base::ScopedObservation<UrlFilteringDelegate, UrlFilteringDelegateObserver>
      device_parental_controls_url_filter_observation_{this};

  base::WeakPtrFactory<SupervisedUserUrlFilteringService> weak_ptr_factory_{
      this};
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTERING_SERVICE_H_
