// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/keyword_table.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/test_encryptor.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::Time;

class KeywordTableTest : public testing::Test {
 public:
  KeywordTableTest()
      : encryptor_(os_crypt_async::GetTestEncryptorForTesting()) {}

  KeywordTableTest(const KeywordTableTest&) = delete;
  KeywordTableTest& operator=(const KeywordTableTest&) = delete;

  ~KeywordTableTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");
    InitDatabase();
  }

  // Pass in an `encryptor` if wanting to override the default one.
  void InitDatabase(
      scoped_refptr<const os_crypt_async::Encryptor> encryptor = nullptr) {
    table_ = std::make_unique<KeywordTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK,
              db_->Init(file_, encryptor ? encryptor : encryptor_));
  }

  void CloseDatabase() {
    db_.reset();
    table_.reset();
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
    keyword.id = TemplateURLID(1);
    keyword.date_created = base::Time::UnixEpoch();
    keyword.last_modified = base::Time::UnixEpoch();
    keyword.last_visited = base::Time::UnixEpoch();
    keyword.policy_origin =
        TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
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

  KeywordTable::Keywords GetKeywords(
      bool expected_prepopulated_decryption_failure = false,
      bool expected_starter_pack_decryption_failure = false) const {
    KeywordTable::Keywords keywords;
    bool had_prepopulated_decryption_failure =
        !expected_prepopulated_decryption_failure;
    bool had_starter_pack_decryption_failure =
        !expected_starter_pack_decryption_failure;
    EXPECT_TRUE(table_->GetKeywords(&keywords,
                                    had_prepopulated_decryption_failure,
                                    had_starter_pack_decryption_failure));
    EXPECT_EQ(expected_prepopulated_decryption_failure,
              had_prepopulated_decryption_failure);
    EXPECT_EQ(expected_starter_pack_decryption_failure,
              had_starter_pack_decryption_failure);
    return keywords;
  }

  void ExpectStoredKeywordCount(int count) const {
    sql::Statement s;
    GetStatement("SELECT COUNT(*) FROM keywords", &s);
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(count, s.ColumnInt(0));
  }

  void GetStatement(const base::cstring_view sql,
                    sql::Statement* statement) const {
    statement->Assign(table_->db()->GetUniqueStatement(sql));
  }

  void ManuallySetMigrationEnabledInDb(int value) const {
    sql::Statement s;
    GetStatement(
        "INSERT OR REPLACE INTO meta (key, value) VALUES ('Is Prepopulated "
        "Engines Migration Enabled', ?)",
        &s);
    s.BindInt64(0, value);
    EXPECT_TRUE(s.Run());
  }

  KeywordTable* GetTable() { return table_.get(); }

  base::FilePath file_;

 protected:
  scoped_refptr<os_crypt_async::TestEncryptor> encryptor_;

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<KeywordTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_F(KeywordTableTest, Keywords) {
  TemplateURLData keyword(CreateAndAddKeyword());

  base::HistogramTester histograms;

  KeywordTable::Keywords keywords(GetKeywords());
  constexpr base::HistogramBase::Sample32 expected_bucket =
#if BUILDFLAG(IS_WIN)
      0;  // HashValidationStatus::kSuccess;
#else
      5;  // HashValidationStatus::kNotVerifiedFeatureDisabled;
#endif  // BUILDFLAG(IS_WIN)
  histograms.ExpectUniqueSample("Search.KeywordTable.HashValidationStatus",
                                expected_bucket, 1);

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
  EXPECT_EQ(keyword.policy_origin, restored_keyword.policy_origin);
  EXPECT_EQ(keyword.regulatory_origin, restored_keyword.regulatory_origin);
  EXPECT_EQ(keyword.usage_count, restored_keyword.usage_count);
  EXPECT_EQ(keyword.prepopulate_id, restored_keyword.prepopulate_id);
  EXPECT_EQ(keyword.is_active, restored_keyword.is_active);
  EXPECT_EQ(keyword.starter_pack_id, restored_keyword.starter_pack_id);
  EXPECT_EQ(keyword.enforced_by_policy, restored_keyword.enforced_by_policy);
  EXPECT_EQ(keyword.featured_by_policy, restored_keyword.featured_by_policy);

  RemoveKeyword(restored_keyword.id);

  EXPECT_EQ(0U, GetKeywords().size());
}

TEST_F(KeywordTableTest, UpdateKeyword) {
  TemplateURLData keyword(CreateAndAddKeyword());

  keyword.SetKeyword(u"url");
  keyword.originating_url = GURL("http://originating.url/");
  keyword.input_encodings.push_back("Shift_JIS");
  keyword.prepopulate_id = 5;
  keyword.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
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
  EXPECT_EQ(keyword.regulatory_origin, restored_keyword.regulatory_origin);
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
  keyword.id = TemplateURLID(-100);
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
  keyword.id = TemplateURLID(1000);
  AddKeyword(keyword);

  keyword.SetShortName(u"bogus");
  keyword.SetKeyword(u"bogus");
  keyword.id = TemplateURLID(2000);
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
    keyword.id = TemplateURLID(1000);
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
    keyword.id = TemplateURLID(2000);
    AddKeyword(keyword);
    KeywordTable::Keywords keywords(GetKeywords());
    EXPECT_EQ(1U, keywords.size());
    const TemplateURLData& keyword_from_database = keywords.front();
    EXPECT_EQ(keyword.id, keyword_from_database.id);
    EXPECT_EQ(u"bogus name", keyword_from_database.short_name());
    RemoveKeyword(keyword.id);
  }
}

