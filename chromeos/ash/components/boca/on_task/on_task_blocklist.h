// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_BLOCKLIST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_BLOCKLIST_H_

#include <map>
#include <memory>

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// The OnTaskBlocklist is responsible for setting the appropriate url navigation
// restrictions for each tab.
class OnTaskBlocklist {
 public:
  enum class RestrictionLevel {
    kNoRestrictions = 1,               // No url restrictions.
    kLimitedNavigation,                // Only allow exact URL match.
    kSameDomainNavigation,             // Allow domain/subdomain navigation.
    kOneLevelDeepNavigation,           // Allow one level deeper navigation.
    kDomainAndOneLevelDeepNavigation,  // Allows same domain traversal and one
                                       // level deep.
  };

  // BlocklistSource implementation that blocks all traffic with the
  // exception of URLs specified by the teacher's navigation restriction level.
  // Note that this implementation only supports one observer at a time. Adding
  // a new observer will remove the previous one. These should only be called
  // from the main thread.
  class OnTaskBlocklistSource : public policy::BlocklistSource {
   public:
    OnTaskBlocklistSource(const GURL& url,
                          OnTaskBlocklist::RestrictionLevel restriction_type);
    OnTaskBlocklistSource(const OnTaskBlocklistSource&) = delete;
    OnTaskBlocklistSource& operator=(const OnTaskBlocklistSource&) = delete;
    ~OnTaskBlocklistSource() override = default;

    const base::Value::List* GetBlocklistSpec() const override;
    const base::Value::List* GetAllowlistSpec() const override;
    void SetBlocklistObserver(base::RepeatingClosure observer) override {}

   private:
    base::Value::List blocklist_;
    base::Value::List allowlist_;
  };

  explicit OnTaskBlocklist(
      std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager);
  OnTaskBlocklist(const OnTaskBlocklist&) = delete;
  OnTaskBlocklist& operator=(const OnTaskBlocklist&) = delete;
  ~OnTaskBlocklist();

  // Returns the URLBlocklistState for the given url.
  policy::URLBlocklist::URLBlocklistState GetURLBlocklistState(
      const GURL& url) const;

  // Sets the url restrictions for the given `url` with `restriction_level`.
  // This is different from `SetParentURLRestrictionLevel` since this can be
  // called on newly navigated urls not sent by the boca producer. True
  // represents we are able to set the restrictions for the tab, false
  // otherwise. It should only be true if it's a new tab.
  bool MaybeSetURLRestrictionLevel(
      content::WebContents* tab,
      const GURL& url,
      OnTaskBlocklist::RestrictionLevel restriction_level);

  // Sets the url restrictions for the given `url` with `restriction_level`.
  // Should only be called for the set of urls sent by the boca producer.
  void SetParentURLRestrictionLevel(
      content::WebContents* tab,
      const GURL& url,
      OnTaskBlocklist::RestrictionLevel restriction_level);

  // Updates the blocklist that is associated with the given `tab`. This is
  // triggered on an active tab change or when the current tab changes.
  void RefreshForUrlBlocklist(content::WebContents* tab);

  // Remove the `tab` from the `parent_tab_to_nav_filters_`;
  void RemoveParentFilter(content::WebContents* tab);

  // Remove the `tab` from the `child_tab_to_nav_filters_`;
  void RemoveChildFilter(content::WebContents* tab);

  void CleanupBlocklist();

  // Returns true if the tab can perform one level deep. If the current
  // restriction level is not `kOneLevelDeepNavigation`, then this will return
  // false. This should only be called in a block that checks that the current
  // restriction level is for one level deep navigation.
  bool CanPerformOneLevelNavigation(content::WebContents* tab);

  bool IsCurrentRestrictionOneLevelDeep();

  // Returns true if the `tab` is a parent tab. A parent tab is any tab that was
  // sent as part of a session bundle. Any other tab created (either via
  // ctrl+left click or a link click that sets itself to open in a new window)
  // during the session by the user is a child tab. Parent tabs should not
  // be closed during any point of an ongoing session.
  bool IsParentTab(content::WebContents* tab);

  content::WebContents* previous_tab();

  const policy::URLBlocklistManager* url_blocklist_manager();
  std::map<SessionID, OnTaskBlocklist::RestrictionLevel>
  parent_tab_to_nav_filters();
  std::map<SessionID, OnTaskBlocklist::RestrictionLevel>
  child_tab_to_nav_filters();
  std::map<SessionID, GURL> one_level_deep_original_url();
  OnTaskBlocklist::RestrictionLevel current_page_restriction_level();

 private:
  OnTaskBlocklist::RestrictionLevel current_page_restriction_level_ =
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions;
  base::WeakPtr<content::WebContents> previous_tab_;
  GURL previous_url_;
  bool first_time_popup_ = true;
  std::map<SessionID, OnTaskBlocklist::RestrictionLevel>
      parent_tab_to_nav_filters_;
  std::map<SessionID, OnTaskBlocklist::RestrictionLevel>
      child_tab_to_nav_filters_;
  std::map<SessionID, GURL> one_level_deep_original_url_;
  const std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager_;
  base::WeakPtrFactory<OnTaskBlocklist> weak_pointer_factory_{this};
};
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_BLOCKLIST_H_
