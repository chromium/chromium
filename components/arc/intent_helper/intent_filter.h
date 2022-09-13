// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_
#define COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_

#include <string>
#include <vector>

class GURL;

namespace arc {

namespace mojom {
enum class PatternType;
}  // namespace mojom

// A chrome-side implementation of Android's IntentFilter class.  This is used
// to approximate the intent filtering and determine whether a given URL is
// likely to be handled by any android-side apps, prior to making expensive IPC
// calls.
class IntentFilter {
 public:
  // A helper class for handling matching of the host part of the URL.
  class AuthorityEntry {
   public:
    AuthorityEntry();
    AuthorityEntry(AuthorityEntry&& other);
    AuthorityEntry(const std::string& host, int port);
    AuthorityEntry(const AuthorityEntry&) = delete;
    AuthorityEntry& operator=(const AuthorityEntry&) = delete;
    AuthorityEntry& operator=(AuthorityEntry&& other);

    const std::string& host() const { return host_; }
    int port() const { return port_; }
    bool wild() const { return wild_; }

   private:
    std::string host_;
    bool wild_;
    int port_;
  };

  // A helper class for handling matching of various patterns in the URL.
  class PatternMatcher {
   public:
    PatternMatcher();
    PatternMatcher(PatternMatcher&& other);
    PatternMatcher(const std::string& pattern, mojom::PatternType match_type);
    PatternMatcher(const PatternMatcher&) = delete;
    PatternMatcher& operator=(const PatternMatcher&) = delete;
    PatternMatcher& operator=(PatternMatcher&& other);

    const std::string& pattern() const { return pattern_; }
    mojom::PatternType match_type() const { return match_type_; }

   private:
    std::string pattern_;
    mojom::PatternType match_type_;
  };

  IntentFilter();
  IntentFilter(IntentFilter&& other);
  IntentFilter(const std::string& package_name,
               std::vector<std::string> actions,
               std::vector<AuthorityEntry> authorities,
               std::vector<PatternMatcher> paths,
               std::vector<std::string> schemes,
               std::vector<std::string> mime_types);
  IntentFilter(const std::string& package_name,
               const std::string& activity_name,
               const std::string& activity_label,
               std::vector<std::string> actions,
               std::vector<IntentFilter::AuthorityEntry> authorities,
               std::vector<IntentFilter::PatternMatcher> paths,
               std::vector<std::string> schemes,
               std::vector<std::string> mime_types);
  IntentFilter(const IntentFilter&) = delete;
  IntentFilter& operator=(const IntentFilter&) = delete;
  IntentFilter& operator=(IntentFilter&& other);
  ~IntentFilter();

  bool Match(const GURL& url) const;

  const std::string& package_name() const { return package_name_; }
  const std::string& activity_name() const { return activity_name_; }
  const std::string& activity_label() const { return activity_label_; }
  const std::vector<std::string>& actions() const { return actions_; }
  const std::vector<AuthorityEntry>& authorities() const {
    return authorities_;
  }
  const std::vector<PatternMatcher>& paths() const { return paths_; }
  const std::vector<std::string>& schemes() const { return schemes_; }
  const std::vector<std::string>& mime_types() const { return mime_types_; }

 private:
  std::string package_name_;
  std::string activity_name_;
  std::string activity_label_;
  std::vector<std::string> actions_;
  std::vector<AuthorityEntry> authorities_;
  std::vector<PatternMatcher> paths_;
  std::vector<std::string> schemes_;
  std::vector<std::string> mime_types_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_
