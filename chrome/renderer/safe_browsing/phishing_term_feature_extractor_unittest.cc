// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_term_feature_extractor.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/mock_feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/murmurhash3_util.h"
#include "chrome/renderer/safe_browsing/test_utils.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using ::testing::Return;

static const uint32_t kMurmurHash3Seed = 2777808611U;

namespace safe_browsing {

class PhishingTermFeatureExtractorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unordered_set<std::string> terms;
    terms.insert("one");
    terms.insert("one one");
    terms.insert("two");
    terms.insert("multi word test");
    terms.insert("capitalization");
    terms.insert("space");
    terms.insert("separator");
    terms.insert("punctuation");
    // Chinese (translation of "hello")
    terms.insert("\xe4\xbd\xa0\xe5\xa5\xbd");
    // Chinese (translation of "goodbye")
    terms.insert("\xe5\x86\x8d\xe8\xa7\x81");

    for (auto it = terms.begin(); it != terms.end(); ++it) {
      term_hashes_.insert(crypto::SHA256HashString(*it));
    }

    std::unordered_set<std::string> words;
    words.insert("one");
    words.insert("two");
    words.insert("multi");
    words.insert("word");
    words.insert("test");
    words.insert("capitalization");
    words.insert("space");
    words.insert("separator");
    words.insert("punctuation");
    words.insert("\xe4\xbd\xa0\xe5\xa5\xbd");
    words.insert("\xe5\x86\x8d\xe8\xa7\x81");

    for (auto it = words.begin(); it != words.end(); ++it) {
      word_hashes_.insert(MurmurHash3String(*it, kMurmurHash3Seed));
    }

    ResetExtractor(3 /* max shingles per page */);
  }

  void ResetExtractor(size_t max_shingles_per_page) {
    extractor_.reset(new PhishingTermFeatureExtractor(
        &term_hashes_,
        &word_hashes_,
        3 /* max_words_per_term */,
        kMurmurHash3Seed,
        max_shingles_per_page,
        4 /* shingle_size */,
        &clock_));
  }

  // Runs the TermFeatureExtractor on |page_text|, waiting for the
  // completion callback.  Returns the success boolean from the callback.
  bool ExtractFeatures(const base::string16* page_text,
                       FeatureMap* features,
                       std::set<uint32_t>* shingle_hashes) {
    success_ = false;
    extractor_->ExtractFeatures(
        page_text, features, shingle_hashes,
        base::BindOnce(&PhishingTermFeatureExtractorTest::ExtractionDone,
                       base::Unretained(this)));
    active_run_loop_ = std::make_unique<base::RunLoop>();
    active_run_loop_->Run();
    return success_;
  }

  void PartialExtractFeatures(const base::string16* page_text,
                              FeatureMap* features,
                              std::set<uint32_t>* shingle_hashes) {
    extractor_->ExtractFeatures(
        page_text, features, shingle_hashes,
        base::BindOnce(&PhishingTermFeatureExtractorTest::ExtractionDone,
                       base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&PhishingTermFeatureExtractorTest::QuitExtraction,
                       base::Unretained(this)));
    active_run_loop_ = std::make_unique<base::RunLoop>();
    active_run_loop_->RunUntilIdle();
  }

  // Completion callback for feature extraction.
  void ExtractionDone(bool success) {
    success_ = success;
    active_run_loop_->QuitWhenIdle();
  }

  void QuitExtraction() {
    extractor_->CancelPendingExtraction();
    active_run_loop_->QuitWhenIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> active_run_loop_;
  MockFeatureExtractorClock clock_;
  std::unique_ptr<PhishingTermFeatureExtractor> extractor_;
  std::unordered_set<std::string> term_hashes_;
  std::unordered_set<uint32_t> word_hashes_;
  bool success_;  // holds the success value from ExtractFeatures
};

