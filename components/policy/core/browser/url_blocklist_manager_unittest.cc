// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_manager.h"

#include <stdint.h>

#include <memory>
#include <ostream>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_features.h"

namespace policy {

namespace {

class TestingURLBlocklistManager : public URLBlocklistManager {
 public:
  explicit TestingURLBlocklistManager(PrefService* pref_service)
      : URLBlocklistManager(pref_service,
                            policy_prefs::kUrlBlocklist,
                            policy_prefs::kUrlAllowlist),
        update_called_(0),
        set_blocklist_called_(false) {}
  TestingURLBlocklistManager(const TestingURLBlocklistManager&) = delete;
  TestingURLBlocklistManager& operator=(const TestingURLBlocklistManager&) =
      delete;

  ~TestingURLBlocklistManager() override = default;

  // Make this method public for testing.
  using URLBlocklistManager::ScheduleUpdate;

  // URLBlocklistManager overrides:
  void SetBlocklist(std::unique_ptr<URLBlocklist> blocklist) override {
    set_blocklist_called_ = true;
    URLBlocklistManager::SetBlocklist(std::move(blocklist));
  }

  void Update() override {
    update_called_++;
    URLBlocklistManager::Update();
  }

  int update_called() const { return update_called_; }
  bool set_blocklist_called() const { return set_blocklist_called_; }

 private:
  int update_called_;
  bool set_blocklist_called_;
};

class URLBlocklistManagerTest : public testing::Test {
 protected:
  URLBlocklistManagerTest() = default;

  void SetUp() override {
    pref_service_.registry()->RegisterListPref(policy_prefs::kUrlBlocklist);
    pref_service_.registry()->RegisterListPref(policy_prefs::kUrlAllowlist);
#if BUILDFLAG(IS_CHROMEOS)
    pref_service_.registry()->RegisterListPref(
        policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist);
#endif
    blocklist_manager_ =
        std::make_unique<TestingURLBlocklistManager>(&pref_service_);
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    if (blocklist_manager_)
      task_environment_.RunUntilIdle();
    blocklist_manager_.reset();
  }

  void SetUrlBlocklistPref(base::Value::List values) {
    pref_service_.SetManagedPref(policy_prefs::kUrlBlocklist,
                                 std::move(values));
  }

  void SetUrlAllowlistPref(base::Value::List values) {
    pref_service_.SetManagedPref(policy_prefs::kUrlAllowlist,
                                 std::move(values));
  }

  TestingURLBlocklistManager* blocklist_manager() {
    return blocklist_manager_.get();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestingURLBlocklistManager> blocklist_manager_;

  base::test::TaskEnvironment task_environment_;
};

// Returns whether |url| matches the |pattern|.
bool MatchesPattern(const std::string& pattern, const std::string& url) {
  URLBlocklist blocklist;
  blocklist.Block(base::Value::List().Append(pattern));
  return blocklist.IsURLBlocked(GURL(url));
}

// Returns the state from blocklist after adding |pattern| to be blocked or
// allowed depending on |use_allowlist| and checking |url|.
URLBlocklist::URLBlocklistState GetUrlBlocklistStateAfterAddingPattern(
    const std::string& pattern,
    const std::string& url,
    const bool use_allowlist) {
  URLBlocklist blocklist;
  if (use_allowlist) {
    blocklist.Allow(base::Value::List().Append(pattern));
  } else {
    blocklist.Block(base::Value::List().Append(pattern));
  }
  return blocklist.GetURLBlocklistState(GURL(url));
}

// Returns the URL blocklist state after adding the pattern to the blocklist.
URLBlocklist::URLBlocklistState GetUrlBlocklistStateAfterBlocking(
    const std::string& pattern,
    const std::string& url) {
  return GetUrlBlocklistStateAfterAddingPattern(pattern, url,
                                                /*use_allowlist=*/false);
}

// Returns the URL blocklist state after adding the pattern to the allowlist.
URLBlocklist::URLBlocklistState GetUrlBlocklistStateAfterAllowing(
    const std::string& pattern,
    const std::string& url) {
  return GetUrlBlocklistStateAfterAddingPattern(pattern, url,
                                                /*use_allowlist=*/true);
}

}  // namespace

TEST_F(URLBlocklistManagerTest, LoadBlocklistOnCreate) {
  SetUrlBlocklistPref(base::Value::List().Append("example.com"));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(
      URLBlocklist::URL_IN_BLOCKLIST,
      blocklist_manager()->GetURLBlocklistState(GURL("http://example.com")));
}

TEST_F(URLBlocklistManagerTest, LoadAllowlistOnCreate) {
  SetUrlAllowlistPref(base::Value::List().Append("example.com"));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(
      URLBlocklist::URL_IN_ALLOWLIST,
      blocklist_manager()->GetURLBlocklistState(GURL("http://example.com")));
}

TEST_F(URLBlocklistManagerTest, SingleUpdateForTwoPrefChanges) {
  SetUrlBlocklistPref(base::Value::List().Append("*.google.com"));
  SetUrlBlocklistPref(base::Value::List().Append("mail.google.com"));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1, blocklist_manager()->update_called());
}

// Non-special URLs behavior is affected by the
// StandardCompliantNonSpecialSchemeURLParsing feature.
// See https://crbug.com/40063064 for details.
class URLBlocklistParamTest : public ::testing::Test,
                              public ::testing::WithParamInterface<bool> {
 public:
  URLBlocklistParamTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    if (use_standard_compliant_non_special_scheme_url_parsing_) {
      scoped_feature_list_.InitAndEnableFeature(
          url::kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          url::kStandardCompliantNonSpecialSchemeURLParsing);
    }
  }

