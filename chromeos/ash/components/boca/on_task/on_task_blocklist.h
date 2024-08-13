// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_BLOCKLIST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_BLOCKLIST_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
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
  // called on newly navigated urls not sent by the boca producer.
  void SetURLRestrictionLevel(
      const GURL& url,
      OnTaskBlocklist::RestrictionLevel restriction_level);

  // Sets the url restrictions for the given `url` with `restriction_level`.
  // Should only be called for the set of urls sent by the boca producer.
  void SetParentURLRestrictionLevel(
      const GURL& url,
      OnTaskBlocklist::RestrictionLevel restriction_level);

  // Updates the blocklist that is associated with the given `url`. This is
  // triggered on an active tab change or when the current tab changes.
  void RefreshForUrlBlocklist(const GURL& url);

  // Remove the `url` from the `child_tab_url_to_nav_filters_`;
  void RemoveChildFilter(const GURL& url);

  void CleanupBlocklist();

  const policy::URLBlocklistManager* url_blocklist_manager();
  std::map<std::string, OnTaskBlocklist::RestrictionLevel>
  parent_tab_url_to_nav_filters();
  std::map<std::string, OnTaskBlocklist::RestrictionLevel>
  child_tab_url_to_nav_filters();
  OnTaskBlocklist::RestrictionLevel current_page_restriction_level();

 private:
  OnTaskBlocklist::RestrictionLevel current_page_restriction_level_ =
      OnTaskBlocklist::RestrictionLevel::kNoRestrictions;
  GURL previous_url_;
  bool first_time_popup_ = true;
  std::map<std::string, OnTaskBlocklist::RestrictionLevel>
      parent_tab_url_to_nav_filters_;
  std::map<std::string, OnTaskBlocklist::RestrictionLevel>
      child_tab_url_to_nav_filters_;
  const std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager_;
  base::WeakPtrFactory<OnTaskBlocklist> weak_pointer_factory_{this};
};
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_BLOCKLIST_H_
