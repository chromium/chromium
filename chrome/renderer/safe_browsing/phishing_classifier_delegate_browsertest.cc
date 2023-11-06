// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier_delegate.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {

std::string GetFlatBufferString(int version) {
  flatbuffers::FlatBufferBuilder builder(1024);
  std::vector<flatbuffers::Offset<flat::Hash>> hashes;
  // Make sure this is sorted.
  std::vector<std::string> hashes_vector = {"feature1", "feature2", "feature3",
                                            "token one", "token two"};
  for (std::string& feature : hashes_vector) {
    std::vector<uint8_t> hash_data(feature.begin(), feature.end());
    hashes.push_back(flat::CreateHashDirect(builder, &hash_data));
  }
  flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flat::Hash>>>
      hashes_flat = builder.CreateVector(hashes);

  std::vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>> rules;
  std::vector<int32_t> rule_feature1 = {};
  std::vector<int32_t> rule_feature2 = {0};
  std::vector<int32_t> rule_feature3 = {0, 1};
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature1, 0.5));
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature2, 2));
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature3, 3));
  flatbuffers::Offset<
      flatbuffers::Vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>>>
      rules_flat = builder.CreateVector(rules);

  std::vector<int32_t> page_terms_vector = {3, 4};
  flatbuffers::Offset<flatbuffers::Vector<int32_t>> page_term_flat =
      builder.CreateVector(page_terms_vector);

  std::vector<uint32_t> page_words_vector = {1000U, 2000U, 3000U};
  flatbuffers::Offset<flatbuffers::Vector<uint32_t>> page_word_flat =
      builder.CreateVector(page_words_vector);

  std::vector<
      flatbuffers::Offset<safe_browsing::flat::TfLiteModelMetadata_::Threshold>>
      thresholds_vector = {};
  flatbuffers::Offset<flat::TfLiteModelMetadata> tflite_metadata_flat =
      flat::CreateTfLiteModelMetadataDirect(builder, 0, &thresholds_vector, 0,
                                            0);

  flat::ClientSideModelBuilder csd_model_builder(builder);
  csd_model_builder.add_hashes(hashes_flat);
  csd_model_builder.add_rule(rules_flat);
  csd_model_builder.add_page_term(page_term_flat);
  csd_model_builder.add_page_word(page_word_flat);
  csd_model_builder.add_max_words_per_term(2);
  csd_model_builder.add_murmur_hash_seed(12345U);
  csd_model_builder.add_max_shingles_per_page(10);
  csd_model_builder.add_shingle_size(3);
  csd_model_builder.add_tflite_metadata(tflite_metadata_flat);
  csd_model_builder.add_version(version);

  builder.Finish(csd_model_builder.Finish());
  return std::string(reinterpret_cast<char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

class MockPhishingClassifier : public PhishingClassifier {
 public:
  explicit MockPhishingClassifier(content::RenderFrame* render_frame)
      : PhishingClassifier(render_frame) {}

  MockPhishingClassifier(const MockPhishingClassifier&) = delete;
  MockPhishingClassifier& operator=(const MockPhishingClassifier&) = delete;

  ~MockPhishingClassifier() override = default;

  MOCK_METHOD2(BeginClassification, void(const std::u16string*, DoneCallback));
  MOCK_METHOD0(CancelPendingClassification, void());
};
}  // namespace

