// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/murmurhash3_util.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
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
  uint64_t VisitedLinkHash(std::string_view canonical_url) override {
    return 0;
  }
};

class PhishingClassifierTest
    : public ChromeRenderViewTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  PhishingClassifierTest()
      : url_tld_token_net_(features::kUrlTldToken + std::string("net")),
        page_link_domain_phishing_(features::kPageLinkDomain +
                                   std::string("phishing.com")),
        page_term_login_(features::kPageTerm + std::string("login")),
        page_text_(
            base::MakeRefCounted<const base::RefCountedString16>(u"login")) {}

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    PrepareFlatModel();
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
          base::ranges::find(hashes_vector, original_hash) -
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
        thresholds_vector;
    thresholds_vector = {flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "502fd246eb6fad3eae0387c54e4ebe74", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "7c4065b088444b37d273872b771e6940", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "712036bd72bf185a2a4f88de9141d02d", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "9e9c15bfa7cb3f8699e2271116a4175c", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "6c2cb3f559e7a03f37dd873fc007dc65", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "1cbeb74661a5e7e05c993f2524781611", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "989790016b6adca9d46b9c8ec6b8fe3a", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "501067590331ca2d243c669e6084c47e", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "40aed7e33c100058e54c73af3ed49524", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "62f53ea23c7ad2590db711235a45fd38", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "ee6fb9baa44f192bc3c53d8d3c6f7a3d", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "ea54b0830d871286e2b4023bbb431710", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "25645a55b844f970337218ea8f1f26b7", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "c9a8640be09f97f170f1a2708058c48f", 2.0),
                         flat::TfLiteModelMetadata_::CreateThresholdDirect(
                             builder, "953255ea26aa8578d06593ff33e99298", 2.0)};
    flatbuffers::Offset<flat::TfLiteModelMetadata> tflite_metadata_flat =
        flat::CreateTfLiteModelMetadataDirect(builder, 0, &thresholds_vector,
                                              48, 48);

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
    csd_model_builder.add_dom_model_version(123);

    builder.Finish(csd_model_builder.Finish());
    std::string model_str(reinterpret_cast<char*>(builder.GetBufferPointer()),
                          builder.GetSize());

    mapped_region_ =
        base::ReadOnlySharedMemoryRegion::Create(model_str.length());
    ASSERT_TRUE(mapped_region_.IsValid());
    memcpy(mapped_region_.mapping.memory(), model_str.data(),
           model_str.length());
    base::File tflite_model;
    base::FilePath tflite_path;
    GetTfliteModelPath(&tflite_path),
        tflite_model = base::File(
            tflite_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ScorerStorage::GetInstance()->SetScorer(Scorer::Create(
        mapped_region_.region.Duplicate(), std::move(tflite_model)));
  }

  void GetTfliteModelPath(base::FilePath* path) {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, path));
#if BUILDFLAG(IS_ANDROID)
    *path = path->AppendASCII("safe_browsing")
                .AppendASCII("visual_model_android.tflite");
#else
    *path = path->AppendASCII("safe_browsing")
                .AppendASCII("visual_model_desktop.tflite");
#endif
  }

  void SetUpClassifier() {
    classifier_ = std::make_unique<PhishingClassifier>(GetMainRenderFrame());
  }

  // Helper method to start phishing classification.
  void RunPhishingClassifier(
      scoped_refptr<const base::RefCountedString16> page_text) {
    feature_map_.Clear();

    classifier_->BeginClassification(
        page_text,
        base::BindOnce(&PhishingClassifierTest::ClassificationFinished,
                       base::Unretained(this)));
    run_loop_.Run();
  }

  // Completion callback for classification.
  void ClassificationFinished(
      const ClientPhishingRequest& verdict,
      PhishingClassifier::Result phishing_classifier_result) {
    verdict_ = verdict;
    for (int i = 0; i < verdict.feature_map_size(); ++i) {
      feature_map_.AddRealFeature(verdict.feature_map(i).name(),
                                  verdict.feature_map(i).value());
    }
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
  scoped_refptr<const base::RefCountedString16> page_text_;

  // Outputs of phishing classifier.
  ClientPhishingRequest verdict_;
  FeatureMap feature_map_;

  // A DiscardableMemoryAllocator is needed for certain Skia operations.
  base::TestDiscardableMemoryAllocator test_allocator_;
};

