// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/safe_browsing/murmurhash3_util.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
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

class PhishingClassifierTest : public ChromeRenderViewTest {
 protected:
  PhishingClassifierTest()
      : url_tld_token_net_(features::kUrlTldToken + std::string("net")),
        page_link_domain_phishing_(features::kPageLinkDomain +
                                   std::string("phishing.com")),
        page_term_login_(features::kPageTerm + std::string("login")),
        page_text_(base::ASCIIToUTF16("login")),
        phishy_score_(PhishingClassifier::kInvalidScore) {}

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    PrepareModel();
    SetUpClassifier();

    base::DiscardableMemoryAllocator::SetInstance(&test_allocator_);
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

    // Add an empty visual target to ensure visual detection runs.
    model.mutable_vision_model()->add_targets();

    scorer_.reset(Scorer::Create(model.SerializeAsString()));
    ASSERT_TRUE(scorer_.get());
  }

  void SetUpClassifier() {
    classifier_ =
        std::make_unique<PhishingClassifier>(view_->GetMainRenderFrame());
    // No scorer yet, so the classifier is not ready.
    ASSERT_FALSE(classifier_->is_ready());

    // Now set the scorer.
    classifier_->set_phishing_scorer(scorer_.get());
    ASSERT_TRUE(classifier_->is_ready());
  }

  // Helper method to start phishing classification.
  void RunPhishingClassifier(const base::string16* page_text) {
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
    screenshot_digest_ = verdict.screenshot_digest();
    screenshot_phash_ = verdict.screenshot_phash();
    phash_dimension_size_ = verdict.phash_dimension_size();

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
  std::unique_ptr<Scorer> scorer_;
  std::unique_ptr<PhishingClassifier> classifier_;
  base::RunLoop run_loop_;

  // Features that are in the model.
  const std::string url_tld_token_net_;
  const std::string page_link_domain_phishing_;
  const std::string page_term_login_;
  base::string16 page_text_;

  // Outputs of phishing classifier.
  FeatureMap feature_map_;
  float phishy_score_;
  bool is_phishing_;
  std::string screenshot_digest_;
  std::string screenshot_phash_;
  int phash_dimension_size_;

  // A DiscardableMemoryAllocator is needed for certain Skia operations.
  base::TestDiscardableMemoryAllocator test_allocator_;
};

TEST_F(PhishingClassifierTest, TestClassificationOfPhishingDotComHttp) {
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

TEST_F(PhishingClassifierTest, TestClassificationOfPhishingDotComHttps) {
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

TEST_F(PhishingClassifierTest, TestClassificationOfSafeDotComHttp) {
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

TEST_F(PhishingClassifierTest, TestClassificationOfSafeDotComHttps) {
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

TEST_F(PhishingClassifierTest, TestClassificationWhenNoTld) {
  // Extraction should fail for this case since there is no TLD.
  LoadHtml(GURL("http://localhost"), "<html><body>content</body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_EQ(0U, feature_map_.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score_);
  EXPECT_FALSE(is_phishing_);
}

TEST_F(PhishingClassifierTest, TestClassificationWhenSchemeNotSupported) {
  // Extraction should also fail for this case because the URL is not http or
  // https.
  LoadHtml(GURL("file://host.net"), "<html><body>content</body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_EQ(0U, feature_map_.features().size());
  EXPECT_EQ(PhishingClassifier::kInvalidScore, phishy_score_);
  EXPECT_FALSE(is_phishing_);
}

TEST_F(PhishingClassifierTest, DisableDetection) {
  EXPECT_TRUE(classifier_->is_ready());
  // Set a NULL scorer, which turns detection back off.
  classifier_->set_phishing_scorer(NULL);
  EXPECT_FALSE(classifier_->is_ready());
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
TEST_F(PhishingClassifierTest, TestSendsVisualHash) {
  LoadHtml(GURL("https://host.net"),
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_EQ(phash_dimension_size_, 48);
  EXPECT_FALSE(screenshot_phash_.empty());
}

TEST_F(PhishingClassifierTest, TestSendsVisualDigest) {
  LoadHtml(GURL("https://host.net"),
           "<html><body><a href=\"http://safe.com/\">login</a></body></html>");
  RunPhishingClassifier(&page_text_);

  EXPECT_FALSE(screenshot_digest_.empty());
}
#endif

// TODO(jialiul): Add test to verify that classification only starts on GET
// method. It seems there is no easy way to simulate a HTTP POST in
// ChromeRenderViewTest.

}  // namespace safe_browsing