 protected:
  bool use_standard_compliant_non_special_scheme_url_parsing() const {
    return use_standard_compliant_non_special_scheme_url_parsing_;
  }

 private:
  bool use_standard_compliant_non_special_scheme_url_parsing_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(URLBlocklistParamTest, Filtering) {
  URLBlocklist blocklist;

  // Block domain and all subdomains, for any filtered scheme.
  EXPECT_TRUE(MatchesPattern("google.com", "http://google.com"));
  EXPECT_TRUE(MatchesPattern("google.com", "http://google.com/"));
  EXPECT_TRUE(MatchesPattern("google.com", "http://google.com/whatever"));
  EXPECT_TRUE(MatchesPattern("google.com", "https://google.com/"));
  if (use_standard_compliant_non_special_scheme_url_parsing()) {
    // When the feature is enabled, the host part in non-special URLs can be
    // recognized.
    EXPECT_TRUE(MatchesPattern("google.com", "bogus://google.com/"));
  } else {
    EXPECT_FALSE(MatchesPattern("google.com", "bogus://google.com/"));
  }
  EXPECT_FALSE(MatchesPattern("google.com", "http://notgoogle.com/"));
  EXPECT_TRUE(MatchesPattern("google.com", "http://mail.google.com"));
  EXPECT_TRUE(MatchesPattern("google.com", "http://x.mail.google.com"));
  EXPECT_TRUE(MatchesPattern("google.com", "https://x.mail.google.com/"));
  EXPECT_TRUE(MatchesPattern("google.com", "http://x.y.google.com/a/b"));
  EXPECT_FALSE(MatchesPattern("google.com", "http://youtube.com/"));

  // Filter only http, ftp and ws schemes.
  EXPECT_TRUE(MatchesPattern("http://secure.com", "http://secure.com"));
  EXPECT_TRUE(
      MatchesPattern("http://secure.com", "http://secure.com/whatever"));
  EXPECT_TRUE(MatchesPattern("ftp://secure.com", "ftp://secure.com/"));
  EXPECT_TRUE(MatchesPattern("ws://secure.com", "ws://secure.com"));
  EXPECT_FALSE(MatchesPattern("http://secure.com", "https://secure.com/"));
  EXPECT_FALSE(MatchesPattern("ws://secure.com", "wss://secure.com"));
  EXPECT_TRUE(MatchesPattern("http://secure.com", "http://www.secure.com"));
  EXPECT_FALSE(MatchesPattern("http://secure.com", "https://www.secure.com"));
  EXPECT_FALSE(MatchesPattern("ws://secure.com", "wss://www.secure.com"));

  // Filter only a certain path prefix.
  EXPECT_TRUE(MatchesPattern("path.to/ruin", "http://path.to/ruin"));
  EXPECT_TRUE(MatchesPattern("path.to/ruin", "https://path.to/ruin"));
  EXPECT_TRUE(MatchesPattern("path.to/ruin", "http://path.to/ruins"));
  EXPECT_TRUE(MatchesPattern("path.to/ruin", "http://path.to/ruin/signup"));
  EXPECT_TRUE(MatchesPattern("path.to/ruin", "http://www.path.to/ruin"));
  EXPECT_FALSE(MatchesPattern("path.to/ruin", "http://path.to/fortune"));

  // Filter only a certain path prefix and scheme.
  EXPECT_TRUE(
      MatchesPattern("https://s.aaa.com/path", "https://s.aaa.com/path"));
  EXPECT_TRUE(
      MatchesPattern("https://s.aaa.com/path", "https://s.aaa.com/path/bbb"));
  EXPECT_FALSE(
      MatchesPattern("https://s.aaa.com/path", "http://s.aaa.com/path"));
  EXPECT_FALSE(
      MatchesPattern("https://s.aaa.com/path", "https://aaa.com/path"));
  EXPECT_FALSE(
      MatchesPattern("https://s.aaa.com/path", "https://x.aaa.com/path"));
  EXPECT_FALSE(
      MatchesPattern("https://s.aaa.com/path", "https://s.aaa.com/bbb"));
  EXPECT_FALSE(MatchesPattern("https://s.aaa.com/path", "https://s.aaa.com/"));

  // Filter only ws and wss schemes.
  EXPECT_TRUE(MatchesPattern("ws://ws.aaa.com", "ws://ws.aaa.com"));
  EXPECT_TRUE(MatchesPattern("wss://ws.aaa.com", "wss://ws.aaa.com"));
  EXPECT_FALSE(MatchesPattern("ws://ws.aaa.com", "http://ws.aaa.com"));
  EXPECT_FALSE(MatchesPattern("ws://ws.aaa.com", "https://ws.aaa.com"));
  EXPECT_FALSE(MatchesPattern("ws://ws.aaa.com", "ftp://ws.aaa.com"));

  // Block an ip address.
  EXPECT_TRUE(MatchesPattern("123.123.123.123", "http://123.123.123.123/"));
  EXPECT_FALSE(MatchesPattern("123.123.123.123", "http://123.123.123.124/"));

  // Test exceptions to path prefixes, and most specific matches.
  base::Value::List blocked;
  base::Value::List allowed;
  blocked.Append("s.xxx.com/a");
  allowed.Append("s.xxx.com/a/b");
  blocked.Append("https://s.xxx.com/a/b/c");
  allowed.Append("https://s.xxx.com/a/b/c/d");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/x")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/d")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c/d")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/d/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c/d/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://xxx.com/a")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://xxx.com/a/b")));

  // Open an exception.
  blocked.clear();
  blocked.Append("google.com");
  allowed.clear();
  allowed.Append("plus.google.com");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://plus.google.com/")));

  // Open an exception only when using https for mail.
  allowed.clear();
  allowed.Append("https://mail.google.com");
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://mail.google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://www.google.com/")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://mail.google.com/")));

  // Match exactly "google.com", only for http. Subdomains without exceptions
  // are still blocked.
  allowed.clear();
  allowed.Append("http://.google.com");
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com/")));

  // A smaller path match in an exact host overrides a longer path for hosts
  // that also match subdomains.
  blocked.clear();
  blocked.Append("yyy.com/aaa");
  blocklist.Block(blocked);
  allowed.clear();
  allowed.Append(".yyy.com/a");
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://yyy.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://yyy.com/aaa")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://yyy.com/aaa2")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://www.yyy.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.yyy.com/aaa")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.yyy.com/aaa2")));

  // If the exact entry is both allowed and blocked, allowing takes precedence.
  blocked.clear();
  blocked.Append("example.com");
  blocklist.Block(blocked);
  allowed.clear();
  allowed.Append("example.com");
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://example.com")));

  // Devtools should not be blocked.
  blocked.clear();
  blocked.Append("*");
  blocklist.Block(blocked);
  allowed.clear();
  allowed.Append("devtools://*");
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("devtools://something.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://something.com")));
}

