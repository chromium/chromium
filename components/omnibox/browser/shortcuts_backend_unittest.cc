// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_backend.h"

#include <stddef.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/shortcuts_constants.h"
#include "components/omnibox/browser/shortcuts_database.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// ShortcutsBackendTest -------------------------------------------------------

class ShortcutsBackendTest : public testing::Test,
                             public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  ShortcutsBackendTest();
  ShortcutsBackendTest(const ShortcutsBackendTest&) = delete;
  ShortcutsBackendTest& operator=(const ShortcutsBackendTest&) = delete;

  ShortcutsDatabase::Shortcut::MatchCore MatchCoreForTesting(
      const std::string& url,
      const std::string& contents_class = std::string(),
      const std::string& description_class = std::string(),
      AutocompleteMatch::Type type = AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  void SetSearchProvider();

  void SetUp() override;
  void TearDown() override;

  void OnShortcutsLoaded() override;
  void OnShortcutsChanged() override;

  const ShortcutsBackend::ShortcutMap& shortcuts_map() const {
    return backend_->shortcuts_map();
  }
  bool changed_notified() const { return changed_notified_; }
  void set_changed_notified(bool changed_notified) {
    changed_notified_ = changed_notified;
  }

  void InitBackend();
  bool AddShortcut(const ShortcutsDatabase::Shortcut& shortcut);
  bool UpdateShortcut(const ShortcutsDatabase::Shortcut& shortcut);
  bool DeleteShortcutsWithURL(const GURL& url);
  bool DeleteShortcutsWithIDs(
      const ShortcutsDatabase::ShortcutIDs& deleted_ids);
  bool ShortcutExists(const std::u16string& terms);

  TemplateURLService* GetTemplateURLService();

  ShortcutsBackend* backend() { return backend_.get(); }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

 private:
  base::ScopedTempDir profile_dir_;
  // `scoped_feature_list_` needs to be destroyed after TaskEnvironment is
  // destroyed, so that other threads won't access the feature list while it is
  // being destroyed.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<history::HistoryService> history_service_;

  scoped_refptr<ShortcutsBackend> backend_;

  bool load_notified_ = false;
  bool changed_notified_ = false;
};

ShortcutsBackendTest::ShortcutsBackendTest() = default;

ShortcutsDatabase::Shortcut::MatchCore
ShortcutsBackendTest::MatchCoreForTesting(const std::string& url,
                                          const std::string& contents_class,
                                          const std::string& description_class,
                                          AutocompleteMatch::Type type) {
  AutocompleteMatch match(nullptr, 0, false, type);
  match.destination_url = GURL(url);
  match.contents = u"test";
  match.contents_class =
      AutocompleteMatch::ClassificationsFromString(contents_class);
  match.description_class =
      AutocompleteMatch::ClassificationsFromString(description_class);
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(match.contents);
  SearchTermsData search_terms_data;
  return ShortcutsBackend::MatchToMatchCore(match, template_url_service_.get(),
                                            &search_terms_data);
}

void ShortcutsBackendTest::SetSearchProvider() {
  TemplateURLData data;
  data.SetURL("http://foo.com/search?bar={searchTerms}");
  data.SetShortName(u"foo");
  data.SetKeyword(u"foo");

  TemplateURL* template_url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
}

void ShortcutsBackendTest::SetUp() {
  ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
  template_url_service_ = std::make_unique<TemplateURLService>(nullptr, 0);
  history_service_ =
      history::CreateHistoryService(profile_dir_.GetPath(), true);
  ASSERT_TRUE(history_service_);

  base::FilePath shortcuts_database_path =
      profile_dir_.GetPath().Append(kShortcutsDatabaseName);
  backend_ = new ShortcutsBackend(
      template_url_service_.get(), std::make_unique<SearchTermsData>(),
      history_service_.get(), shortcuts_database_path, false);
  ASSERT_TRUE(backend_.get());
  backend_->AddObserver(this);
}

void ShortcutsBackendTest::TearDown() {
  backend_->RemoveObserver(this);
  backend_->ShutdownOnUIThread();
  backend_.reset();

  // Explicitly shut down the history service and wait for its backend to be
  // destroyed to prevent resource leaks.
  base::RunLoop run_loop;
  history_service_->SetOnBackendDestroyTask(run_loop.QuitClosure());
  history_service_->Shutdown();
  run_loop.Run();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(profile_dir_.Delete());
}

