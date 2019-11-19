// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blacklist_manager.h"

#include <stdint.h>
#include <memory>
#include <ostream>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

class TestingURLBlacklistManager : public URLBlacklistManager {
 public:
  explicit TestingURLBlacklistManager(PrefService* pref_service)
      : URLBlacklistManager(pref_service),
        update_called_(0),
        set_blacklist_called_(false) {}

  ~TestingURLBlacklistManager() override {}

  // Make this method public for testing.
  using URLBlacklistManager::ScheduleUpdate;

  // URLBlacklistManager overrides:
  void SetBlacklist(std::unique_ptr<URLBlacklist> blacklist) override {
    set_blacklist_called_ = true;
    URLBlacklistManager::SetBlacklist(std::move(blacklist));
  }

  void Update() override {
    update_called_++;
    URLBlacklistManager::Update();
  }

  int update_called() const { return update_called_; }
  bool set_blacklist_called() const { return set_blacklist_called_; }

 private:
  int update_called_;
  bool set_blacklist_called_;

  DISALLOW_COPY_AND_ASSIGN(TestingURLBlacklistManager);
};

class URLBlacklistManagerTest : public testing::Test {
 protected:
  URLBlacklistManagerTest() {}

  void SetUp() override {
    pref_service_.registry()->RegisterListPref(policy_prefs::kUrlBlacklist);
    pref_service_.registry()->RegisterListPref(policy_prefs::kUrlWhitelist);
    blacklist_manager_.reset(new TestingURLBlacklistManager(&pref_service_));
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    if (blacklist_manager_)
      task_environment_.RunUntilIdle();
    blacklist_manager_.reset();
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestingURLBlacklistManager> blacklist_manager_;

  base::test::TaskEnvironment task_environment_;
};

}  // namespace

// Returns whether |url| matches the |pattern|.
bool IsMatch(const std::string& pattern, const std::string& url) {
  URLBlacklist blacklist;

  // Add the pattern to blacklist.
  std::unique_ptr<base::ListValue> blocked(new base::ListValue);
  blocked->AppendString(pattern);
  blacklist.Block(blocked.get());

  return blacklist.IsURLBlocked(GURL(url));
}

// Returns the state from blacklist after adding |pattern| to be blocked or
// allowed depending on |use_whitelist| and checking |url|.
policy::URLBlacklist::URLBlacklistState GetMatch(const std::string& pattern,
                                                 const std::string& url,
                                                 const bool use_whitelist) {
  URLBlacklist blacklist;

  // Add the pattern to list.
  std::unique_ptr<base::ListValue> blocked(new base::ListValue);
  blocked->AppendString(pattern);

  if (use_whitelist) {
    blacklist.Allow(blocked.get());
  } else {
    blacklist.Block(blocked.get());
  }

  return blacklist.GetURLBlacklistState(GURL(url));
}

