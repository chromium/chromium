// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/flatbuffer_scorer.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/murmurhash3_util.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/protobuf_scorer.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "crypto/sha2.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "url/gurl.h"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pair;

namespace safe_browsing {

class TestChromeContentRendererClient : public ChromeContentRendererClient {
 public:
  TestChromeContentRendererClient() {}
  ~TestChromeContentRendererClient() override {}
  // Since visited_link_reader_ in ChromeContentRenderClient never get
  // initiated, overrides VisitedLinkedHash() function to prevent crashing.
  uint64_t VisitedLinkHash(const char* canonical_url, size_t length) override {
    return 0;
  }
};

class PhishingClassifierTest : public ChromeRenderViewTest,
                               public ::testing::WithParamInterface<bool> {
 protected:
  PhishingClassifierTest()
      : url_tld_token_net_(features::kUrlTldToken + std::string("net")),
        page_link_domain_phishing_(features::kPageLinkDomain +
                                   std::string("phishing.com")),
        page_term_login_(features::kPageTerm + std::string("login")),
        page_text_(u"login"),
        phishy_score_(PhishingClassifier::kInvalidScore) {}

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    if (GetParam()) {
      PrepareModel();
    } else {
      PrepareFlatModel();
    }
    SetUpClassifier();

    base::DiscardableMemoryAllocator::SetInstance(&test_allocator_);
  }

  void PrepareFlatModel() {
    flatbuffers::FlatBufferBuilder builder(1024);
    std::vector<flatbuffers::Offset<flat::Hash>> hashes;
    // Make sure this is sorted.
    std::vector<std::string> original_hashes_vector = {
        crypto::SHA256HashString(url_tld_token_net_),
        crypto::SHA256HashString(page_link_domain_phishing_),
        crypto::SHA256HashString(page_term_login_),
        crypto::SHA256HashString("login"),
        crypto::SHA256HashString(features::kUrlTldToken + std::string("net")),
        crypto::SHA256HashString(features::kPageLinkDomain +
                                 std::string("phishing.com")),
        crypto::SHA256HashString(features::kPageTerm + std::string("login")),
        crypto::SHA256HashString("login")};
    std::vector<std::string> hashes_vector = original_hashes_vector;
    std::sort(hashes_vector.begin(), hashes_vector.end());
    std::vector<int> indices_map_from_original;
    for (const auto& original_hash : original_hashes_vector) {
      indices_map_from_original.push_back(
          std::find(hashes_vector.begin(), hashes_vector.end(), original_hash) -
          hashes_vector.begin());
    }
    for (std::string& feature : hashes_vector) {
      std::vector<uint8_t> hash_data(feature.begin(), feature.end());
      hashes.push_back(flat::CreateHashDirect(builder, &hash_data));
    }
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flat::Hash>>>
        hashes_flat = builder.CreateVector(hashes);

    std::vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>> rules;

    // Add a default rule with a non-phishy weight.
    std::vector<int32_t> rule_feature1 = {};
    rules.push_back(flat::ClientSideModel_::CreateRuleDirect(
        builder, &rule_feature1, -1.0));
    // To give a phishy score, the total weight needs to be >= 0
    // (0.5 when converted to a probability).  This will only happen
    // if all of the listed features are present.
    std::vector<int32_t> rule_feature2 = {indices_map_from_original[0],
                                          indices_map_from_original[1],
                                          indices_map_from_original[2]};
    std::sort(rule_feature2.begin(), rule_feature2.end());
    rules.push_back(
        flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature2, 1.0));
    flatbuffers::Offset<
        flatbuffers::Vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>>>
        rules_flat = builder.CreateVector(rules);

    std::vector<int32_t> page_terms_vector = {indices_map_from_original[3]};
    flatbuffers::Offset<flatbuffers::Vector<int32_t>> page_term_flat =
        builder.CreateVector(page_terms_vector);

    uint32_t murmur_hash_seed = 2777808611U;
    std::vector<uint32_t> page_words_vector = {
        MurmurHash3String("login", murmur_hash_seed)};
    flatbuffers::Offset<flatbuffers::Vector<uint32_t>> page_word_flat =
        builder.CreateVector(page_words_vector);

    std::vector<flatbuffers::Offset<flat::TfLiteModelMetadata_::Threshold>>
        thresholds_vector = {};
    flatbuffers::Offset<flat::TfLiteModelMetadata> tflite_metadata_flat =
        flat::CreateTfLiteModelMetadataDirect(builder, 0, &thresholds_vector, 0,
                                              0);

