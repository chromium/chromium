// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_IOS_CONTENT_RULE_LIST_DATA_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_IOS_CONTENT_RULE_LIST_DATA_H_

#include <optional>
#include <string>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"

namespace script_blocking {

// Manages the content rule list for script blocking.
//
// This singleton acts as a bridge between the browser-wide ComponentUpdater,
// which provides the ruleset, and per-profile services which consume it. It
// holds the latest version of the rules as a JSON string and notifies
// registered observers of any updates.
class ContentRuleListData final {
 public:
  // Observer interface for classes that need to be notified of updates to the
  // content rule list data.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnScriptBlockingRuleListUpdated(
        const std::string& rules_json) = 0;

   protected:
    ~Observer() override = default;
  };

  static ContentRuleListData& GetInstance();

  ContentRuleListData(const ContentRuleListData&) = delete;
  ContentRuleListData& operator=(const ContentRuleListData&) = delete;

  // Updates the internal rule list from a JSON string and notifies observers.
  void SetContentRuleList(std::string content_rule_list);

  // Returns a reference to the content rule list string.
  // Returns std::nullopt if the data has not yet been populated.
  const std::optional<std::string>& GetContentRuleList() const;

  // Adds/removes an observer. Observers should unregister themselves on their
  // destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Resets the singleton instance to its initial state. Only for use in tests.
  void ResetForTesting();

 private:
  friend class base::NoDestructor<ContentRuleListData>;

  ContentRuleListData();
  ~ContentRuleListData();

  SEQUENCE_CHECKER(sequence_checker_);

  // The current content rule list.
  // This is std::nullopt until the first call to SetContentRuleList().
  std::optional<std::string> content_rule_list_;

  // Observers to be notified of updates to the content rule list.
  base::ObserverList<Observer, true> observers_;
};

}  // namespace script_blocking

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_IOS_CONTENT_RULE_LIST_DATA_H_
