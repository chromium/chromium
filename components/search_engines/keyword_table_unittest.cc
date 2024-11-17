// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/keyword_table.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/search_engines/template_url_data.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::Time;

class KeywordTableTest : public testing::Test {
 public:
  KeywordTableTest() = default;

  KeywordTableTest(const KeywordTableTest&) = delete;
  KeywordTableTest& operator=(const KeywordTableTest&) = delete;

  ~KeywordTableTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<KeywordTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  void AddKeyword(const TemplateURLData& keyword) const {
    EXPECT_TRUE(table_->AddKeyword(keyword));
  }

  TemplateURLData CreateAndAddKeyword() const {
    TemplateURLData keyword;
    keyword.SetShortName(u"short_name");
    keyword.SetKeyword(u"keyword");
    keyword.SetURL("http://url/");
    keyword.suggestions_url = "url2";
    keyword.image_url = "http://image-search-url/";
    keyword.new_tab_url = "http://new-tab-url/";
    keyword.search_url_post_params = "ie=utf-8,oe=utf-8";
    keyword.image_url_post_params = "name=1,value=2";
    keyword.favicon_url = GURL("http://favicon.url/");
    keyword.originating_url = GURL("http://google.com/");
    keyword.safe_for_autoreplace = true;
    keyword.input_encodings.push_back("UTF-8");
    keyword.input_encodings.push_back("UTF-16");
    keyword.id = 1;
    keyword.date_created = base::Time::UnixEpoch();
    keyword.last_modified = base::Time::UnixEpoch();
    keyword.last_visited = base::Time::UnixEpoch();
    keyword.created_by_policy =
        TemplateURLData::CreatedByPolicy::kDefaultSearchProvider;
    keyword.usage_count = 32;
    keyword.prepopulate_id = 10;
    keyword.sync_guid = "1234-5678-90AB-CDEF";
    keyword.alternate_urls.push_back("a_url1");
    keyword.alternate_urls.push_back("a_url2");
    keyword.starter_pack_id = 1;
    keyword.enforced_by_policy = true;
    keyword.featured_by_policy = true;
    AddKeyword(keyword);
    return keyword;
  }

  void RemoveKeyword(TemplateURLID id) const {
    EXPECT_TRUE(table_->RemoveKeyword(id));
  }

  void UpdateKeyword(const TemplateURLData& keyword) const {
    EXPECT_TRUE(table_->UpdateKeyword(keyword));
  }

  KeywordTable::Keywords GetKeywords() const {
    KeywordTable::Keywords keywords;
    EXPECT_TRUE(table_->GetKeywords(&keywords));
    return keywords;
  }

  void KeywordMiscTest() const {
    EXPECT_EQ(kInvalidTemplateURLID, table_->GetDefaultSearchProviderID());
    EXPECT_EQ(0, table_->GetBuiltinKeywordDataVersion());
    EXPECT_EQ(0, table_->GetBuiltinKeywordCountry());

    EXPECT_TRUE(table_->SetDefaultSearchProviderID(10));
    EXPECT_TRUE(table_->SetBuiltinKeywordDataVersion(11));
    EXPECT_TRUE(table_->SetBuiltinKeywordCountry(12));

    EXPECT_EQ(10, table_->GetDefaultSearchProviderID());
    EXPECT_EQ(11, table_->GetBuiltinKeywordDataVersion());
    EXPECT_EQ(12, table_->GetBuiltinKeywordCountry());
  }

  void GetStatement(const base::cstring_view sql,
                    sql::Statement* statement) const {
    statement->Assign(table_->db()->GetUniqueStatement(sql));
  }

 private:
  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<KeywordTable> table_;
  std::unique_ptr<WebDatabase> db_;
};


TEST_F(KeywordTableTest, Keywords) {
  TemplateURLData keyword(CreateAndAddKeyword());

  KeywordTable::Keywords keywords(GetKeywords());
  EXPECT_EQ(1U, keywords.size());
  const TemplateURLData& restored_keyword = keywords.front();

  EXPECT_EQ(keyword.short_name(), restored_keyword.short_name());
  EXPECT_EQ(keyword.keyword(), restored_keyword.keyword());
  EXPECT_EQ(keyword.url(), restored_keyword.url());
  EXPECT_EQ(keyword.suggestions_url, restored_keyword.suggestions_url);
  EXPECT_EQ(keyword.favicon_url, restored_keyword.favicon_url);
  EXPECT_EQ(keyword.originating_url, restored_keyword.originating_url);
  EXPECT_EQ(keyword.safe_for_autoreplace,
            restored_keyword.safe_for_autoreplace);
  EXPECT_EQ(keyword.input_encodings, restored_keyword.input_encodings);
  EXPECT_EQ(keyword.id, restored_keyword.id);
  // The database stores time only at the resolution of a second.
  EXPECT_EQ(keyword.date_created.ToTimeT(),
            restored_keyword.date_created.ToTimeT());
  EXPECT_EQ(keyword.last_modified.ToTimeT(),
            restored_keyword.last_modified.ToTimeT());
  EXPECT_EQ(keyword.last_visited.ToTimeT(),
            restored_keyword.last_visited.ToTimeT());
  EXPECT_EQ(keyword.created_by_policy, restored_keyword.created_by_policy);
  EXPECT_EQ(keyword.created_from_play_api,
            restored_keyword.created_from_play_api);
  EXPECT_EQ(keyword.usage_count, restored_keyword.usage_count);
  EXPECT_EQ(keyword.prepopulate_id, restored_keyword.prepopulate_id);
  EXPECT_EQ(keyword.is_active, restored_keyword.is_active);
  EXPECT_EQ(keyword.starter_pack_id, restored_keyword.starter_pack_id);
  EXPECT_EQ(keyword.enforced_by_policy, restored_keyword.enforced_by_policy);
  EXPECT_EQ(keyword.featured_by_policy, restored_keyword.featured_by_policy);

  RemoveKeyword(restored_keyword.id);

  EXPECT_EQ(0U, GetKeywords().size());
}