TEST_F(PhishingTermFeatureExtractorTest, ExtractFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  base::string16 page_text = ASCIIToUTF16("blah");
  FeatureMap expected_features;  // initially empty
  std::set<uint32_t> expected_shingle_hashes;

  FeatureMap features;
  std::set<uint32_t> shingle_hashes;
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));

  page_text = ASCIIToUTF16("one one");
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("one"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("one one"));
  expected_shingle_hashes.clear();

  features.Clear();
  shingle_hashes.clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));

  page_text = ASCIIToUTF16("bla bla multi word test bla");
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("multi word test"));
  expected_shingle_hashes.clear();
  expected_shingle_hashes.insert(MurmurHash3String("bla bla multi word ",
                                                   kMurmurHash3Seed));
  expected_shingle_hashes.insert(MurmurHash3String("bla multi word test ",
                                                   kMurmurHash3Seed));
  expected_shingle_hashes.insert(MurmurHash3String("multi word test bla ",
                                                   kMurmurHash3Seed));

  features.Clear();
  shingle_hashes.clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));

  // This text has all of the words for one of the terms, but they are
  // not in the correct order.
  page_text = ASCIIToUTF16("bla bla test word multi bla");
  expected_features.Clear();
  expected_shingle_hashes.clear();
  expected_shingle_hashes.insert(MurmurHash3String("bla bla test word ",
                                                   kMurmurHash3Seed));
  expected_shingle_hashes.insert(MurmurHash3String("bla test word multi ",
                                                   kMurmurHash3Seed));
  expected_shingle_hashes.insert(MurmurHash3String("test word multi bla ",
                                                   kMurmurHash3Seed));

  features.Clear();
  shingle_hashes.clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));

  // Test various separators.
  page_text = ASCIIToUTF16("Capitalization plus non-space\n"
                           "separator... punctuation!");
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("capitalization"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("space"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("separator"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("punctuation"));
  expected_shingle_hashes.clear();
  expected_shingle_hashes.insert(
      MurmurHash3String("capitalization plus non space ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(MurmurHash3String("plus non space separator ",
                                                   kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("non space separator punctuation ", kMurmurHash3Seed));

  features.Clear();
  shingle_hashes.clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));

  // Test a page with too many words and we should only 3 minimum hashes.
  page_text = ASCIIToUTF16("This page has way too many words.");
  expected_features.Clear();
  expected_shingle_hashes.clear();
  expected_shingle_hashes.insert(MurmurHash3String("this page has way ",
                                                   kMurmurHash3Seed));
  expected_shingle_hashes.insert(MurmurHash3String("page has way too ",
                                                   kMurmurHash3Seed));
  expected_shingle_hashes.insert(MurmurHash3String("has way too many ",
                                                   kMurmurHash3Seed));
  expected_shingle_hashes.insert(MurmurHash3String("way too many words ",
                                                   kMurmurHash3Seed));
  auto it = expected_shingle_hashes.end();
  expected_shingle_hashes.erase(--it);

  features.Clear();
  shingle_hashes.clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));

  // Test with empty page text.
  page_text = base::string16();
  expected_features.Clear();
  expected_shingle_hashes.clear();
  features.Clear();
  shingle_hashes.clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));

#if !defined(OS_ANDROID)
  // The test code is disabled due to http://crbug.com/392234
  // The client-side detection feature is not enabled on Android yet.
  // If we decided to enable the feature, we need to fix the bug first.

  // Chinese translation of the phrase "hello goodbye hello goodbye". This tests
  // that we can correctly separate terms in languages that don't use spaces.
  page_text =
      base::UTF8ToUTF16("\xe4\xbd\xa0\xe5\xa5\xbd\xe5\x86\x8d\xe8\xa7\x81"
                        "\xe4\xbd\xa0\xe5\xa5\xbd\xe5\x86\x8d\xe8\xa7\x81");
  expected_features.Clear();
  expected_features.AddBooleanFeature(
      features::kPageTerm + std::string("\xe4\xbd\xa0\xe5\xa5\xbd"));
  expected_features.AddBooleanFeature(
      features::kPageTerm + std::string("\xe5\x86\x8d\xe8\xa7\x81"));
  expected_shingle_hashes.clear();
  expected_shingle_hashes.insert(MurmurHash3String(
      "\xe4\xbd\xa0\xe5\xa5\xbd \xe5\x86\x8d\xe8\xa7\x81 "
      "\xe4\xbd\xa0\xe5\xa5\xbd \xe5\x86\x8d\xe8\xa7\x81 ", kMurmurHash3Seed));

  features.Clear();
  shingle_hashes.clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));
#endif
}

