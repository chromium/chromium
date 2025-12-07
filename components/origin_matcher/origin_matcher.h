// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_MATCHER_ORIGIN_MATCHER_H_
#define COMPONENTS_ORIGIN_MATCHER_ORIGIN_MATCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace url {
class Origin;
}  // namespace url

namespace origin_matcher {

class OriginMatcherRule;

// An url origin matcher allows wildcard subdomain matching. It supports two
// types of rules.
//
// (1) "*"
// A single * (without quote) will match any origin.
//
// (2) SCHEME "://" [ HOSTNAME_PATTERN ][":" PORT]
//
// SCHEME is required. When matching custom schemes, HOSTNAME_PATTERN and PORT
// shouldn't present. When SCHEME is "http" or "https", HOSTNAME_PATTERN is
// required.
//
// HOSTNAME_PATTERN allows wildcard '*' to match subdomains, such as
// "*.example.com". Rules such as "x.*.y.com", "*foobar.com" are not allowed.
// Note that "*.example.com" won't match "example.com", so need another rule
// "example.com" to match it. If the HOSTNAME_PATTERN is an IP literal, it
// will be used for exact matching.
//
// PORT is optional for "http" and "https" schemes, when it is not present, for
// "http" and "https" schemes, it will match default port number (80 and 443
// correspondingly).
class OriginMatcher {
 public:
  using RuleList = std::vector<std::unique_ptr<OriginMatcherRule>>;

  OriginMatcher();
  // Allow copy and assign.
  OriginMatcher(const OriginMatcher& rhs);
  OriginMatcher(OriginMatcher&&);
  OriginMatcher& operator=(const OriginMatcher& rhs);
  OriginMatcher& operator=(OriginMatcher&&);

  ~OriginMatcher();

  void SetRules(RuleList rules);

  // Moves the rules from one origin matcher to another.
  void MoveRules(OriginMatcher& matcher);

  // Adds a rule given by the string |raw|. Returns true if the rule was
  // successfully added.
  bool AddRuleFromString(const std::string& raw);

  // Returns true if the |origin| matches any rule in this matcher.
  bool Matches(const url::Origin& origin) const;

  // Returns the current list of rules.
  const RuleList& rules() const { return rules_; }

  // Returns string representation of this origin matcher.
  std::vector<std::string> Serialize() const;

 private:
  RuleList rules_;
};

#if BUILDFLAG(IS_ANDROID)
OriginMatcher ToNativeOriginMatcher(JNIEnv* env,
                                    const jni_zero::JavaRef<jobject>& jobject);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace origin_matcher

#if BUILDFLAG(IS_ANDROID)
namespace jni_zero {
template <>
inline origin_matcher::OriginMatcher FromJniType(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  // This creates a copy of the OriginMatcher in native because the Java object
  // needs to manage the lifetime of the native instance.
  return origin_matcher::ToNativeOriginMatcher(env, obj);
}
}  // namespace jni_zero
#endif  // BUILDFLAG(IS_ANDROID)

#endif  // COMPONENTS_ORIGIN_MATCHER_ORIGIN_MATCHER_H_