TEST_F(KeywordTableTest, KeywordMisc) {
  KeywordMiscTest();
}

TEST_F(KeywordTableTest, UpdateKeyword) {
  TemplateURLData keyword(CreateAndAddKeyword());

  keyword.SetKeyword(u"url");
  keyword.originating_url = GURL("http://originating.url/");
  keyword.input_encodings.push_back("Shift_JIS");
  keyword.prepopulate_id = 5;
  keyword.created_from_play_api = true;
  keyword.starter_pack_id = 0;
  keyword.enforced_by_policy = false;
  keyword.featured_by_policy = false;
  UpdateKeyword(keyword);

  KeywordTable::Keywords keywords(GetKeywords());
  EXPECT_EQ(1U, keywords.size());
  const TemplateURLData& restored_keyword = keywords.front();

  EXPECT_EQ(keyword.short_name(), restored_keyword.short_name());
  EXPECT_EQ(keyword.keyword(), restored_keyword.keyword());
  EXPECT_EQ(keyword.suggestions_url, restored_keyword.suggestions_url);
  EXPECT_EQ(keyword.favicon_url, restored_keyword.favicon_url);
  EXPECT_EQ(keyword.originating_url, restored_keyword.originating_url);
  EXPECT_EQ(keyword.safe_for_autoreplace,
            restored_keyword.safe_for_autoreplace);
  EXPECT_EQ(keyword.input_encodings, restored_keyword.input_encodings);
  EXPECT_EQ(keyword.id, restored_keyword.id);
  EXPECT_EQ(keyword.prepopulate_id, restored_keyword.prepopulate_id);
  EXPECT_EQ(keyword.created_from_play_api,
            restored_keyword.created_from_play_api);
  EXPECT_EQ(keyword.is_active, restored_keyword.is_active);
  EXPECT_EQ(keyword.starter_pack_id, restored_keyword.starter_pack_id);
  EXPECT_EQ(keyword.enforced_by_policy, restored_keyword.enforced_by_policy);
  EXPECT_EQ(keyword.featured_by_policy, restored_keyword.featured_by_policy);
}

TEST_F(KeywordTableTest, KeywordWithNoFavicon) {
  TemplateURLData keyword;
  keyword.SetShortName(u"short_name");
  keyword.SetKeyword(u"keyword");
  keyword.SetURL("http://url/");
  keyword.safe_for_autoreplace = true;
  keyword.id = -100;
  AddKeyword(keyword);

  KeywordTable::Keywords keywords(GetKeywords());
  EXPECT_EQ(1U, keywords.size());
  const TemplateURLData& restored_keyword = keywords.front();

  EXPECT_EQ(keyword.short_name(), restored_keyword.short_name());
  EXPECT_EQ(keyword.keyword(), restored_keyword.keyword());
  EXPECT_EQ(keyword.favicon_url, restored_keyword.favicon_url);
  EXPECT_EQ(keyword.safe_for_autoreplace,
            restored_keyword.safe_for_autoreplace);
  EXPECT_EQ(keyword.id, restored_keyword.id);
}

TEST_F(KeywordTableTest, SanitizeURLs) {
  TemplateURLData keyword;
  keyword.SetShortName(u"legit");
  keyword.SetKeyword(u"legit");
  keyword.SetURL("http://url/");
  keyword.id = 1000;
  AddKeyword(keyword);

  keyword.SetShortName(u"bogus");
  keyword.SetKeyword(u"bogus");
  keyword.id = 2000;
  AddKeyword(keyword);

  EXPECT_EQ(2U, GetKeywords().size());

  // Erase the URL field for the second keyword to simulate having bogus data
  // previously saved into the database.
  sql::Statement s;
  GetStatement("UPDATE keywords SET url=? WHERE id=?", &s);
  s.BindString16(0, std::u16string());
  s.BindInt64(1, 2000);
  EXPECT_TRUE(s.Run());

  // GetKeywords() should erase the entry with the empty URL field.
  EXPECT_EQ(1U, GetKeywords().size());
}

TEST_F(KeywordTableTest, SanitizeShortName) {
  TemplateURLData keyword;
  {
    keyword.SetShortName(u"legit name");
    keyword.SetKeyword(u"legit");
    keyword.SetURL("http://url/");
    keyword.id = 1000;
    AddKeyword(keyword);
    KeywordTable::Keywords keywords(GetKeywords());
    EXPECT_EQ(1U, keywords.size());
    const TemplateURLData& keyword_from_database = keywords.front();
    EXPECT_EQ(keyword.id, keyword_from_database.id);
    EXPECT_EQ(u"legit name", keyword_from_database.short_name());
    RemoveKeyword(keyword.id);
  }

  {
    keyword.SetShortName(u"\t\tbogus \tname \n");
    keyword.SetKeyword(u"bogus");
    keyword.id = 2000;
    AddKeyword(keyword);
    KeywordTable::Keywords keywords(GetKeywords());
    EXPECT_EQ(1U, keywords.size());
    const TemplateURLData& keyword_from_database = keywords.front();
    EXPECT_EQ(keyword.id, keyword_from_database.id);
    EXPECT_EQ(u"bogus name", keyword_from_database.short_name());
    RemoveKeyword(keyword.id);
  }
}