TEST_F(PhishingTermFeatureExtractorTest, Continuation) {
  // For this test, we'll cause the feature extraction to run multiple
  // iterations by incrementing the clock.
  ResetExtractor(200 /* max shingles per page */);

  // This page has a total of 30 words.  For the features to be computed
  // correctly, the extractor has to process the entire string of text.
  base::string16 page_text(ASCIIToUTF16("one "));
  for (int i = 0; i < 28; ++i) {
    page_text.append(ASCIIToUTF16(base::StringPrintf("%d ", i)));
  }
  page_text.append(ASCIIToUTF16("two"));

  // Advance the clock 3 ms every 5 words processed, 10 ms between chunks.
  // Note that this assumes kClockCheckGranularity = 5 and
  // kMaxTimePerChunkMs = 10.
  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(3)))
      // Time check after the next 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(6)))
      // Time check after the next 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(9)))
      // Time check after the next 5 words.  This is over the chunk
      // time limit, so a continuation task will be posted.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(12)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(22)))
      // Time check after the next 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(25)))
      // Time check after the next 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(28)))
      // A final check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(30)));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("one"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("two"));
  std::set<uint32_t> expected_shingle_hashes;
  expected_shingle_hashes.insert(
      MurmurHash3String("one 0 1 2 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("0 1 2 3 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("1 2 3 4 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("2 3 4 5 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("3 4 5 6 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("4 5 6 7 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("5 6 7 8 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("6 7 8 9 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("7 8 9 10 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("8 9 10 11 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("9 10 11 12 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("10 11 12 13 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("11 12 13 14 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("12 13 14 15 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("13 14 15 16 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("14 15 16 17 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("15 16 17 18 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("16 17 18 19 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("17 18 19 20 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("18 19 20 21 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("19 20 21 22 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("20 21 22 23 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("21 22 23 24 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("22 23 24 25 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("23 24 25 26 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("24 25 26 27 ", kMurmurHash3Seed));
  expected_shingle_hashes.insert(
      MurmurHash3String("25 26 27 two ", kMurmurHash3Seed));

  FeatureMap features;
  std::set<uint32_t> shingle_hashes;
  ASSERT_TRUE(ExtractFeatures(&page_text, &features, &shingle_hashes));
  ExpectFeatureMapsAreEqual(features, expected_features);
  EXPECT_THAT(expected_shingle_hashes, testing::ContainerEq(shingle_hashes));
  // Make sure none of the mock expectations carry over to the next test.
  ::testing::Mock::VerifyAndClearExpectations(&clock_);

  // Now repeat the test with the same text, but advance the clock faster so
  // that the extraction time exceeds the maximum total time for the feature
  // extractor.  Extraction should fail.  Note that this assumes
  // kMaxTotalTimeMs = 500.
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 5 words,
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(300)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(350)))
      // Time check after the next 5 words.  This is over the limit.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(600)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(620)));

  features.Clear();
  shingle_hashes.clear();
  EXPECT_FALSE(ExtractFeatures(&page_text, &features, &shingle_hashes));
}

TEST_F(PhishingTermFeatureExtractorTest, PartialExtractionTest) {
  std::unique_ptr<base::string16> page_text(
      new base::string16(ASCIIToUTF16("one ")));
  for (int i = 0; i < 28; ++i) {
    page_text->append(ASCIIToUTF16(base::StringPrintf("%d ", i)));
  }

  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(7)))
      // Time check after the next 5 words. This should be greater than
      // kMaxTimePerChunkMs so that we stop and schedule extraction for later.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(14)));

  FeatureMap features;
  std::set<uint32_t> shingle_hashes;
  // Extract first 10 words then stop.
  PartialExtractFeatures(page_text.get(), &features, &shingle_hashes);

  page_text.reset(new base::string16());
  for (int i = 30; i < 58; ++i) {
    page_text->append(ASCIIToUTF16(base::StringPrintf("%d ", i)));
  }
  page_text->append(ASCIIToUTF16("multi word test "));
  features.Clear();
  shingle_hashes.clear();

  // This part doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  // Now extract normally and make sure nothing breaks.
  EXPECT_TRUE(ExtractFeatures(page_text.get(), &features, &shingle_hashes));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("multi word test"));
  ExpectFeatureMapsAreEqual(features, expected_features);
}

}  // namespace safe_browsing