TEST_F(URLBlacklistManagerTest, LoadBlacklistOnCreate) {
  auto list = std::make_unique<base::ListValue>();
  list->AppendString("example.com");
  pref_service_.SetManagedPref(policy_prefs::kUrlBlacklist, std::move(list));
  auto manager = std::make_unique<URLBlacklistManager>(&pref_service_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(URLBlacklist::URL_IN_BLACKLIST,
            manager->GetURLBlacklistState(GURL("http://example.com")));
}

TEST_F(URLBlacklistManagerTest, LoadWhitelistOnCreate) {
  auto list = std::make_unique<base::ListValue>();
  list->AppendString("example.com");
  pref_service_.SetManagedPref(policy_prefs::kUrlWhitelist, std::move(list));
  auto manager = std::make_unique<URLBlacklistManager>(&pref_service_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(URLBlacklist::URL_IN_WHITELIST,
            manager->GetURLBlacklistState(GURL("http://example.com")));
}

TEST_F(URLBlacklistManagerTest, SingleUpdateForTwoPrefChanges) {
  auto blacklist = std::make_unique<base::ListValue>();
  blacklist->AppendString("*.google.com");
  auto whitelist = std::make_unique<base::ListValue>();
  whitelist->AppendString("mail.google.com");
  pref_service_.SetManagedPref(policy_prefs::kUrlBlacklist,
                               std::move(blacklist));
  pref_service_.SetManagedPref(policy_prefs::kUrlBlacklist,
                               std::move(whitelist));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, blacklist_manager_->update_called());
}

TEST_F(URLBlacklistManagerTest, Filtering) {
  URLBlacklist blacklist;

  // Block domain and all subdomains, for any filtered scheme.
  EXPECT_TRUE(IsMatch("google.com", "http://google.com"));
  EXPECT_TRUE(IsMatch("google.com", "http://google.com/"));
  EXPECT_TRUE(IsMatch("google.com", "http://google.com/whatever"));
  EXPECT_TRUE(IsMatch("google.com", "https://google.com/"));
  EXPECT_FALSE(IsMatch("google.com", "bogus://google.com/"));
  EXPECT_FALSE(IsMatch("google.com", "http://notgoogle.com/"));
  EXPECT_TRUE(IsMatch("google.com", "http://mail.google.com"));
  EXPECT_TRUE(IsMatch("google.com", "http://x.mail.google.com"));
  EXPECT_TRUE(IsMatch("google.com", "https://x.mail.google.com/"));
  EXPECT_TRUE(IsMatch("google.com", "http://x.y.google.com/a/b"));
  EXPECT_FALSE(IsMatch("google.com", "http://youtube.com/"));

  // Filter only http, ftp and ws schemes.
  EXPECT_TRUE(IsMatch("http://secure.com", "http://secure.com"));
  EXPECT_TRUE(IsMatch("http://secure.com", "http://secure.com/whatever"));
  EXPECT_TRUE(IsMatch("ftp://secure.com", "ftp://secure.com/"));
  EXPECT_TRUE(IsMatch("ws://secure.com", "ws://secure.com"));
  EXPECT_FALSE(IsMatch("http://secure.com", "https://secure.com/"));
  EXPECT_FALSE(IsMatch("ws://secure.com", "wss://secure.com"));
  EXPECT_TRUE(IsMatch("http://secure.com", "http://www.secure.com"));
  EXPECT_FALSE(IsMatch("http://secure.com", "https://www.secure.com"));
  EXPECT_FALSE(IsMatch("ws://secure.com", "wss://www.secure.com"));

  // Filter only a certain path prefix.
  EXPECT_TRUE(IsMatch("path.to/ruin", "http://path.to/ruin"));
  EXPECT_TRUE(IsMatch("path.to/ruin", "https://path.to/ruin"));
  EXPECT_TRUE(IsMatch("path.to/ruin", "http://path.to/ruins"));
  EXPECT_TRUE(IsMatch("path.to/ruin", "http://path.to/ruin/signup"));
  EXPECT_TRUE(IsMatch("path.to/ruin", "http://www.path.to/ruin"));
  EXPECT_FALSE(IsMatch("path.to/ruin", "http://path.to/fortune"));

  // Filter only a certain path prefix and scheme.
  EXPECT_TRUE(IsMatch("https://s.aaa.com/path", "https://s.aaa.com/path"));
  EXPECT_TRUE(
      IsMatch("https://s.aaa.com/path", "https://s.aaa.com/path/bbb"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "http://s.aaa.com/path"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "https://aaa.com/path"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "https://x.aaa.com/path"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "https://s.aaa.com/bbb"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "https://s.aaa.com/"));

  // Filter only ws and wss schemes.
  EXPECT_TRUE(IsMatch("ws://ws.aaa.com", "ws://ws.aaa.com"));
  EXPECT_TRUE(IsMatch("wss://ws.aaa.com", "wss://ws.aaa.com"));
  EXPECT_FALSE(IsMatch("ws://ws.aaa.com", "http://ws.aaa.com"));
  EXPECT_FALSE(IsMatch("ws://ws.aaa.com", "https://ws.aaa.com"));
  EXPECT_FALSE(IsMatch("ws://ws.aaa.com", "ftp://ws.aaa.com"));

  // Block an ip address.
  EXPECT_TRUE(IsMatch("123.123.123.123", "http://123.123.123.123/"));
  EXPECT_FALSE(IsMatch("123.123.123.123", "http://123.123.123.124/"));

  // Test exceptions to path prefixes, and most specific matches.
  std::unique_ptr<base::ListValue> blocked(new base::ListValue);
  std::unique_ptr<base::ListValue> allowed(new base::ListValue);
  blocked->AppendString("s.xxx.com/a");
  allowed->AppendString("s.xxx.com/a/b");
  blocked->AppendString("https://s.xxx.com/a/b/c");
  allowed->AppendString("https://s.xxx.com/a/b/c/d");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/x")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/d")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c/d")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/d/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c/d/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://xxx.com/a")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://xxx.com/a/b")));

  // Open an exception.
  blocked.reset(new base::ListValue);
  blocked->AppendString("google.com");
  allowed.reset(new base::ListValue);
  allowed->AppendString("plus.google.com");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://plus.google.com/")));

  // Open an exception only when using https for mail.
  allowed.reset(new base::ListValue);
  allowed->AppendString("https://mail.google.com");
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://mail.google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://www.google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://mail.google.com/")));

  // Match exactly "google.com", only for http. Subdomains without exceptions
  // are still blocked.
  allowed.reset(new base::ListValue);
  allowed->AppendString("http://.google.com");
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));

  // A smaller path match in an exact host overrides a longer path for hosts
  // that also match subdomains.
  blocked.reset(new base::ListValue);
  blocked->AppendString("yyy.com/aaa");
  blacklist.Block(blocked.get());
  allowed.reset(new base::ListValue);
  allowed->AppendString(".yyy.com/a");
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com/aaa")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com/aaa2")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://www.yyy.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.yyy.com/aaa")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.yyy.com/aaa2")));

  // If the exact entry is both allowed and blocked, allowing takes precedence.
  blocked.reset(new base::ListValue);
  blocked->AppendString("example.com");
  blacklist.Block(blocked.get());
  allowed.reset(new base::ListValue);
  allowed->AppendString("example.com");
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://example.com")));

  // Treats chrome-devtools and devtools schemes the same way.
  blocked.reset(new base::ListValue);
  blocked->AppendString("*");
  blacklist.Block(blocked.get());
  allowed.reset(new base::ListValue);
  allowed->AppendString("chrome-devtools://*");
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("devtools://something.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://something.com")));

  blocked.reset(new base::ListValue);
  blocked->AppendString("*");
  blacklist.Block(blocked.get());
  allowed.reset(new base::ListValue);
  allowed->AppendString("devtools://*");
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("devtools://something.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://something.com")));
}