TEST_F(URLBlocklistManagerTest, QueryParameters) {
  URLBlocklist blocklist;
  base::Value::List blocked;
  base::Value::List allowed;

  // Block domain and all subdomains, for any filtered scheme.
  blocked.Append("youtube.com");
  allowed.Append("youtube.com/watch?v=XYZ");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);

  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=123")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=123&v=XYZ")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ&v=123")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ&foo=bar")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?foo=bar&v=XYZ")));

  allowed.clear();
  allowed.Append("youtube.com/watch?av=XYZ&ag=123");
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?av=123")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?av=XYZ")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?av=123&ag=XYZ")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?ag=XYZ&av=123")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?av=XYZ&ag=123")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?ag=123&av=XYZ")));
  EXPECT_TRUE(blocklist.IsURLBlocked(
      GURL("http://youtube.com/watch?av=XYZ&ag=123&av=123")));
  EXPECT_TRUE(blocklist.IsURLBlocked(
      GURL("http://youtube.com/watch?av=XYZ&ag=123&ag=1234")));

  allowed.clear();
  allowed.Append("youtube.com/watch?foo=bar*&vid=2*");
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?foo=bar")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2&foo=bar")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2&foo=bar1")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=234&foo=bar")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube.com/watch?vid=234&foo=bar23")));

  blocked.clear();
  blocked.Append("youtube1.com/disallow?v=44678");
  blocklist.Block(blocked);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com?v=123")));
  // Path does not match
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com?v=44678")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube1.com/disallow?v=44678")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube1.com/disallow?v=4467")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube1.com/disallow?v=4467&v=123")));
  EXPECT_TRUE(blocklist.IsURLBlocked(
      GURL("http://youtube1.com/disallow?v=4467&v=123&v=44678")));

  blocked.clear();
  blocked.Append("youtube1.com/disallow?g=*");
  blocklist.Block(blocked);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com?ag=123")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube1.com/disallow?g=123")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube1.com/disallow?ag=13&g=123")));

  blocked.clear();
  blocked.Append("youtube2.com/disallow?a*");
  blocklist.Block(blocked);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube2.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(
      GURL("http://youtube2.com/disallow?b=123&a21=467")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube2.com/disallow?abba=true")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube2.com/disallow?baba=true")));

  allowed.clear();
  blocked.clear();
  blocked.Append("youtube3.com");
  allowed.Append("youtube3.com/watch?fo*");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube3.com")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube3.com/watch?b=123&a21=467")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&foo1")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&foo=bar")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&fo=ba")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube3.com/watch?foreign=true")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube3.com/watch?fold")));

  allowed.clear();
  blocked.clear();
  blocked.Append("youtube4.com");
  allowed.Append("youtube4.com?*");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube4.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube4.com/?hello")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube4.com/?foo")));

  allowed.clear();
  blocked.clear();
  blocked.Append("youtube5.com?foo=bar");
  allowed.Append("youtube5.com?foo1=bar1&foo2=bar2&");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube5.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube5.com/?foo=bar&a=b")));
  // More specific filter is given precedence.
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube5.com/?a=b&foo=bar&foo1=bar1&foo2=bar2")));
}

