// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for objects providing content setting rules.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_RULE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_RULE_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace content_settings {

class SCOPED_LOCKABLE RefCountedAutoLock
    : public base::RefCounted<RefCountedAutoLock> {
 public:
  explicit RefCountedAutoLock(base::Lock& lock);

 protected:
  virtual ~RefCountedAutoLock();

 private:
  friend class base::RefCounted<RefCountedAutoLock>;

  base::AutoLock auto_lock_;
};

// Note that Rules and their iterators must be destroyed before modifying the
// map that their values come from, as some types of rules hold locks on the map
// that owns their value. See UnownedRule and OriginIdentifierValueMap.
struct Rule {
  Rule(const ContentSettingsPattern& primary_pattern,
       const ContentSettingsPattern& secondary_pattern,
       const RuleMetaData& metadata);

  Rule(const Rule&) = delete;
  Rule& operator=(const Rule&) = delete;

  Rule(Rule&& other) = delete;
  Rule& operator=(Rule&& other) = delete;

  virtual ~Rule();

  virtual const base::Value& value() const = 0;
  virtual base::Value TakeValue() = 0;

  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  RuleMetaData metadata;
};

// Note that this Rule doesn't actually own its value, it's usually just a
// pointer to the OriginIdentifierValueMap that created it.
struct UnownedRule : public Rule {
  UnownedRule(const ContentSettingsPattern& primary_pattern,
              const ContentSettingsPattern& secondary_pattern,
              const base::Value* unowned_value,
              scoped_refptr<RefCountedAutoLock> value_lock,
              const RuleMetaData& metadata);

  ~UnownedRule() override;

  const base::Value& value() const override;
  base::Value TakeValue() override;

  // Owned by the creator of the Rule, usually an OriginIdentifierValueMap.
  raw_ptr<const base::Value> unowned_value;
  // A lock held to ensure |unowned_value| is not modified/destroyed before
  // this Rule is.
  scoped_refptr<RefCountedAutoLock> value_lock;
};

struct OwnedRule : public Rule {
  OwnedRule(const ContentSettingsPattern& primary_pattern,
            const ContentSettingsPattern& secondary_pattern,
            base::Value unowned_value,
            const RuleMetaData& metadata);

  ~OwnedRule() override;

  const base::Value& value() const override;
  base::Value TakeValue() override;

  base::Value owned_value;
};

class RuleIterator {
 public:
  virtual ~RuleIterator();
  virtual bool HasNext() const = 0;
  virtual std::unique_ptr<Rule> Next() = 0;
};

class ConcatenationIterator : public RuleIterator {
 public:
  explicit ConcatenationIterator(
      std::vector<std::unique_ptr<RuleIterator>> iterators);
  ~ConcatenationIterator() override;
  bool HasNext() const override;
  std::unique_ptr<Rule> Next() override;

 private:
  std::vector<std::unique_ptr<RuleIterator>> iterators_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_RULE_H_