#if BUILDFLAG(IS_WIN)
namespace {

enum class Tamper { kNone, kUrl, kKeyword };

struct TestCase {
  bool encryption_enabled;
  Tamper tamper;
  base::HistogramBase::Sample32 expected_histogram_sample;
  size_t expected_keyword_count;

  std::string Name() const {
    std::string tamper_str = "NoTamper";
    if (tamper == Tamper::kUrl) {
      tamper_str = "TamperUrl";
    }
    if (tamper == Tamper::kKeyword) {
      tamper_str = "TamperKeyword";
    }
    return base::StrCat(
        {encryption_enabled ? "Encryption" : "NoEncryption", tamper_str});
  }
};

}  // namespace

class KeywordTableTestEncryption
    : public KeywordTableTest,
      public ::testing::WithParamInterface<::TestCase> {};

TEST_P(KeywordTableTestEncryption, KeywordBadHash) {
  TemplateURLData keyword(CreateAndAddKeyword());
  {
    KeywordTable::Keywords keywords(GetKeywords());
    EXPECT_EQ(1U, keywords.size());
  }
  CloseDatabase();
  if (GetParam().tamper != Tamper::kNone) {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(file_));
    if (GetParam().tamper == Tamper::kUrl) {
      EXPECT_TRUE(
          db.Execute("UPDATE keywords SET url='http://bad.com/' WHERE id=1"));
    } else {
      EXPECT_TRUE(
          db.Execute("UPDATE keywords SET keyword='badkeyword' WHERE id=1"));
    }
  }
  encryptor_->set_decryption_available_for_testing(
      GetParam().encryption_enabled);
  base::HistogramTester histograms;
  InitDatabase();
  KeywordTable::Keywords keywords(GetKeywords());
  // If decryption is not available, the hash is skipped, otherwise the hash
  // should be invalid and the row dropped.
  histograms.ExpectUniqueSample("Search.KeywordTable.HashValidationStatus",
                                GetParam().expected_histogram_sample, 1);
  EXPECT_EQ(GetParam().expected_keyword_count, keywords.size());
  ExpectStoredKeywordCount(GetParam().expected_keyword_count);
}