TEST_F(URLBlacklistManagerTest, QueryParameters) {
  URLBlacklist blacklist;
  std::unique_ptr<base::ListValue> blocked(new base::ListValue);
  std::unique_ptr<base::ListValue> allowed(new base::ListValue);

  // Block domain and all subdomains, for any filtered scheme.
  blocked->AppendString("youtube.com");
  allowed->AppendString("youtube.com/watch?v=XYZ");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());

  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=123")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=123&v=XYZ")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ&v=123")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ&foo=bar")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?foo=bar&v=XYZ")));

  allowed.reset(new base::ListValue);
  allowed->AppendString("youtube.com/watch?av=XYZ&ag=123");
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?av=123")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?av=XYZ")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?av=123&ag=XYZ")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?ag=XYZ&av=123")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?av=XYZ&ag=123")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?ag=123&av=XYZ")));
  EXPECT_TRUE(blacklist.IsURLBlocked(
      GURL("http://youtube.com/watch?av=XYZ&ag=123&av=123")));
  EXPECT_TRUE(blacklist.IsURLBlocked(
      GURL("http://youtube.com/watch?av=XYZ&ag=123&ag=1234")));

  allowed.reset(new base::ListValue);
  allowed->AppendString("youtube.com/watch?foo=bar*&vid=2*");
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?foo=bar")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2&foo=bar")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2&foo=bar1")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=234&foo=bar")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube.com/watch?vid=234&foo=bar23")));

  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube1.com/disallow?v=44678");
  blacklist.Block(blocked.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com?v=123")));
  // Path does not match
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com?v=44678")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube1.com/disallow?v=44678")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube1.com/disallow?v=4467")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube1.com/disallow?v=4467&v=123")));
  EXPECT_TRUE(blacklist.IsURLBlocked(
      GURL("http://youtube1.com/disallow?v=4467&v=123&v=44678")));

  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube1.com/disallow?g=*");
  blacklist.Block(blocked.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com?ag=123")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube1.com/disallow?g=123")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube1.com/disallow?ag=13&g=123")));

  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube2.com/disallow?a*");
  blacklist.Block(blocked.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube2.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(
      GURL("http://youtube2.com/disallow?b=123&a21=467")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube2.com/disallow?abba=true")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube2.com/disallow?baba=true")));

  allowed.reset(new base::ListValue);
  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube3.com");
  allowed->AppendString("youtube3.com/watch?fo*");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube3.com")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube3.com/watch?b=123&a21=467")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&foo1")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&foo=bar")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&fo=ba")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube3.com/watch?foriegn=true")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube3.com/watch?fold")));

  allowed.reset(new base::ListValue);
  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube4.com");
  allowed->AppendString("youtube4.com?*");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube4.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube4.com/?hello")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube4.com/?foo")));

  allowed.reset(new base::ListValue);
  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube5.com?foo=bar");
  allowed->AppendString("youtube5.com?foo1=bar1&foo2=bar2&");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube5.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube5.com/?foo=bar&a=b")));
  // More specific filter is given precedence.
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube5.com/?a=b&foo=bar&foo1=bar1&foo2=bar2")));
}

