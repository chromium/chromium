// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_BROWSER_SAFE_BROWSING_URL_ALLOW_LIST_H_
#define COMPONENTS_SAFE_BROWSING_IOS_BROWSER_SAFE_BROWSING_URL_ALLOW_LIST_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

// SafeBrowsingUrlAllowList tracks the allowlist decisions for URLs for a given
// threat type, as well as decisions that are pending.  Decisions are stored for
// URLs with empty paths, meaning that allowlisted threats are allowed for the
// entire domain.
class SafeBrowsingUrlAllowList
    : public web::WebStateUserData<SafeBrowsingUrlAllowList> {
 public:
  // Enum describing the policy for navigations with a particular threat type to
  // a URL.
  enum class Policy : short { kDisallowed, kPending, kAllowed };

  // Observer class for the allow list.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the policy for navigations to |url| with |threat_type| is
    // updated to |policy|.
    virtual void ThreatPolicyUpdated(SafeBrowsingUrlAllowList* allow_list,
                                     const GURL& url,
                                     safe_browsing::SBThreatType threat_type,
                                     Policy policy) {}

    // Called when the policies for navigations to |url| with the threats in
    // |threat_types| are updated to |policy|.
    virtual void ThreatPolicyBatchUpdated(
        SafeBrowsingUrlAllowList* allow_list,
        const GURL& url,
        const std::set<safe_browsing::SBThreatType>& threat_type,
        Policy policy) {}

    // Called when |allow_list| is about to be destroyed.
    virtual void SafeBrowsingAllowListDestroyed(
        SafeBrowsingUrlAllowList* allow_list) {}
  };

  ~SafeBrowsingUrlAllowList() override;

  // Returns the URL under which allow list decisions should be stored for
  // |resource|.
  static GURL GetDecisionUrl(
      const security_interstitials::UnsafeResource& resource);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether unsafe navigations to |url| are allowed.  If |threat_types|
  // is non-null, it is populated with the allowed threat types.
  bool AreUnsafeNavigationsAllowed(
      const GURL& url,
      std::set<safe_browsing::SBThreatType>* threat_types = nullptr) const;

  // Allows future unsafe navigations to |url| that encounter threats with
  // |threat_type|.
  void AllowUnsafeNavigations(const GURL& url,
                              safe_browsing::SBThreatType threat_type);

  // Prohibits all previously allowed navigations for |url|.
  void DisallowUnsafeNavigations(const GURL& url);

  // Returns whether there are pending unsafe navigation decisions for |url|.
  // If |threat_types| is non-null, it is populated with the pending threat
  // types.
  bool IsUnsafeNavigationDecisionPending(
      const GURL& url,
      std::set<safe_browsing::SBThreatType>* threat_types = nullptr) const;

  // Records that a navigation to |url| has encountered |threat_type|, but the
  // user has not yet chosen whether to allow the navigation.
  void AddPendingUnsafeNavigationDecision(
      const GURL& url,
      safe_browsing::SBThreatType threat_type);

  // Removes all pending decisions for |url|.
  void RemovePendingUnsafeNavigationDecisions(const GURL& url);

 private:
  explicit SafeBrowsingUrlAllowList(web::WebState* web_state);
  friend class web::WebStateUserData<SafeBrowsingUrlAllowList>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Struct storing the threat types that have been allowed and those for
  // which the user has not made a decision yet.
  struct UnsafeNavigationDecisions {
    UnsafeNavigationDecisions();
    ~UnsafeNavigationDecisions();
    std::set<safe_browsing::SBThreatType> allowed_threats;
    std::set<safe_browsing::SBThreatType> pending_threats;
  };

  // Returns a reference to the UnsafeNavigationDecisions for |url|.  The path
  // is stripped from the URLs before accessing |decisions_| to allow unafe
  // navigation decisions to be shared across all URLs for a given domain.
  UnsafeNavigationDecisions& GetUnsafeNavigationDecisions(const GURL& url);
  const UnsafeNavigationDecisions& GetUnsafeNavigationDecisions(
      const GURL& url) const;

  // Setter for the policy for navigations to |url| with |threat_type|.
  void SetThreatPolicy(const GURL& url,
                       safe_browsing::SBThreatType threat_type,
                       Policy policy);

  // Returns whether the list contains any |policy| decisions for |url|.
  // Populates |threat_types| with found threats if provided.
  bool ContainsThreats(
      const GURL& url,
      Policy policy,
      std::set<safe_browsing::SBThreatType>* threat_types) const;

  // Disallows all threats that for |url| that are currently allowed under
  // |policy|.
  void RevertPolicy(const GURL& url, Policy policy);

  // The WebState whose allowed navigations are recorded by this list.
  raw_ptr<web::WebState> web_state_ = nullptr;
  // Map storing the allowlist decisions for each URL.
  std::map<GURL, UnsafeNavigationDecisions> decisions_;
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

#endif  // COMPONENTS_SAFE_BROWSING_IOS_BROWSER_SAFE_BROWSING_URL_ALLOW_LIST_H_