INSTANTIATE_TEST_SUITE_P(
    /*empty*/,
    KeywordTableTestEncryption,
    ::testing::Values(
        ::TestCase{.encryption_enabled = false,
                   .tamper = Tamper::kUrl,
                   .expected_histogram_sample = /*kNotVerifiedNoCrypto*/ 4,
                   .expected_keyword_count = 1u},
        ::TestCase{.encryption_enabled = true,
                   .tamper = Tamper::kUrl,
                   .expected_histogram_sample = /*kIncorrectHash*/ 3,
                   .expected_keyword_count = 0},
        ::TestCase{.encryption_enabled = false,
                   .tamper = Tamper::kKeyword,
                   .expected_histogram_sample = /*kNotVerifiedNoCrypto*/ 4,
                   .expected_keyword_count = 1u},
        ::TestCase{.encryption_enabled = true,
                   .tamper = Tamper::kKeyword,
                   .expected_histogram_sample = /*kIncorrectHash*/ 3,
                   .expected_keyword_count = 0},
        ::TestCase{.encryption_enabled = false,
                   .tamper = Tamper::kNone,
                   .expected_histogram_sample = /*kNotVerifiedNoCrypto*/ 4,
                   .expected_keyword_count = 1u},
        ::TestCase{.encryption_enabled = true,
                   .tamper = Tamper::kNone,
                   .expected_histogram_sample = /*kSuccess*/ 0,
                   .expected_keyword_count = 1u}),
    [](const auto& info) { return info.param.Name(); });

TEST_F(KeywordTableTest, KeywordBadCrypto) {
  TemplateURLData keyword(CreateAndAddKeyword());
  {
    KeywordTable::Keywords keywords(GetKeywords());
    EXPECT_EQ(1U, keywords.size());
  }
  CloseDatabase();
  {
    base::HistogramTester histograms;
    // A replacement encryptor with a new key that will make decryption of the
    // hash fail.
    const auto new_encryptor = os_crypt_async::GetTestEncryptorForTesting();
    InitDatabase(new_encryptor);
    {
      // The test row has both built-in IDs set, so both sets need recovery.
      KeywordTable::Keywords keywords(
          GetKeywords(/*expected_prepopulated_decryption_failure=*/true,
                      /*expected_starter_pack_decryption_failure=*/true));
      EXPECT_TRUE(keywords.empty());
    }

    histograms.ExpectUniqueSample("Search.KeywordTable.HashValidationStatus",
                                  /*HashValidationStatus::kDecryptFailed*/ 1,
                                  1);
    ExpectStoredKeywordCount(0);
    // The signal describes this load, rather than sticking after recovery.
    EXPECT_TRUE(GetKeywords().empty());
  }
}

TEST_F(KeywordTableTest, KeywordBadCryptoEngineTypes) {
  for (int prepopulate_id : {-1, 0, 10}) {
    for (int starter_pack_id : {-1, 0, 1}) {
      SCOPED_TRACE(prepopulate_id);
      SCOPED_TRACE(starter_pack_id);
      CloseDatabase();
      InitDatabase();
      TemplateURLData keyword(CreateAndAddKeyword());
      keyword.prepopulate_id = prepopulate_id;
      keyword.starter_pack_id = starter_pack_id;
      UpdateKeyword(keyword);
      CloseDatabase();

      InitDatabase(os_crypt_async::GetTestEncryptorForTesting());
      base::HistogramTester histograms;
      EXPECT_TRUE(
          GetKeywords(
              /*expected_prepopulated_decryption_failure=*/prepopulate_id > 0,
              /*expected_starter_pack_decryption_failure=*/starter_pack_id > 0)
              .empty());
      histograms.ExpectUniqueSample("Search.KeywordTable.HashValidationStatus",
                                    /*kDecryptFailed=*/1, 1);
      ExpectStoredKeywordCount(0);
      EXPECT_TRUE(GetKeywords().empty());
    }
  }
}