TEST_F(PhishingClassifierTest, TestClassificationOfPhishingDotComHttp) {
  LoadHtml(
      GURL("http://host.net"),
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>");
  RunPhishingClassifier(page_text_);

  // Note: features.features() might contain other features that simply aren't
  // in the model.
  EXPECT_THAT(feature_map_.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_link_domain_phishing_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_FLOAT_EQ(0.5, verdict_.client_score());
  EXPECT_TRUE(verdict_.is_phishing());
}

TEST_F(PhishingClassifierTest, TestClassificationOfPhishingDotComHttps) {
  // Host the target page on HTTPS.
  LoadHtml(
      GURL("https://host.net"),
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>");
  RunPhishingClassifier(page_text_);

  // Note: features.features() might contain other features that simply aren't
  // in the model.
  EXPECT_THAT(feature_map_.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_link_domain_phishing_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_FLOAT_EQ(0.5, verdict_.client_score());
  EXPECT_TRUE(verdict_.is_phishing());
}

TEST_F(PhishingClassifierTest, TestClassificationOfSafeDotComHttp) {
  // Change the link domain to something non-phishy.
  LoadHtml(GURL("http://host.net"),
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  RunPhishingClassifier(page_text_);

  EXPECT_THAT(feature_map_.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_THAT(feature_map_.features(),
              Not(Contains(Pair(page_link_domain_phishing_, 1.0))));
  EXPECT_GE(verdict_.client_score(), 0.0);
  EXPECT_LT(verdict_.client_score(), 0.5);
  EXPECT_FALSE(verdict_.is_phishing());
}

TEST_F(PhishingClassifierTest, TestClassificationOfSafeDotComHttps) {
  // Host target page in HTTPS and change the link domain to something
  // non-phishy.
  LoadHtml(GURL("https://host.net"),
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  RunPhishingClassifier(page_text_);

  EXPECT_THAT(feature_map_.features(),
              AllOf(Contains(Pair(url_tld_token_net_, 1.0)),
                    Contains(Pair(page_term_login_, 1.0))));
  EXPECT_THAT(feature_map_.features(),
              Not(Contains(Pair(page_link_domain_phishing_, 1.0))));
  EXPECT_GE(verdict_.client_score(), 0.0);
  EXPECT_LT(verdict_.client_score(), 0.5);
  EXPECT_FALSE(verdict_.is_phishing());
}

TEST_F(PhishingClassifierTest, TestClassificationWhenNoTld) {
  // Extraction should fail for this case since there is no TLD.
  LoadHtml(GURL("http://localhost"), "<html><body>content</body></html>");
  RunPhishingClassifier(page_text_);

  EXPECT_EQ(0U, feature_map_.features().size());
  EXPECT_EQ(PhishingClassifier::kClassifierFailed,
            static_cast<int>(verdict_.client_score()));
  EXPECT_FALSE(verdict_.is_phishing());
}

TEST_F(PhishingClassifierTest, TestClassificationWhenSchemeNotSupported) {
  // Extraction should also fail for this case because the URL is not http or
  // https.
  LoadHtml(GURL("file://host.net"), "<html><body>content</body></html>");
  RunPhishingClassifier(page_text_);

  EXPECT_EQ(0U, feature_map_.features().size());
  EXPECT_EQ(PhishingClassifier::kClassifierFailed,
            static_cast<int>(verdict_.client_score()));
  EXPECT_FALSE(verdict_.is_phishing());
}

TEST_F(PhishingClassifierTest, DisableDetection) {
  EXPECT_TRUE(classifier_->is_ready());
  // Set a NULL scorer, which turns detection back off.
  ScorerStorage::GetInstance()->SetScorer(nullptr);
  EXPECT_FALSE(classifier_->is_ready());
}

TEST_F(PhishingClassifierTest, TestPhishingPagesAreDomMatches) {
  LoadHtml(
      GURL("http://host.net"),
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>");
  RunPhishingClassifier(page_text_);

  EXPECT_NE(PhishingClassifier::kClassifierFailed, verdict_.client_score());
  EXPECT_TRUE(verdict_.is_phishing());
  EXPECT_TRUE(verdict_.is_dom_match());
}

TEST_F(PhishingClassifierTest, TestSafePagesAreNotDomMatches) {
  LoadHtml(GURL("http://host.net"),
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  RunPhishingClassifier(page_text_);

  EXPECT_NE(PhishingClassifier::kClassifierFailed, verdict_.client_score());
  EXPECT_FALSE(verdict_.is_phishing());
  EXPECT_FALSE(verdict_.is_dom_match());
}

TEST_F(PhishingClassifierTest, TestDomModelVersionPopulated) {
  LoadHtml(
      GURL("http://host.net"),
      "<html><body><a href=\"http://phishing.com/\">login</a></body></html>");
  RunPhishingClassifier(page_text_);

  EXPECT_EQ(verdict_.dom_model_version(), 123);
}

// TODO(jialiul): Add test to verify that classification only starts on GET
// method. It seems there is no easy way to simulate a HTTP POST in
// ChromeRenderViewTest.

}  // namespace safe_browsing