void ShortcutsBackendTest::OnShortcutsLoaded() {
  load_notified_ = true;
}

void ShortcutsBackendTest::OnShortcutsChanged() {
  changed_notified_ = true;
}

void ShortcutsBackendTest::InitBackend() {
  ASSERT_TRUE(backend_);
  ASSERT_FALSE(load_notified_);
  ASSERT_FALSE(backend_->initialized());
  backend_->Init();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(load_notified_);
  EXPECT_TRUE(backend_->initialized());
}

bool ShortcutsBackendTest::AddShortcut(
    const ShortcutsDatabase::Shortcut& shortcut) {
  return backend_->AddShortcut(shortcut);
}

bool ShortcutsBackendTest::UpdateShortcut(
    const ShortcutsDatabase::Shortcut& shortcut) {
  return backend_->UpdateShortcut(shortcut);
}

bool ShortcutsBackendTest::DeleteShortcutsWithURL(const GURL& url) {
  return backend_->DeleteShortcutsWithURL(url);
}

bool ShortcutsBackendTest::DeleteShortcutsWithIDs(
    const ShortcutsDatabase::ShortcutIDs& deleted_ids) {
  return backend_->DeleteShortcutsWithIDs(deleted_ids);
}

bool ShortcutsBackendTest::ShortcutExists(const std::u16string& terms) {
  return shortcuts_map().find(terms) != shortcuts_map().end();
}

TemplateURLService* ShortcutsBackendTest::GetTemplateURLService() {
  return template_url_service_.get();
}

// Actual tests ---------------------------------------------------------------

// Verifies that creating MatchCores strips classifications and sanitizes match
// types.
TEST_F(ShortcutsBackendTest, SanitizeMatchCore) {
  struct {
    std::string input_contents_class;
    std::string input_description_class;
    AutocompleteMatch::Type input_type;
    std::string output_contents_class;
    std::string output_description_class;
    AutocompleteMatch::Type output_type;
  } cases[] = {
    { "0,1,4,0", "0,3,4,1",  AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      "0,1,4,0", "0,1",      AutocompleteMatchType::HISTORY_URL },
    { "0,3,5,1", "0,2,5,0",  AutocompleteMatchType::NAVSUGGEST,
      "0,1",     "0,0",      AutocompleteMatchType::HISTORY_URL },
    { "0,1",     "0,0,11,2,15,0",
                             AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      "0,1",     "0,0",      AutocompleteMatchType::SEARCH_HISTORY },
    { "0,1",     "0,0",      AutocompleteMatchType::SEARCH_SUGGEST,
      "0,1",     "0,0",      AutocompleteMatchType::SEARCH_HISTORY },
    { "0,1",     "0,0",      AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
      "",        "",         AutocompleteMatchType::SEARCH_HISTORY },
    { "0,1",     "0,0",      AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
      "",        "",         AutocompleteMatchType::SEARCH_HISTORY },
    { "0,1",     "0,0",      AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED,
      "",        "",         AutocompleteMatchType::SEARCH_HISTORY },
    { "0,1",     "0,0",      AutocompleteMatchType::SEARCH_SUGGEST_PROFILE,
      "",        "",         AutocompleteMatchType::SEARCH_HISTORY },
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    ShortcutsDatabase::Shortcut::MatchCore match_core(MatchCoreForTesting(
        std::string(), cases[i].input_contents_class,
        cases[i].input_description_class, cases[i].input_type));
    EXPECT_EQ(cases[i].output_contents_class, match_core.contents_class)
        << ":i:" << i << ":type:" << cases[i].input_type;
    EXPECT_EQ(cases[i].output_description_class, match_core.description_class)
        << ":i:" << i << ":type:" << cases[i].input_type;
    EXPECT_EQ(cases[i].output_type, match_core.type)
        << ":i:" << i << ":type:" << cases[i].input_type;
  }
}

