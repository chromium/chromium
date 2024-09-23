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
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace content_settings {

// Note that Rules and their iterators must be destroyed before modifying the
// map that their values come from, as some types of rules hold locks on the map
// that owns their value. See UnownedRule and OriginValueMap.
struct Rule {
  Rule(ContentSettingsPattern primary_pattern,
       ContentSettingsPattern secondary_pattern,
       base::Value value,
       RuleMetaData metadata);

  Rule(const Rule&) = delete;
  Rule& operator=(const Rule&) = delete;

  Rule(Rule&& other) = delete;
  Rule& operator=(Rule&& other) = delete;

  ~Rule();

  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  base::Value value;
  RuleMetaData metadata;
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