TEST_F(URLBlocklistManagerTest, BlockAllWithExceptions) {
  URLBlocklist blocklist;

  base::Value::List blocked;
  base::Value::List allowed;
  blocked.Append("*");
  allowed.Append(".www.google.com");
  allowed.Append("plus.google.com");
  allowed.Append("https://mail.google.com");
  allowed.Append("https://very.safe/path");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://random.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://google.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://s.www.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://plus.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.plus.google.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://mail.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://mail.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://s.mail.google.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://very.safe/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://very.safe/path")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://very.safe/path")));
}

TEST_P(URLBlocklistParamTest, DefaultBlocklistExceptions) {
  URLBlocklist blocklist;
  base::Value::List blocked;

  // Blocklist everything:
  blocked.Append("*");
  blocklist.Block(blocked);

  // Internal NTP and extension URLs are not blocked by the "*":
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome-extension://xyz")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("chrome-search://most-visited/title.html")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome-native://ntp")));
#if BUILDFLAG(IS_IOS)
  // Ensure that the NTP is not blocked on iOS by "*".
  // TODO(crbug.com/40686232): On iOS, the NTP can not be blocked even by
  // explicitly listing it as a blocked URL. This is due to the usage of
  // "about:newtab" as its URL which is not recognized and filtered by the
  // URLBlocklist code.
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("about:newtab")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome://newtab")));
  if (use_standard_compliant_non_special_scheme_url_parsing()) {
    // When the feature is enabled, the host part in non-special URLs can be
    // recognized.
    EXPECT_TRUE(blocklist.IsURLBlocked(GURL("about://newtab/")));
  } else {
    EXPECT_FALSE(blocklist.IsURLBlocked(GURL("about://newtab/")));
  }
#endif

  // Unless they are explicitly on the blocklist:
  blocked.Append("chrome-extension://*");
  base::Value::List allowed;
  allowed.Append("chrome-extension://abc");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);

  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("chrome-extension://xyz")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome-extension://abc")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("chrome-search://most-visited/title.html")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome-native://ntp")));
}