class PhishingClassifierDelegateTest : public ChromeRenderViewTest {
 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    content::RenderFrame* render_frame = GetMainRenderFrame();
    classifier_ = new StrictMock<MockPhishingClassifier>(render_frame);
    render_frame->GetAssociatedInterfaceRegistry()->RemoveInterface(
        mojom::PhishingDetector::Name_);
    delegate_ = PhishingClassifierDelegate::Create(render_frame, classifier_);
    classifier_not_ready_ = false;
  }

  // Runs the ClassificationDone callback, then verify if message sent
  // by FakeRenderThread is correct.
  void RunAndVerifyClassificationDone(const ClientPhishingRequest& verdict) {
    delegate_->ClassificationDone(verdict);
  }

  void OnStartPhishingDetection(const GURL& url) {
    delegate_->StartPhishingDetection(
        url, base::BindOnce(&PhishingClassifierDelegateTest::VerifyRequestProto,
                            base::Unretained(this)));
  }

  void SimulateRedirection(const GURL& redir_url) {
    delegate_->last_url_received_from_browser_ = redir_url;
  }

  void SimulatePageTrantitionForwardOrBack(const char* html, const char* url) {
    LoadHTMLWithUrlOverride(html, url);
    delegate_->last_main_frame_transition_ = ui::PAGE_TRANSITION_FORWARD_BACK;
  }

  void VerifyRequestProto(mojom::PhishingDetectorResult result,
                          const std::string& request_proto) {
    if (result == mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY) {
      classifier_not_ready_ = true;
      return;
    }

    if (result != mojom::PhishingDetectorResult::SUCCESS)
      return;

    ClientPhishingRequest verdict;
    ASSERT_TRUE(verdict.ParseFromString(request_proto));
    EXPECT_EQ("http://host.test/", verdict.url());
    EXPECT_EQ(0.8f, verdict.client_score());
    EXPECT_FALSE(verdict.is_phishing());
  }

  void SetScorer(int model_version) {
    std::string model_str = GetFlatBufferString(model_version);
    base::MappedReadOnlyRegion mapped_region =
        base::ReadOnlySharedMemoryRegion::Create(model_str.length());
    memcpy(mapped_region.mapping.memory(), model_str.data(),
           model_str.length());

    ScorerStorage::GetInstance()->SetScorer(
        Scorer::Create(mapped_region.region.Duplicate(), base::File()));
  }

  raw_ptr<StrictMock<MockPhishingClassifier>, DanglingUntriaged>
      classifier_;  // Owned by |delegate_|.
  raw_ptr<PhishingClassifierDelegate, DanglingUntriaged>
      delegate_;  // Owned by the RenderFrame.
  bool classifier_not_ready_;
};