TEST_F(ShortcutsBackendTest, EntitySuggestionTest) {
  SetSearchProvider();
  AutocompleteMatch match;
  match.fill_into_edit = u"franklin d roosevelt";
  match.type = AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
  match.contents = u"roosevelt";
  match.contents_class =
      AutocompleteMatch::ClassificationsFromString("0,0,5,2");
  match.description = u"Franklin D. Roosevelt";
  match.description_class = AutocompleteMatch::ClassificationsFromString("0,4");
  match.destination_url =
      GURL("http://www.foo.com/search?bar=franklin+d+roosevelt&gs_ssp=1234");
  match.keyword = u"foo";
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(match.fill_into_edit);

  SearchTermsData search_terms_data;
  ShortcutsDatabase::Shortcut::MatchCore match_core =
      ShortcutsBackend::MatchToMatchCore(match, GetTemplateURLService(),
                                         &search_terms_data);
  EXPECT_EQ("http://foo.com/search?bar=franklin+d+roosevelt",
            match_core.destination_url.spec());
  EXPECT_EQ(match.fill_into_edit, match_core.contents);
  EXPECT_EQ("0,0", match_core.contents_class);
  EXPECT_EQ(std::u16string(), match_core.description);
  EXPECT_TRUE(match_core.description_class.empty());
}

TEST_F(ShortcutsBackendTest, MatchCoreDescriptionTest) {
  // When match.description_for_shortcuts is empty, match_core should use
  // match.description.
  {
    AutocompleteMatch match;
    match.description = u"the cat";
    match.description_class =
        AutocompleteMatch::ClassificationsFromString("0,1");

    SearchTermsData search_terms_data;
    ShortcutsDatabase::Shortcut::MatchCore match_core =
        ShortcutsBackend::MatchToMatchCore(match, GetTemplateURLService(),
                                           &search_terms_data);
    EXPECT_EQ(match_core.description, match.description);
    EXPECT_EQ(
        match_core.description_class,
        AutocompleteMatch::ClassificationsToString(match.description_class));
  }

  // When match.description_for_shortcuts is set, match_core should use it
  // instead of match.description.
  {
    AutocompleteMatch match;
    match.description = u"the cat";
    match.description_class =
        AutocompleteMatch::ClassificationsFromString("0,1");
    match.description_for_shortcuts = u"the elephant";
    match.description_class_for_shortcuts =
        AutocompleteMatch::ClassificationsFromString("0,4");

    SearchTermsData search_terms_data;
    ShortcutsDatabase::Shortcut::MatchCore match_core =
        ShortcutsBackend::MatchToMatchCore(match, GetTemplateURLService(),
                                           &search_terms_data);
    EXPECT_EQ(match_core.description, match.description_for_shortcuts);
    EXPECT_EQ(match_core.description_class,
              AutocompleteMatch::ClassificationsToString(
                  match.description_class_for_shortcuts));
  }
}

TEST_F(ShortcutsBackendTest, AddAndUpdateShortcut) {
  InitBackend();
  EXPECT_FALSE(changed_notified());

  ShortcutsDatabase::Shortcut shortcut(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", u"goog",
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut));
  EXPECT_TRUE(changed_notified());
  auto shortcut_iter(shortcuts_map().find(shortcut.text));
  ASSERT_TRUE(shortcut_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut.id, shortcut_iter->second.id);
  EXPECT_EQ(shortcut.match_core.contents,
            shortcut_iter->second.match_core.contents);

  set_changed_notified(false);
  shortcut.match_core.contents = u"Google Web Search";
  EXPECT_TRUE(UpdateShortcut(shortcut));
  EXPECT_TRUE(changed_notified());
  shortcut_iter = shortcuts_map().find(shortcut.text);
  ASSERT_TRUE(shortcut_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut.id, shortcut_iter->second.id);
  EXPECT_EQ(shortcut.match_core.contents,
            shortcut_iter->second.match_core.contents);
}