TEST_P(URLBlocklistParamTest, BlocklistBasicCoverage) {
  // Tests to cover the documentation from
  // http://www.chromium.org/administrators/url-blocklist-filter-format

  // [scheme://][.]host[:port][/path][@query]
  // Scheme can be http, https, ftp, chrome, etc. This field is optional, and
  // must be followed by '://'.
  EXPECT_TRUE(MatchesPattern("file://*", "file:///abc.txt"));
  EXPECT_TRUE(MatchesPattern("file:*", "file:///usr/local/boot.txt"));
  EXPECT_TRUE(MatchesPattern("https://*", "https:///abc.txt"));
  EXPECT_TRUE(MatchesPattern("ftp://*", "ftp://ftp.txt"));
  EXPECT_TRUE(MatchesPattern("chrome://*", "chrome:policy"));
  EXPECT_TRUE(MatchesPattern("noscheme", "http://noscheme"));
  // Filter custom schemes.
  EXPECT_TRUE(MatchesPattern("custom://*", "custom://example_app"));
  EXPECT_TRUE(MatchesPattern("custom:*", "custom:example2_app"));
  EXPECT_FALSE(MatchesPattern("custom://*", "customs://example_apps"));
  EXPECT_FALSE(MatchesPattern("custom://*", "cust*://example_ap"));
  EXPECT_FALSE(MatchesPattern("custom://*", "ecustom:example_app"));
  EXPECT_TRUE(MatchesPattern("custom://*", "custom:///abc.txt"));
  // Tests for custom scheme patterns that are not supported.
  EXPECT_FALSE(MatchesPattern("wrong://app", "wrong://app"));
  EXPECT_FALSE(MatchesPattern("wrong ://*", "wrong ://app"));
  EXPECT_FALSE(MatchesPattern(" wrong:*", " wrong://app"));

  // Omitting the scheme matches most standard schemes.
  EXPECT_TRUE(MatchesPattern("example.com", "chrome:example.com"));
  EXPECT_TRUE(MatchesPattern("example.com", "chrome://example.com"));
  EXPECT_TRUE(MatchesPattern("example.com", "file://example.com/"));
  EXPECT_TRUE(MatchesPattern("example.com", "ftp://example.com"));
  EXPECT_TRUE(MatchesPattern("example.com", "http://example.com"));
  EXPECT_TRUE(MatchesPattern("example.com", "https://example.com"));
  EXPECT_TRUE(MatchesPattern("example.com", "ws://example.com"));
  EXPECT_TRUE(MatchesPattern("example.com", "wss://example.com"));

  // Some schemes are not matched when the scheme is omitted.
  EXPECT_FALSE(MatchesPattern("example.com", "about:example.com"));
  EXPECT_FALSE(MatchesPattern("example.com/*", "filesystem:///something"));
  if (use_standard_compliant_non_special_scheme_url_parsing()) {
    // When the feature is enabled, the host part in non-special URLs can be
    // recognized.
    EXPECT_TRUE(MatchesPattern("example.com", "about://example.com"));
    EXPECT_TRUE(MatchesPattern("example.com", "custom://example.com"));
    EXPECT_TRUE(MatchesPattern("example", "custom://example"));
    EXPECT_TRUE(MatchesPattern("example.com", "gopher://example.com"));
  } else {
    EXPECT_FALSE(MatchesPattern("example.com", "about://example.com"));
    EXPECT_FALSE(MatchesPattern("example.com", "about:example.com"));
    EXPECT_FALSE(MatchesPattern("example", "custom://example"));
    EXPECT_FALSE(MatchesPattern("example.com", "gopher://example.com"));
  }

  // An optional '.' (dot) can prefix the host field to disable subdomain
  // matching, see below for details.
  EXPECT_TRUE(MatchesPattern(".example.com", "http://example.com/path"));
  EXPECT_FALSE(MatchesPattern(".example.com", "http://mail.example.com/path"));
  EXPECT_TRUE(MatchesPattern("example.com", "http://mail.example.com/path"));
  EXPECT_TRUE(MatchesPattern("ftp://.ftp.file", "ftp://ftp.file"));
  EXPECT_FALSE(MatchesPattern("ftp://.ftp.file", "ftp://sub.ftp.file"));

  // The host field is required, and is a valid hostname or an IP address. It
  // can also take the special '*' value, see below for details.
  EXPECT_TRUE(MatchesPattern("*", "http://anything"));
  EXPECT_TRUE(MatchesPattern("*", "ftp://anything"));
  EXPECT_TRUE(MatchesPattern("*", "custom://anything"));
  EXPECT_TRUE(MatchesPattern("host", "http://host:8080"));
  EXPECT_FALSE(MatchesPattern("host", "file:///host"));
  EXPECT_TRUE(MatchesPattern("10.1.2.3", "http://10.1.2.3:8080/path"));
  // No host, will match nothing.
  EXPECT_FALSE(MatchesPattern(":8080", "http://host:8080"));
  EXPECT_FALSE(MatchesPattern(":8080", "http://:8080"));

  // An optional port can come after the host. It must be a valid port value
  // from 1 to 65535.
  EXPECT_TRUE(MatchesPattern("host:8080", "http://host:8080/path"));
  EXPECT_TRUE(MatchesPattern("host:1", "http://host:1/path"));
  // Out of range port.
  EXPECT_FALSE(MatchesPattern("host:65536", "http://host:65536/path"));
  // Star is not allowed in port numbers.
  EXPECT_FALSE(MatchesPattern("example.com:*", "http://example.com"));
  EXPECT_FALSE(MatchesPattern("example.com:*", "http://example.com:8888"));

  // An optional path can come after port.
  EXPECT_TRUE(MatchesPattern("host/path", "http://host:8080/path"));
  EXPECT_TRUE(MatchesPattern("host/path/path2", "http://host/path/path2"));
  EXPECT_TRUE(MatchesPattern("host/path", "http://host/path/path2"));

  // An optional query can come in the end, which is a set of key-value and
  // key-only tokens delimited by '&'. The key-value tokens are separated
  // by '='. A query token can optionally end with a '*' to indicate prefix
  // match. Token order is ignored during matching.
  EXPECT_TRUE(MatchesPattern("host?q1=1&q2=2", "http://host?q2=2&q1=1"));
  EXPECT_FALSE(MatchesPattern("host?q1=1&q2=2", "http://host?q2=1&q1=2"));
  EXPECT_FALSE(MatchesPattern("host?q1=1&q2=2", "http://host?Q2=2&Q1=1"));
  EXPECT_TRUE(MatchesPattern("host?q1=1&q2=2", "http://host?q2=2&q1=1&q3=3"));
  EXPECT_TRUE(MatchesPattern("host?q1=1&q2=2*", "http://host?q2=21&q1=1&q3=3"));

  // user:pass fields can be included but will be ignored
  // (e.g. http://user:pass@ftp.example.com/pub/bigfile.iso).
  EXPECT_TRUE(
      MatchesPattern("host.com/path", "http://user:pass@host.com:8080/path"));
  EXPECT_TRUE(MatchesPattern("ftp://host.com/path",
                             "ftp://user:pass@host.com:8080/path"));

  // Case sensitivity.
  // Scheme is case insensitive.
  EXPECT_TRUE(MatchesPattern("suPPort://*", "support:example"));
  EXPECT_TRUE(MatchesPattern("FILE://*", "file:example"));
  EXPECT_TRUE(MatchesPattern("FILE://*", "FILE://example"));
  EXPECT_TRUE(MatchesPattern("FtP:*", "ftp://example"));
  EXPECT_TRUE(MatchesPattern("http://example.com", "HTTP://example.com"));
  EXPECT_TRUE(MatchesPattern("HTTP://example.com", "http://example.com"));
  // Host is case insensitive.
  EXPECT_TRUE(MatchesPattern("http://EXAMPLE.COM", "http://example.com"));
  EXPECT_TRUE(MatchesPattern("Example.com", "http://examplE.com/Path?Query=1"));
  // Path is case sensitive.
  EXPECT_FALSE(MatchesPattern("example.com/Path", "http://example.com/path"));
  EXPECT_TRUE(MatchesPattern("http://example.com/aB", "http://example.com/aB"));
  EXPECT_FALSE(
      MatchesPattern("http://example.com/aB", "http://example.com/Ab"));
  EXPECT_FALSE(
      MatchesPattern("http://example.com/aB", "http://example.com/ab"));
  EXPECT_FALSE(
      MatchesPattern("http://example.com/aB", "http://example.com/AB"));
  // Query is case sensitive.
  EXPECT_FALSE(MatchesPattern("host/path?Query=1", "http://host/path?query=1"));
}