TEST_F(PhishingClassifierDelegateTest, Navigation) {
  SetScorer(/*model_version=*/1);
  ASSERT_TRUE(classifier_->is_ready());

  // Test an initial load.  We expect classification to happen normally.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  std::string html = "<html><body>dummy</body></html>";
  GURL url("http://host.test/index.html");
  LoadHTMLWithUrlOverride(html.c_str(), url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);

  OnStartPhishingDetection(url);
  std::u16string page_text = u"dummy";
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    delegate_->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // Reloading the same page will trigger a new classification.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  LoadHTMLWithUrlOverride(html.c_str(), url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);

  OnStartPhishingDetection(url);
  page_text = u"dummy";
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  page_text = u"dummy";
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  OnStartPhishingDetection(url);
  page_text = u"dummy";
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // Same document navigation works similarly to a subframe navigation, but see
  // the TODO in PhishingClassifierDelegate::DidCommitProvisionalLoad.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  OnSameDocumentNavigation(GetMainFrame(), true);
  Mock::VerifyAndClearExpectations(classifier_);

  OnStartPhishingDetection(url);
  page_text = u"dummy";
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // Now load a new toplevel page, which should trigger another classification.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL new_url("http://host2.com");
  LoadHTMLWithUrlOverride("dummy2", new_url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);

  page_text = u"dummy2";
  OnStartPhishingDetection(new_url);
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    delegate_->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // No classification should happen on back/forward navigation.
  // Note: in practice, the browser will not send a StartPhishingDetection IPC
  // in this case.  However, we want to make sure that the delegate behaves
  // correctly regardless.
  EXPECT_CALL(*classifier_, CancelPendingClassification()).Times(1);
  // Simulate a go back navigation, i.e. back to http://host.test/index.html.
  SimulatePageTrantitionForwardOrBack(html.c_str(), url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  page_text = u"dummy";
  OnStartPhishingDetection(new_url);
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  // Simulate a go forward navigation, i.e. forward to http://host.test
  SimulatePageTrantitionForwardOrBack("dummy2", new_url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);

  page_text = u"dummy2";
  OnStartPhishingDetection(url);
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // Now go back again and navigate to a different place within
  // the same page. No classification should happen.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  // Simulate a go back again to http://host.test/index.html
  SimulatePageTrantitionForwardOrBack(html.c_str(), url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);

  page_text = u"dummy";
  OnStartPhishingDetection(url);
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  // Same document navigation.
  OnSameDocumentNavigation(GetMainFrame(), true);
  Mock::VerifyAndClearExpectations(classifier_);

  OnStartPhishingDetection(url);
  page_text = u"dummy";
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, NoPhishingModel) {
  ASSERT_FALSE(classifier_->is_ready());
  ScorerStorage::GetInstance()->SetScorer(nullptr);
  // The scorer is nullptr so the classifier should still not be ready.
  ASSERT_FALSE(classifier_->is_ready());
}

TEST_F(PhishingClassifierDelegateTest, HasFlatBufferModel) {
  ASSERT_FALSE(classifier_->is_ready());

  SetScorer(/*model_version=*/1);
  ASSERT_TRUE(classifier_->is_ready());

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, HasVisualTfLiteModel) {
  ASSERT_FALSE(classifier_->is_ready());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("visual_model.tflite");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  std::string file_contents = "visual model file";
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  std::string model_str = GetFlatBufferString(0);
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(model_str.length());
  memcpy(mapped_region.mapping.memory(), model_str.data(), model_str.length());
  ScorerStorage::GetInstance()->SetScorer(
      Scorer::Create(mapped_region.region.Duplicate(), std::move(file)));
  ASSERT_TRUE(classifier_->is_ready());

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, NoScorer) {
  // For this test, we'll create the delegate with no scorer available yet.
  ASSERT_FALSE(classifier_->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  GURL url("http://host.test");
  std::u16string page_text = u"dummy";
  LoadHTMLWithUrlOverride("dummy", url.spec().c_str());
  OnStartPhishingDetection(url);
  delegate_->PageCaptured(&page_text, false);

  GURL url2("http://host2.com");
  page_text = u"dummy";
  LoadHTMLWithUrlOverride("dummy", url2.spec().c_str());
  OnStartPhishingDetection(url2);
  delegate_->PageCaptured(&page_text, false);

  // Now set a scorer, which should cause a classifier to be created, and
  // classification will happen again because the scorer is set within timeout.
  page_text = u"dummy";
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  SetScorer(/*model_version=*/1);
  Mock::VerifyAndClearExpectations(classifier_);

  // Manually start a classification, so that when a new scorer is set, it
  // should cancel.
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  OnStartPhishingDetection(url2);

  // If we set a new scorer while a classification is going on the
  // classification should be cancelled.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  SetScorer(/*model_version=*/2);
  Mock::VerifyAndClearExpectations(classifier_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, NoScorer_Ref) {
  // Similar to the last test, but navigates within the page before
  // setting the scorer.
  ASSERT_FALSE(classifier_->is_ready());

  // Queue up a pending classification, cancel it, then queue up another one.
  GURL url("http://host.test");
  std::u16string page_text = u"dummy";
  LoadHTMLWithUrlOverride("dummy", url.spec().c_str());
  OnStartPhishingDetection(url);
  delegate_->PageCaptured(&page_text, false);

  page_text = u"dummy";
  OnStartPhishingDetection(url);
  delegate_->PageCaptured(&page_text, false);

  // Now set a scorer, which should cause a classifier to be created, and
  // classification will happen again because the scorer is set within timeout.
  page_text = u"dummy";
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  SetScorer(/*model_version=*/1);
  Mock::VerifyAndClearExpectations(classifier_);

  // Manually start a classification, so that when a new scorer is set, it
  // should cancel.
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  OnStartPhishingDetection(url);

  // If we set a new scorer while a classification is going on the
  // classification should be cancelled.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  SetScorer(/*model_version=*/2);
  Mock::VerifyAndClearExpectations(classifier_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, NoScorerWithinTimeout) {
  std::map<std::string, std::string> feature_params;
  feature_params["RetryTimeMax"] = "0";
  auto scoped_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_list->InitWithFeaturesAndParameters(
      {{safe_browsing::kClientSideDetectionRetryLimit, feature_params}}, {});
  // Similar to the last test, but the timeout delay is 0 seconds, so we expect
  // classifier not ready to occur, and setting the scorer will not retry the
  // classification.
  ASSERT_FALSE(classifier_->is_ready());
  EXPECT_FALSE(classifier_not_ready_);

  // Queue up a pending classification.
  GURL url("http://host.test");
  std::u16string page_text = u"dummy";
  LoadHTMLWithUrlOverride("dummy", url.spec().c_str());
  OnStartPhishingDetection(url);
  delegate_->PageCaptured(&page_text, false);

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(classifier_not_ready_);
}

TEST_F(PhishingClassifierDelegateTest, NoStartPhishingDetection) {
  // Tests the behavior when OnStartPhishingDetection has not yet been called
  // when the page load finishes.
  SetScorer(/*model_version=*/1);
  ASSERT_TRUE(classifier_->is_ready());

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url("http://host.test");
  LoadHTMLWithUrlOverride("<html><body>phish</body></html>",
                          url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  std::u16string page_text = u"phish";
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);
  // Now simulate the StartPhishingDetection IPC.  We expect classification
  // to begin.
  page_text = u"phish";
  EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
  OnStartPhishingDetection(url);
  Mock::VerifyAndClearExpectations(classifier_);

  // Now try again, but this time we will navigate the page away before
  // the IPC is sent.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url2("http://host2.com");
  LoadHTMLWithUrlOverride("<html><body>phish</body></html>",
                          url2.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  page_text = u"phish";
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url3("http://host3.com");
  LoadHTMLWithUrlOverride("<html><body>phish</body></html>",
                          url3.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);

  // In this test, the original page is a redirect, which we do not get a
  // StartPhishingDetection IPC for.  We simulate the redirection event to
  // load a new page while reusing the original session history entry, and
  // check that classification begins correctly for the landing page.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url4("http://host4.com");
  LoadHTMLWithUrlOverride("<html><body>phish</body></html>",
                          url4.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  page_text = u"abc";
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);
  EXPECT_CALL(*classifier_, CancelPendingClassification());

  GURL redir_url("http://host4.com/redir");
  LoadHTMLWithUrlOverride("123", redir_url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url4);
  page_text = u"123";
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    SimulateRedirection(redir_url);
    delegate_->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, IgnorePreliminaryCapture) {
  // Tests that preliminary PageCaptured notifications are ignored.
  SetScorer(/*model_version=*/1);
  ASSERT_TRUE(classifier_->is_ready());

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url("http://host.test");
  LoadHTMLWithUrlOverride("<html><body>phish</body></html>",
                          url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);
  std::u16string page_text = u"phish";
  delegate_->PageCaptured(&page_text, true);

  // Once the non-preliminary capture happens, classification should begin.
  page_text = u"phish";
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    delegate_->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, DuplicatePageCapture) {
  // Tests that a second PageCaptured notification causes classification to
  // be cancelled.
  SetScorer(/*model_version=*/1);
  ASSERT_TRUE(classifier_->is_ready());

  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url("http://host.test");
  LoadHTMLWithUrlOverride("<html><body>phish</body></html>",
                          url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  OnStartPhishingDetection(url);
  std::u16string page_text = u"phish";
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    delegate_->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  page_text = u"phish";
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  delegate_->PageCaptured(&page_text, false);
  Mock::VerifyAndClearExpectations(classifier_);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

TEST_F(PhishingClassifierDelegateTest, PhishingDetectionDone) {
  // Tests that a SafeBrowsingHostMsg_PhishingDetectionDone IPC is
  // sent to the browser whenever we finish classification.
  SetScorer(/*model_version=*/1);
  ASSERT_TRUE(classifier_->is_ready());

  // Start by loading a page to populate the delegate's state.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
  GURL url("http://host.test");
  LoadHTMLWithUrlOverride("<html><body>phish</body></html>",
                          url.spec().c_str());
  Mock::VerifyAndClearExpectations(classifier_);
  std::u16string page_text = u"phish";
  OnStartPhishingDetection(url);
  {
    InSequence s;
    EXPECT_CALL(*classifier_, CancelPendingClassification());
    EXPECT_CALL(*classifier_, BeginClassification(Pointee(page_text), _));
    delegate_->PageCaptured(&page_text, false);
    Mock::VerifyAndClearExpectations(classifier_);
  }

  // Now run the callback to simulate the classifier finishing.
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.8f);
  verdict.set_is_phishing(false);  // Send IPC even if site is not phishing.
  RunAndVerifyClassificationDone(verdict);

  // The delegate will cancel pending classification on destruction.
  EXPECT_CALL(*classifier_, CancelPendingClassification());
}

}  // namespace safe_browsing