TEST_F(KeywordTableTest, KeywordBadCryptoRecoverySurvivesLaterRows) {
  TemplateURLData prepopulated(CreateAndAddKeyword());
  prepopulated.starter_pack_id = 0;
  UpdateKeyword(prepopulated);
  TemplateURLData starter_pack = prepopulated;
  starter_pack.id = TemplateURLID(2);
  starter_pack.prepopulate_id = 0;
  starter_pack.starter_pack_id = 1;
  AddKeyword(starter_pack);
  TemplateURLData custom = prepopulated;
  custom.id = TemplateURLID(3);
  custom.prepopulate_id = 0;
  custom.starter_pack_id = 0;
  AddKeyword(custom);
  CloseDatabase();
  InitDatabase(os_crypt_async::GetTestEncryptorForTesting());

  TemplateURLData valid = custom;
  valid.id = TemplateURLID(4);
  AddKeyword(valid);

  base::HistogramTester histograms;
  // Neither the later custom decryption failure nor a valid row may clear the
  // earlier requests to reconcile prepopulated and starter pack engines.
  EXPECT_THAT(GetKeywords(/*expected_prepopulated_decryption_failure=*/true,
                          /*expected_starter_pack_decryption_failure=*/true),
              testing::ElementsAre(valid));
  histograms.ExpectBucketCount("Search.KeywordTable.HashValidationStatus",
                               /*kDecryptFailed=*/1, 3);
  histograms.ExpectBucketCount("Search.KeywordTable.HashValidationStatus",
                               /*kSuccess=*/0, 1);
  ExpectStoredKeywordCount(1);
}

TEST_F(KeywordTableTest, StarterPackNoCrypto) {
  TemplateURLData keyword(CreateAndAddKeyword());
  keyword.prepopulate_id = 0;
  UpdateKeyword(keyword);
  CloseDatabase();

  const auto unavailable_encryptor =
      os_crypt_async::GetTestEncryptorForTesting();
  unavailable_encryptor->set_decryption_available_for_testing(false);
  InitDatabase(unavailable_encryptor);
  base::HistogramTester histograms;
  EXPECT_THAT(GetKeywords(), testing::ElementsAre(keyword));
  histograms.ExpectUniqueSample("Search.KeywordTable.HashValidationStatus",
                                /*kNotVerifiedNoCrypto=*/4, 1);
  ExpectStoredKeywordCount(1);
}

TEST_F(KeywordTableTest, StarterPackInvalidRowsDoNotRequestRecovery) {
  for (int expected_status : {/*kInvalidHash=*/2, /*kIncorrectHash=*/3,
                              /*empty URL, no validation=*/-1}) {
    SCOPED_TRACE(expected_status);
    TemplateURLData keyword(CreateAndAddKeyword());
    keyword.prepopulate_id = 0;
    UpdateKeyword(keyword);
    auto hash = keyword.GenerateHash();
    if (expected_status == 2) {
      hash.pop_back();
    } else {
      hash.back() ^= 1;
    }
    const auto encrypted_hash =
        encryptor_->EncryptString(std::string(hash.begin(), hash.end()));
    ASSERT_TRUE(encrypted_hash);
    {
      sql::Statement s;
      GetStatement("UPDATE keywords SET url_hash=?, url=? WHERE id=1", &s);
      s.BindBlob(0, *encrypted_hash);
      s.BindString(1, expected_status == -1 ? "" : keyword.url());
      ASSERT_TRUE(s.Run());
    }

    base::HistogramTester histograms;
    EXPECT_TRUE(GetKeywords().empty());
    if (expected_status == -1) {
      histograms.ExpectTotalCount("Search.KeywordTable.HashValidationStatus",
                                  0);
    } else {
      histograms.ExpectUniqueSample("Search.KeywordTable.HashValidationStatus",
                                    expected_status, 1);
    }
    ExpectStoredKeywordCount(0);
  }
}