INSTANTIATE_TEST_SUITE_P(All, URLBlocklistParamTest, ::testing::Bool());

// Test for GetURLBlocklistState method.
TEST_F(URLBlocklistManagerTest, UseBlocklistState) {
  using State = URLBlocklist::URLBlocklistState;
  // Test allowlist states.
  EXPECT_EQ(State::URL_IN_ALLOWLIST, GetUrlBlocklistStateAfterAllowing(
                                         "example.com", "http://example.com"));
  EXPECT_EQ(State::URL_IN_ALLOWLIST, GetUrlBlocklistStateAfterAllowing(
                                         "http://*", "http://example.com"));
  EXPECT_EQ(State::URL_IN_ALLOWLIST,
            GetUrlBlocklistStateAfterAllowing("custom://*", "custom://app"));
  EXPECT_EQ(State::URL_IN_ALLOWLIST,
            GetUrlBlocklistStateAfterAllowing("custom:*", "custom://app/play"));
  EXPECT_EQ(State::URL_IN_ALLOWLIST,
            GetUrlBlocklistStateAfterAllowing("custom:*", "custom://app:8080"));
  // Test blocklist states.
  EXPECT_EQ(State::URL_IN_BLOCKLIST,
            GetUrlBlocklistStateAfterBlocking("ftp:*", "ftp://server"));
  // Test neutral states.
  EXPECT_EQ(State::URL_NEUTRAL_STATE,
            GetUrlBlocklistStateAfterAllowing("file:*", "http://example.com"));
  EXPECT_EQ(State::URL_NEUTRAL_STATE, GetUrlBlocklistStateAfterBlocking(
                                          "https://*", "http://example.com"));
}