    flat::ClientSideModelBuilder csd_model_builder(builder);
    csd_model_builder.add_hashes(hashes_flat);
    csd_model_builder.add_rule(rules_flat);
    csd_model_builder.add_page_term(page_term_flat);
    csd_model_builder.add_page_word(page_word_flat);
    csd_model_builder.add_max_words_per_term(1);
    csd_model_builder.add_murmur_hash_seed(murmur_hash_seed);
    csd_model_builder.add_max_shingles_per_page(100);
    csd_model_builder.add_shingle_size(3);
    csd_model_builder.add_tflite_metadata(tflite_metadata_flat);

    builder.Finish(csd_model_builder.Finish());
    std::string model_str(reinterpret_cast<char*>(builder.GetBufferPointer()),
                          builder.GetSize());

    mapped_region_ =
        base::ReadOnlySharedMemoryRegion::Create(model_str.length());
    ASSERT_TRUE(mapped_region_.IsValid());
    memcpy(mapped_region_.mapping.memory(), model_str.data(),
           model_str.length());
    ScorerStorage::GetInstance()->SetScorer(FlatBufferModelScorer::Create(
        mapped_region_.region.Duplicate(), base::File()));
  }

  void PrepareModel() {
    // Construct a model to test with.  We include one feature from each of
    // the feature extractors, which allows us to verify that they all ran.
    ClientSideModel model;

    model.add_hashes(crypto::SHA256HashString(url_tld_token_net_));
    model.add_hashes(crypto::SHA256HashString(page_link_domain_phishing_));
    model.add_hashes(crypto::SHA256HashString(page_term_login_));
    model.add_hashes(crypto::SHA256HashString("login"));
    model.add_hashes(crypto::SHA256HashString(features::kUrlTldToken +
                                              std::string("net")));
    model.add_hashes(crypto::SHA256HashString(features::kPageLinkDomain +
                                              std::string("phishing.com")));
    model.add_hashes(crypto::SHA256HashString(features::kPageTerm +
                                              std::string("login")));
    model.add_hashes(crypto::SHA256HashString("login"));

    // Add a default rule with a non-phishy weight.
    ClientSideModel::Rule* rule = model.add_rule();
    rule->set_weight(-1.0);

    // To give a phishy score, the total weight needs to be >= 0
    // (0.5 when converted to a probability).  This will only happen
    // if all of the listed features are present.
    rule = model.add_rule();
    rule->add_feature(0);
    rule->add_feature(1);
    rule->add_feature(2);
    rule->set_weight(1.0);

    model.add_page_term(3);
    model.set_murmur_hash_seed(2777808611U);
    model.add_page_word(MurmurHash3String("login", model.murmur_hash_seed()));
    model.set_max_words_per_term(1);
    model.set_max_shingles_per_page(100);
    model.set_shingle_size(3);

    ScorerStorage::GetInstance()->SetScorer(
        ProtobufModelScorer::Create(model.SerializeAsString(), base::File()));
  }

  void SetUpClassifier() {
    classifier_ = std::make_unique<PhishingClassifier>(GetMainRenderFrame());
  }

  // Helper method to start phishing classification.
  void RunPhishingClassifier(const std::u16string* page_text) {
    feature_map_.Clear();

    classifier_->BeginClassification(
        page_text,
        base::BindOnce(&PhishingClassifierTest::ClassificationFinished,
                       base::Unretained(this)));
    run_loop_.Run();
  }

  // Completion callback for classification.
  void ClassificationFinished(const ClientPhishingRequest& verdict) {
    phishy_score_ = verdict.client_score();
    for (int i = 0; i < verdict.feature_map_size(); ++i) {
      feature_map_.AddRealFeature(verdict.feature_map(i).name(),
                                  verdict.feature_map(i).value());
    }
    is_phishing_ = verdict.is_phishing();
    is_dom_match_ = verdict.is_dom_match();

    run_loop_.Quit();
  }

  void LoadHtml(const GURL& url, const std::string& content) {
    LoadHTMLWithUrlOverride(content.c_str(), url.spec().c_str());
  }

  content::ContentRendererClient* CreateContentRendererClient() override {
    ChromeContentRendererClient* client = new TestChromeContentRendererClient();
    InitChromeContentRendererClient(client);
    return client;
  }

  std::string response_content_;
  std::unique_ptr<PhishingClassifier> classifier_;
  base::RunLoop run_loop_;
  base::MappedReadOnlyRegion mapped_region_;

  // Features that are in the model.
  const std::string url_tld_token_net_;
  const std::string page_link_domain_phishing_;
  const std::string page_term_login_;
  std::u16string page_text_;

  // Outputs of phishing classifier.
  FeatureMap feature_map_;
  float phishy_score_;
  bool is_phishing_;
  bool is_dom_match_;

  // A DiscardableMemoryAllocator is needed for certain Skia operations.
  base::TestDiscardableMemoryAllocator test_allocator_;
};