TEST_F(KeywordTableTest, KeywordInvalidHash) {
  CreateAndAddKeyword();
  const auto encrypted_hash = encryptor_->EncryptString("too short");
  ASSERT_TRUE(encrypted_hash);
  {
    sql::Statement s;
    GetStatement("UPDATE keywords SET url_hash=? WHERE id=1", &s);
    s.BindBlob(0, *encrypted_hash);
    ASSERT_TRUE(s.Run());
  }

  // Invalid hash contents still cause rejection without requesting recovery.
  TemplateURLData valid_keyword;
  valid_keyword.id = TemplateURLID(2);
  valid_keyword.SetURL("https://valid.example/?q={searchTerms}");
  valid_keyword.SetKeyword(u"valid");
  AddKeyword(valid_keyword);

  base::HistogramTester histograms;
  EXPECT_THAT(GetKeywords(), testing::ElementsAre(valid_keyword));
  histograms.ExpectBucketCount("Search.KeywordTable.HashValidationStatus",
                               /*kInvalidHash=*/2, 1);
  histograms.ExpectBucketCount("Search.KeywordTable.HashValidationStatus",
                               /*kSuccess=*/0, 1);
  ExpectStoredKeywordCount(1);
}

namespace {

class KeywordHashKeyProvider : public os_crypt_async::KeyProvider {
 public:
  explicit KeywordHashKeyProvider(std::string tag) : tag_(std::move(tag)) {}

  void GetKey(KeyCallback callback) override {
    const std::array<uint8_t, os_crypt_async::Encryptor::Key::kAES256GCMKeySize>
        key{};
    std::move(callback).Run(
        tag_, os_crypt_async::Encryptor::Key(
                  key, os_crypt_async::mojom::Algorithm::kAES256GCM));
  }

  bool UseForEncryption() override { return true; }

 private:
  const std::string tag_;
};

}  // namespace

TEST_F(KeywordTableTest, KeywordWeakHash) {
  base::test::TaskEnvironment task_environment;
  auto make_encryptor = [](bool with_strong_provider) {
    std::vector<std::pair<os_crypt_async::OSCryptAsync::Precedence,
                          std::unique_ptr<os_crypt_async::KeyProvider>>>
        providers;
    providers.emplace_back(0, std::make_unique<KeywordHashKeyProvider>("v10"));
    if (with_strong_provider) {
      providers.emplace_back(1,
                             std::make_unique<KeywordHashKeyProvider>("v20"));
    }
    os_crypt_async::OSCryptAsync os_crypt(std::move(providers));
    base::test::TestFuture<scoped_refptr<os_crypt_async::Encryptor>> future;
    os_crypt.GetInstance(future.GetCallback());
    return future.Take();
  };
  const auto weak_encryptor = make_encryptor(false);
  const auto strong_encryptor = make_encryptor(true);

  for (bool reject_weak_hashes : {false, true}) {
    SCOPED_TRACE(reject_weak_hashes);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(switches::kRejectWeakKeywordHashes,
                                      reject_weak_hashes);
    CloseDatabase();
    InitDatabase(weak_encryptor);
    TemplateURLData starter_pack(CreateAndAddKeyword());
    starter_pack.id = TemplateURLID(2);
    starter_pack.prepopulate_id = 0;
    AddKeyword(starter_pack);
    EXPECT_EQ(2u, GetKeywords().size());

    CloseDatabase();
    InitDatabase(strong_encryptor);
    base::HistogramTester histograms;
    EXPECT_EQ(reject_weak_hashes ? 0u : 2u, GetKeywords().size());
    histograms.ExpectUniqueSample(
        "Search.KeywordTable.HashValidationStatus",
        reject_weak_hashes ? /*kWeakAlgorithmUsed=*/6 : /*kSuccess=*/0, 2);
    ExpectStoredKeywordCount(reject_weak_hashes ? 0 : 2);
    if (!reject_weak_hashes) {
      RemoveKeyword(TemplateURLID(1));
      RemoveKeyword(TemplateURLID(2));
    }
  }
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(KeywordTableTest, KeywordBadUrl) {
  TemplateURLData keyword(CreateAndAddKeyword());
  {
    KeywordTable::Keywords keywords(GetKeywords());
    EXPECT_EQ(1U, keywords.size());
  }
  CloseDatabase();
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(file_));
    EXPECT_TRUE(db.Execute("UPDATE keywords SET url='' WHERE id=1"));
  }
  InitDatabase();
  KeywordTable::Keywords keywords(GetKeywords());

  // Invalid keyword with empty url should have been dropped.
  EXPECT_TRUE(keywords.empty());
  ExpectStoredKeywordCount(0);
}