#if BUILDFLAG(IS_CHROMEOS)
// Custom BlocklistSource implementation.
// Custom BlocklistSource implementation.
class CustomBlocklistSource : public BlocklistSource {
 public:
  CustomBlocklistSource() = default;
  CustomBlocklistSource(const CustomBlocklistSource&) = delete;
  CustomBlocklistSource& operator=(const CustomBlocklistSource&) = delete;
  ~CustomBlocklistSource() override = default;

  const base::Value::List* GetBlocklistSpec() const override {
    return &blocklist_;
  }

  const base::Value::List* GetAllowlistSpec() const override {
    return &allowlist_;
  }

  void SetBlocklistObserver(base::RepeatingClosure observer) override {
    blocklist_observer_ = std::move(observer);
  }

  void SetBlocklistSpec(base::Value::List blocklist) {
    blocklist_ = std::move(blocklist);
    TriggerObserver();
  }

  void SetAllowlistSpec(base::Value::List allowlist) {
    allowlist_ = std::move(allowlist);
    TriggerObserver();
  }

 private:
  void TriggerObserver() {
    if (!blocklist_observer_) {
      return;
    }
    blocklist_observer_.Run();
  }

  base::Value::List blocklist_;
  base::Value::List allowlist_;
  base::RepeatingClosure blocklist_observer_;
};