TEST_P(PhishingClassifierTest, TestClassificationOfPhishingDotComHttp) {
  LoadHtml(
      GURL("http://host.net"),
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>");
  RunPhishingClassifier(&page_text_);

  // Note: features.features() might contain other features that simply aren't
  // in the model.
  EXPECT_THAT(feature_map_.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_link_domain_phishing_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_FLOAT_EQ(0.5, phishy_score_);
  EXPECT_TRUE(is_phishing_);
}

TEST_P(PhishingClassifierTest, TestClassificationOfPhishingDotComHttps) {
  // Host the target page on HTTPS.
  LoadHtml(
      GURL("https://host.net"),
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>");
  RunPhishingClassifier(&page_text_);

  // Note: features.features() might contain other features that simply aren't
  // in the model.
  EXPECT_THAT(feature_map_.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_link_domain_phishing_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_FLOAT_EQ(0.5, phishy_score_);
  EXPECT_TRUE(is_phishing_);
}

TEST_P(PhishingClassifierTest, TestClassificationOfSafeDotComHttp) {
  // Change the link domain to something non-phishy.
  LoadHtml(GURL("http://host.net"),
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_THAT(feature_map_.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_THAT(feature_map_.features(),
              Not(Contains(Pair(page_link_domain_phishing_, 1.0))));
  EXPECT_GE(phishy_score_, 0.0);
  EXPECT_LT(phishy_score_, 0.5);
  EXPECT_FALSE(is_phishing_);
}

TEST_P(PhishingClassifierTest, TestClassificationOfSafeDotComHttps) {
  // Host target page in HTTPS and change the link domain to something
  // non-phishy.
  LoadHtml(GURL("https://host.net"),
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_THAT(feature_map_.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_THAT(feature_map_.features(),
              Not(Contains(Pair(page_link_domain_phishing_, 1.0))));
  EXPECT_GE(phishy_score_, 0.0);
  EXPECT_LT(phishy_score_, 0.5);
  EXPECT_FALSE(is_phishing_);
}

TEST_P(PhishingClassifierTest, TestClassificationWhenNoTld) {
  // Extraction should fail for this case since there is no TLD.
  LoadHtml(GURL("http://localhost"), "<html><body>content</body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_EQ(0U, feature_map_.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score_);
  EXPECT_FALSE(is_phishing_);
}

TEST_P(PhishingClassifierTest, TestClassificationWhenSchemeNotSupported) {
  // Extraction should also fail for this case because the URL is not http or
  // https.
  LoadHtml(GURL("file://host.net"), "<html><body>content</body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_EQ(0U, feature_map_.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score_);
  EXPECT_FALSE(is_phishing_);
}

TEST_P(PhishingClassifierTest, DisableDetection) {
  EXPECT_TRUE(classifier_->is_ready());
  // Set a NULL scorer, which turns detection back off.
  ScorerStorage::GetInstance()->SetScorer(nullptr);
  EXPECT_FALSE(classifier_->is_ready());
}

TEST_P(PhishingClassifierTest, TestPhishingPagesAreDomMatches) {
  LoadHtml(
      GURL("http://host.net"),
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_TRUE(is_phishing_);
  EXPECT_TRUE(is_dom_match_);
}

TEST_P(PhishingClassifierTest, TestSafePagesAreNotDomMatches) {
  LoadHtml(GURL("http://host.net"),
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_FALSE(is_phishing_);
  EXPECT_FALSE(is_dom_match_);
}

INSTANTIATE_TEST_SUITE_P(CSDModelTypes,
                         PhishingClassifierTest,
                         ::testing::Bool());

// TODO(jialiul): Add test to verify that classification only starts on GET
// method. It seems there is no easy way to simulate a HTTP POST in
// ChromeRenderViewTest.

}  // namespace safe_browsing