TEST_F(URLBlacklistManagerTest, BlockAllWithExceptions) {
  URLBlacklist blacklist;

  std::unique_ptr<base::ListValue> blocked(new base::ListValue);
  std::unique_ptr<base::ListValue> allowed(new base::ListValue);
  blocked->AppendString("*");
  allowed->AppendString(".www.google.com");
  allowed->AppendString("plus.google.com");
  allowed->AppendString("https://mail.google.com");
  allowed->AppendString("https://very.safe/path");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://random.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://s.www.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://plus.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.plus.google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://mail.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://mail.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.mail.google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://very.safe/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://very.safe/path")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://very.safe/path")));
}

TEST_F(URLBlacklistManagerTest, DefaultBlacklistExceptions) {
  URLBlacklist blacklist;
  std::unique_ptr<base::ListValue> blocked(new base::ListValue);

  // Blacklist everything:
  blocked->AppendString("*");
  blacklist.Block(blocked.get());

  // Internal NTP and extension URLs are not blocked by the "*":
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-extension://xyz"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-search://local-ntp"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-native://ntp"))));

  // Unless they are explicitly blacklisted:
  blocked->AppendString("chrome-extension://*");
  std::unique_ptr<base::ListValue> allowed(new base::ListValue);
  allowed->AppendString("chrome-extension://abc");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());

  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_TRUE((blacklist.IsURLBlocked(GURL("chrome-extension://xyz"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-extension://abc"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-search://local-ntp"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-native://ntp"))));
}

TEST_F(URLBlacklistManagerTest, BlacklistBasicCoverage) {
  // Tests to cover the documentation from
  // http://www.chromium.org/administrators/url-blacklist-filter-format

  // [scheme://][.]host[:port][/path][@query]
  // Scheme can be http, https, ftp, chrome, etc. This field is optional, and
  // must be followed by '://'.
  EXPECT_TRUE(IsMatch("file://*", "file:///abc.txt"));
  EXPECT_TRUE(IsMatch("file:*", "file:///usr/local/boot.txt"));
  EXPECT_TRUE(IsMatch("https://*", "https:///abc.txt"));
  EXPECT_TRUE(IsMatch("ftp://*", "ftp://ftp.txt"));
  EXPECT_TRUE(IsMatch("chrome://*", "chrome:policy"));
  EXPECT_TRUE(IsMatch("noscheme", "http://noscheme"));
  // Filter custom schemes.
  EXPECT_TRUE(IsMatch("custom://*", "custom://example_app"));
  EXPECT_TRUE(IsMatch("custom:*", "custom:example2_app"));
  EXPECT_FALSE(IsMatch("custom://*", "customs://example_apps"));
  EXPECT_FALSE(IsMatch("custom://*", "cust*://example_ap"));
  EXPECT_FALSE(IsMatch("custom://*", "ecustom:example_app"));
  EXPECT_TRUE(IsMatch("custom://*", "custom:///abc.txt"));
  // Tests for custom scheme patterns that are not supported.
  EXPECT_FALSE(IsMatch("wrong://app", "wrong://app"));
  EXPECT_FALSE(IsMatch("wrong ://*", "wrong ://app"));
  EXPECT_FALSE(IsMatch(" wrong:*", " wrong://app"));

  // Ommitting the scheme matches most standard schemes.
  EXPECT_TRUE(IsMatch("example.com", "chrome:example.com"));
  EXPECT_TRUE(IsMatch("example.com", "chrome://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "file://example.com/"));
  EXPECT_TRUE(IsMatch("example.com", "ftp://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "http://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "https://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "ws://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "wss://example.com"));

  // Some schemes are not matched when the scheme is ommitted.
  EXPECT_FALSE(IsMatch("example.com", "about://example.com"));
  EXPECT_FALSE(IsMatch("example.com", "about:example.com"));
  EXPECT_FALSE(IsMatch("example.com/*", "filesystem:///something"));
  EXPECT_FALSE(IsMatch("example.com", "custom://example.com"));
  EXPECT_FALSE(IsMatch("example", "custom://example"));
  EXPECT_FALSE(IsMatch("example.com", "gopher://example.com"));

  // An optional '.' (dot) can prefix the host field to disable subdomain
  // matching, see below for details.
  EXPECT_TRUE(IsMatch(".example.com", "http://example.com/path"));
  EXPECT_FALSE(IsMatch(".example.com", "http://mail.example.com/path"));
  EXPECT_TRUE(IsMatch("example.com", "http://mail.example.com/path"));
  EXPECT_TRUE(IsMatch("ftp://.ftp.file", "ftp://ftp.file"));
  EXPECT_FALSE(IsMatch("ftp://.ftp.file", "ftp://sub.ftp.file"));

  // The host field is required, and is a valid hostname or an IP address. It
  // can also take the special '*' value, see below for details.
  EXPECT_TRUE(IsMatch("*", "http://anything"));
  EXPECT_TRUE(IsMatch("*", "ftp://anything"));
  EXPECT_TRUE(IsMatch("*", "custom://anything"));
  EXPECT_TRUE(IsMatch("host", "http://host:8080"));
  EXPECT_FALSE(IsMatch("host", "file:///host"));
  EXPECT_TRUE(IsMatch("10.1.2.3", "http://10.1.2.3:8080/path"));
  // No host, will match nothing.
  EXPECT_FALSE(IsMatch(":8080", "http://host:8080"));
  EXPECT_FALSE(IsMatch(":8080", "http://:8080"));

  // An optional port can come after the host. It must be a valid port value
  // from 1 to 65535.
  EXPECT_TRUE(IsMatch("host:8080", "http://host:8080/path"));
  EXPECT_TRUE(IsMatch("host:1", "http://host:1/path"));
  // Out of range port.
  EXPECT_FALSE(IsMatch("host:65536", "http://host:65536/path"));
  // Star is not allowed in port numbers.
  EXPECT_FALSE(IsMatch("example.com:*", "http://example.com"));
  EXPECT_FALSE(IsMatch("example.com:*", "http://example.com:8888"));

  // An optional path can come after port.
  EXPECT_TRUE(IsMatch("host/path", "http://host:8080/path"));
  EXPECT_TRUE(IsMatch("host/path/path2", "http://host/path/path2"));
  EXPECT_TRUE(IsMatch("host/path", "http://host/path/path2"));

  // An optional query can come in the end, which is a set of key-value and
  // key-only tokens delimited by '&'. The key-value tokens are separated
  // by '='. A query token can optionally end with a '*' to indicate prefix
  // match. Token order is ignored during matching.
  EXPECT_TRUE(IsMatch("host?q1=1&q2=2", "http://host?q2=2&q1=1"));
  EXPECT_FALSE(IsMatch("host?q1=1&q2=2", "http://host?q2=1&q1=2"));
  EXPECT_FALSE(IsMatch("host?q1=1&q2=2", "http://host?Q2=2&Q1=1"));
  EXPECT_TRUE(IsMatch("host?q1=1&q2=2", "http://host?q2=2&q1=1&q3=3"));
  EXPECT_TRUE(IsMatch("host?q1=1&q2=2*", "http://host?q2=21&q1=1&q3=3"));

  // user:pass fields can be included but will be ignored
  // (e.g. http://user:pass@ftp.example.com/pub/bigfile.iso).
  EXPECT_TRUE(
      IsMatch("host.com/path", "http://user:pass@host.com:8080/path"));
  EXPECT_TRUE(
      IsMatch("ftp://host.com/path", "ftp://user:pass@host.com:8080/path"));

  // Case sensitivity.
  // Scheme is case insensitive.
  EXPECT_TRUE(IsMatch("suPPort://*", "support:example"));
  EXPECT_TRUE(IsMatch("FILE://*", "file:example"));
  EXPECT_TRUE(IsMatch("FILE://*", "FILE://example"));
  EXPECT_TRUE(IsMatch("FtP:*", "ftp://example"));
  EXPECT_TRUE(IsMatch("http://example.com", "HTTP://example.com"));
  EXPECT_TRUE(IsMatch("HTTP://example.com", "http://example.com"));
  // Host is case insensitive.
  EXPECT_TRUE(IsMatch("http://EXAMPLE.COM", "http://example.com"));
  EXPECT_TRUE(IsMatch("Example.com", "http://examplE.com/Path?Query=1"));
  // Path is case sensitive.
  EXPECT_FALSE(IsMatch("example.com/Path", "http://example.com/path"));
  EXPECT_TRUE(IsMatch("http://example.com/aB", "http://example.com/aB"));
  EXPECT_FALSE(IsMatch("http://example.com/aB", "http://example.com/Ab"));
  EXPECT_FALSE(IsMatch("http://example.com/aB", "http://example.com/ab"));
  EXPECT_FALSE(IsMatch("http://example.com/aB", "http://example.com/AB"));
  // Query is case sensitive.
  EXPECT_FALSE(IsMatch("host/path?Query=1", "http://host/path?query=1"));
}

// Test for GetURLBlacklistState method.
TEST_F(URLBlacklistManagerTest, UseBlacklistState) {
  const policy::URLBlacklist::URLBlacklistState in_blacklist =
      policy::URLBlacklist::URLBlacklistState::URL_IN_BLACKLIST;
  const policy::URLBlacklist::URLBlacklistState in_whitelist =
      policy::URLBlacklist::URLBlacklistState::URL_IN_WHITELIST;
  const policy::URLBlacklist::URLBlacklistState neutral_state =
      policy::URLBlacklist::URLBlacklistState::URL_NEUTRAL_STATE;

  // Test whitelist states.
  EXPECT_EQ(in_whitelist, GetMatch("example.com", "http://example.com", true));
  EXPECT_EQ(in_whitelist, GetMatch("http://*", "http://example.com", true));
  EXPECT_EQ(in_whitelist, GetMatch("custom://*", "custom://app", true));
  EXPECT_EQ(in_whitelist, GetMatch("custom:*", "custom://app/play", true));
  EXPECT_EQ(in_whitelist, GetMatch("custom:*", "custom://app:8080", true));
  // Test blacklist states.
  EXPECT_EQ(in_blacklist, GetMatch("ftp:*", "ftp://server", false));
  // Test neutral states.
  EXPECT_EQ(neutral_state, GetMatch("file:*", "http://example.com", true));
  EXPECT_EQ(neutral_state, GetMatch("https://*", "http://example.com", false));
}
}  // namespace policy