TEST_F(URLBlocklistManagerTest, SetAndUnsetOverrideBlockListSource) {
  SetUrlBlocklistPref(
      base::Value::List().Append("blocked-by-general-pref.com"));
  SetUrlAllowlistPref(
      base::Value::List().Append("allowed-by-general-pref.com"));
  task_environment()->RunUntilIdle();

  std::unique_ptr<CustomBlocklistSource> custom_blocklist =
      std::make_unique<CustomBlocklistSource>();
  custom_blocklist->SetAllowlistSpec(
      base::Value::List().Append("allowed-preconnect.com"));
  custom_blocklist->SetBlocklistSpec(
      base::Value::List().Append("blocked-preconnect.com"));

  blocklist_manager()->SetOverrideBlockListSource(std::move(custom_blocklist));
  task_environment()->RunUntilIdle();
  // Verify that custom BlocklistSource is active.
  EXPECT_EQ(URLBlocklist::URL_IN_ALLOWLIST,
            blocklist_manager()->GetURLBlocklistState(
                GURL("http://allowed-preconnect.com")));
  EXPECT_EQ(URLBlocklist::URL_IN_BLOCKLIST,
            blocklist_manager()->GetURLBlocklistState(
                GURL("http://blocked-preconnect.com")));
  // URLs not covered by the custom BlocklistSource should be in neutrat state.
  EXPECT_EQ(URLBlocklist::URL_NEUTRAL_STATE,
            blocklist_manager()->GetURLBlocklistState(
                GURL("http://allowed-by-general-pref.com")));
  EXPECT_EQ(URLBlocklist::URL_NEUTRAL_STATE,
            blocklist_manager()->GetURLBlocklistState(
                GURL("http://neutral-by-general-pref.com")));

  blocklist_manager()->SetOverrideBlockListSource(nullptr);
  task_environment()->RunUntilIdle();
  // Verify that default BlocklistSource is active.
  EXPECT_EQ(URLBlocklist::URL_IN_BLOCKLIST,
            blocklist_manager()->GetURLBlocklistState(
                GURL("http://blocked-by-general-pref.com")));
  EXPECT_EQ(URLBlocklist::URL_IN_ALLOWLIST,
            blocklist_manager()->GetURLBlocklistState(
                GURL("http://allowed-by-general-pref.com")));
  // URLs not covered by the default BlocklistSource should be in neutrat state.
  EXPECT_EQ(URLBlocklist::URL_NEUTRAL_STATE,
            blocklist_manager()->GetURLBlocklistState(
                GURL("http://allowed-preconnect.com")));
  EXPECT_EQ(URLBlocklist::URL_NEUTRAL_STATE,
            blocklist_manager()->GetURLBlocklistState(
                GURL("http://blocked-preconnect.com")));
}

TEST_F(URLBlocklistManagerTest, BlockListSourceUpdates) {
  std::unique_ptr<CustomBlocklistSource> custom_blocklist =
      std::make_unique<CustomBlocklistSource>();
  custom_blocklist->SetBlocklistSpec(
      base::Value::List().Append("preconnect.com"));

  raw_ptr<CustomBlocklistSource> custom_blocklist_ptr = custom_blocklist.get();
  blocklist_manager()->SetOverrideBlockListSource(std::move(custom_blocklist));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(
      URLBlocklist::URL_IN_BLOCKLIST,
      blocklist_manager()->GetURLBlocklistState(GURL("http://preconnect.com")));

  // Update the BlocklistSource.
  custom_blocklist_ptr->SetBlocklistSpec(base::Value::List());
  task_environment()->RunUntilIdle();

  custom_blocklist_ptr = nullptr;

  EXPECT_EQ(
      URLBlocklist::URL_NEUTRAL_STATE,
      blocklist_manager()->GetURLBlocklistState(GURL("http://preconnect.com")));
}
#endif
}  // namespace policy
