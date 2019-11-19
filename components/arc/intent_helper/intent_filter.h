// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_
#define COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_

#include <string>
#include <vector>

#include "base/macros.h"

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

    AuthorityEntry& operator=(AuthorityEntry&& other);

    bool Match(const GURL& url) const;

    const std::string& host() const { return host_; }
    int port() const { return port_; }

   private:
    std::string host_;
    bool wild_;
    int port_;

    DISALLOW_COPY_AND_ASSIGN(AuthorityEntry);
  };

  // A helper class for handling matching of various patterns in the URL.
  class PatternMatcher {
   public:
    PatternMatcher();
    PatternMatcher(PatternMatcher&& other);
    PatternMatcher(const std::string& pattern, mojom::PatternType match_type);

    PatternMatcher& operator=(PatternMatcher&& other);

    bool Match(const std::string& match) const;

    const std::string& pattern() const { return pattern_; }
    mojom::PatternType match_type() const { return match_type_; }

   private:
    std::string pattern_;
    mojom::PatternType match_type_;

    DISALLOW_COPY_AND_ASSIGN(PatternMatcher);
  };

  IntentFilter();
  IntentFilter(IntentFilter&& other);
  IntentFilter(const std::string& package_name,
               std::vector<AuthorityEntry> authorities,
               std::vector<PatternMatcher> paths,
               std::vector<std::string> schemes);
  ~IntentFilter();

  IntentFilter& operator=(IntentFilter&& other);

  bool Match(const GURL& url) const;

  const std::string& package_name() const { return package_name_; }
  const std::vector<AuthorityEntry>& authorities() const {
    return authorities_;
  }
  const std::vector<PatternMatcher>& paths() const { return paths_; }
  const std::vector<std::string>& schemes() const { return schemes_; }

 private:
  bool MatchDataAuthority(const GURL& url) const;
  bool HasDataPath(const GURL& url) const;

  std::string package_name_;
  std::vector<AuthorityEntry> authorities_;
  std::vector<PatternMatcher> paths_;
  std::vector<std::string> schemes_;

  DISALLOW_COPY_AND_ASSIGN(IntentFilter);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_INTENT_FILTER_H_