TEST_F(KeywordTableTest, MigrateVersion152ExpandHashColumnRetainsData) {
  CloseDatabase();
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(file_));
    ASSERT_TRUE(
        db.Execute("INSERT INTO keywords (id, short_name, keyword, "
                   "favicon_url, url, safe_for_autoreplace, starter_pack_id, "
                   "enforced_by_policy) VALUES (1, 'Test', '@testing', '', "
                   "'chrome://test/?q={searchTerms}', 1, 1234, 1)"));
  }

  std::optional<KeywordTable::Keywords> keywords_no_hash;
  {
    // An Encryptor with no decryption services causes validation to pass,
    // allowing access to retrieve the keyword data without hash verification,
    // which is important as it's not yet present.
    auto encryptor = os_crypt_async::GetTestEncryptorForTesting();
    encryptor->set_decryption_available_for_testing(false);
    InitDatabase(encryptor);
    keywords_no_hash.emplace(GetKeywords());
    CloseDatabase();
  }

  ASSERT_TRUE(keywords_no_hash.has_value());
  ASSERT_EQ(1u, keywords_no_hash->size());

  InitDatabase();
  // Manually invoke the migration script, this generates the hashes from the
  // keyword row.
  ASSERT_TRUE(GetTable()->MigrateToVersion152ExpandHashColumn());

  // Verify that the row survives hash validation during GetKeywords()
  // and that its fields were retained.
  KeywordTable::Keywords keywords(GetKeywords());
  EXPECT_THAT(keywords, ::testing::ContainerEq(keywords_no_hash.value()));
}

TEST_F(KeywordTableTest, MigrationState) {
  KeywordTable* table = GetTable();

  // Initially, it should be empty.
  EXPECT_TRUE(table->GetPrepopulatedEnginesMigrationState().empty());

  // Set some state.
  KeywordTable::PrepopulatedEngineMigrationSet state;
  state.Put(KeywordTable::PrepopulatedEngineMigration::kMigration);
  EXPECT_TRUE(table->SetPrepopulatedEnginesMigrationState(state));

  // Read it back.
  EXPECT_EQ(state, table->GetPrepopulatedEnginesMigrationState());

  // Set both.
  state.Put(KeywordTable::PrepopulatedEngineMigration::kShadowVariants);
  EXPECT_TRUE(table->SetPrepopulatedEnginesMigrationState(state));
  EXPECT_EQ(state, table->GetPrepopulatedEnginesMigrationState());

  // Clear.
  state.Clear();
  EXPECT_TRUE(table->SetPrepopulatedEnginesMigrationState(state));
  EXPECT_TRUE(table->GetPrepopulatedEnginesMigrationState().empty());
}

TEST_F(KeywordTableTest, MigrationStateCompatibility) {
  // Simulate old DB with migration enabled (bool true -> 1).
  ManuallySetMigrationEnabledInDb(1);

  // It should be read as containing only kMigration.
  KeywordTable::PrepopulatedEngineMigrationSet expected_state;
  expected_state.Put(KeywordTable::PrepopulatedEngineMigration::kMigration);
  EXPECT_EQ(expected_state, GetTable()->GetPrepopulatedEnginesMigrationState());

  // Simulate old DB with migration disabled (bool false -> 0).
  ManuallySetMigrationEnabledInDb(0);
  EXPECT_TRUE(GetTable()->GetPrepopulatedEnginesMigrationState().empty());
}