TEST_F(ShortcutsBackendTest, DeleteShortcuts) {
  InitBackend();
  ShortcutsDatabase::Shortcut shortcut1(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", u"goog",
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut1));

  ShortcutsDatabase::Shortcut shortcut2(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E0", u"gle",
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut2));

  ShortcutsDatabase::Shortcut shortcut3(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E1", u"sp",
      MatchCoreForTesting("http://www.sport.com"), base::Time::Now(), 10);
  EXPECT_TRUE(AddShortcut(shortcut3));

  ShortcutsDatabase::Shortcut shortcut4(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E2", u"mov",
      MatchCoreForTesting("http://www.film.com"), base::Time::Now(), 10);
  EXPECT_TRUE(AddShortcut(shortcut4));

  ASSERT_EQ(4U, shortcuts_map().size());
  EXPECT_EQ(shortcut1.id, shortcuts_map().find(shortcut1.text)->second.id);
  EXPECT_EQ(shortcut2.id, shortcuts_map().find(shortcut2.text)->second.id);
  EXPECT_EQ(shortcut3.id, shortcuts_map().find(shortcut3.text)->second.id);
  EXPECT_EQ(shortcut4.id, shortcuts_map().find(shortcut4.text)->second.id);

  EXPECT_TRUE(DeleteShortcutsWithURL(shortcut1.match_core.destination_url));

  ASSERT_EQ(2U, shortcuts_map().size());
  EXPECT_EQ(0U, shortcuts_map().count(shortcut1.text));
  EXPECT_EQ(0U, shortcuts_map().count(shortcut2.text));
  const ShortcutsBackend::ShortcutMap::const_iterator shortcut3_iter(
      shortcuts_map().find(shortcut3.text));
  ASSERT_TRUE(shortcut3_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut3.id, shortcut3_iter->second.id);
  const ShortcutsBackend::ShortcutMap::const_iterator shortcut4_iter(
      shortcuts_map().find(shortcut4.text));
  ASSERT_TRUE(shortcut4_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut4.id, shortcut4_iter->second.id);

  ShortcutsDatabase::ShortcutIDs deleted_ids;
  deleted_ids.push_back(shortcut3.id);
  deleted_ids.push_back(shortcut4.id);
  EXPECT_TRUE(DeleteShortcutsWithIDs(deleted_ids));

  ASSERT_EQ(0U, shortcuts_map().size());
}

TEST_F(ShortcutsBackendTest, AddOrUpdateShortcut) {
  InitBackend();

  AutocompleteMatch match;
  match.destination_url = GURL("https://www.google.com");

  // Should not have a shortcut initially.
  EXPECT_EQ(shortcuts_map().size(), 0u);
  EXPECT_FALSE(ShortcutExists(u"we need very long text for this test"));

  // Should have shortcut after shortcut is added to a match.
  backend()->AddOrUpdateShortcut(u"we need very long text for this test",
                                 match);
  EXPECT_EQ(shortcuts_map().size(), 1u);
  EXPECT_TRUE(ShortcutExists(u"we need very long text for this test"));

  // Should not shorten shortcut when a slightly shorter input (less than 3
  // chars shorter) is used for the match.
  backend()->AddOrUpdateShortcut(u"we need very long text for this t", match);
  EXPECT_EQ(shortcuts_map().size(), 1u);
  EXPECT_FALSE(ShortcutExists(u"we need very long text for this t"));
  EXPECT_TRUE(ShortcutExists(u"we need very long text for this test"));

  // Should shorten shortcut when a shorter input is used for the match.
  backend()->AddOrUpdateShortcut(u"we need very long", match);
  EXPECT_EQ(shortcuts_map().size(), 1u);
  EXPECT_TRUE(ShortcutExists(u"we need very long te"));
  EXPECT_FALSE(ShortcutExists(u"we need very long text for this test"));

  // Should add new shortcut when a longer input is used for the match.
  backend()->AddOrUpdateShortcut(u"we need very long text for this test",
                                 match);
  EXPECT_EQ(shortcuts_map().size(), 2u);
  EXPECT_TRUE(ShortcutExists(u"we need very long te"));
  EXPECT_TRUE(ShortcutExists(u"we need very long text for this test"));

  // Should shorten shortcut when a shorter input is used for the match. The
  // shorter shortcut to the same match should remain.
  backend()->AddOrUpdateShortcut(u"we need very long text", match);
  EXPECT_EQ(shortcuts_map().size(), 2u);
  EXPECT_TRUE(ShortcutExists(u"we need very long te"));
  EXPECT_TRUE(ShortcutExists(u"we need very long text fo"));
  EXPECT_FALSE(ShortcutExists(u"we need very long text for this test"));

  // Should only touch the shortest shortcut. The longer shortcut to the same
  // match should remain.
  backend()->AddOrUpdateShortcut(u"we", match);
  EXPECT_EQ(shortcuts_map().size(), 2u);
  EXPECT_TRUE(ShortcutExists(u"we ne"));
  EXPECT_FALSE(ShortcutExists(u"we need very long te"));
  EXPECT_TRUE(ShortcutExists(u"we need very long text fo"));
}
