// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"

#include <memory>
#include <unordered_map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/mock_feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/test_utils.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/native_theme/native_theme_features.h"

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using blink::WebRuntimeFeatures;

namespace safe_browsing {

// TestPhishingDOMFeatureExtractor has nearly identical behavior as
// PhishingDOMFeatureExtractor, except the IsExternalDomain() and
// CompleteURL() functions. This is to work around the fact that
// ChromeRenderViewTest object does not know where the html content is hosted.
class TestPhishingDOMFeatureExtractor : public PhishingDOMFeatureExtractor {
 public:
  explicit TestPhishingDOMFeatureExtractor(FeatureExtractorClock* clock)
      : PhishingDOMFeatureExtractor(clock) {}

  void SetDocumentDomain(std::string domain) { base_domain_ = domain; }

  void SetURLToFrameDomainCheckingMap(
      const std::unordered_map<std::string, std::string>& checking_map) {
    url_to_frame_domain_map_ = checking_map;
  }

  void Reset() {
    base_domain_.clear();
    url_to_frame_domain_map_.clear();
  }

 private:
  // LoadHTML() function in RenderViewTest only loads html as data,
  // thus cur_frame_data_->domain is empty. Therefore, in base class
  // PhishingDOMFeatureExtractor::IsExternalDomain() will always return false.
  // Overriding IsExternalDomain(..) to work around this problem.
  bool IsExternalDomain(const GURL& url, std::string* domain) const override {
    DCHECK(domain);
    *domain = net::registry_controlled_domains::GetDomainAndRegistry(
        url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    if (domain->empty())
      *domain = url.spec();
    // If this html only has one frame, use base_domain_ to determine if url
    // is external.
    if (!base_domain_.empty()) {
      return !url.DomainIs(base_domain_);
    } else {
      // html contains multiple frames, need to check against
      // its corresponding frame's domain.
      auto it = url_to_frame_domain_map_.find(url.spec());
      if (it != url_to_frame_domain_map_.end()) {
        const std::string document_domain = it->second;
        return !url.DomainIs(document_domain);
      }
      NOTREACHED() << "Testing input setup is incorrect. "
                      "Please check url_to_frame_domain_map_ setup.";
      return true;
    }
  }

  // For similar reason as above, PhishingDOMFeatureExtractor::CompeteURL(..)
  // always returns empty WebURL. Overriding this CompeteURL(..) to work around
  // this issue.
  blink::WebURL CompleteURL(const blink::WebElement& element,
                            const blink::WebString& partial_url) override {
    GURL parsed_url = blink::WebStringToGURL(partial_url);
    GURL full_url;
    if (parsed_url.has_scheme()) {
      // This is already a complete URL.
      full_url = parsed_url;
    } else if (!base_domain_.empty()) {
      // This is a partial URL and only one frame in testing html.
      full_url = GURL("http://" + base_domain_).Resolve(partial_url.Utf8());
    } else {
      auto it = url_to_frame_domain_map_.find(partial_url.Utf8());
      if (it != url_to_frame_domain_map_.end()) {
        const std::string frame_domain = it->second;
        full_url = GURL("http://" + it->second).Resolve(partial_url.Utf8());
        url_to_frame_domain_map_[full_url.spec()] = it->second;
      } else {
        NOTREACHED() << "Testing input setup is incorrect. "
                        "Please check url_to_frame_domain_map_ setup.";
      }
    }
    return blink::WebURL(full_url);
  }

  // If there is only main frame, we use base_domain_ to track where
  // the html content is hosted.
  std::string base_domain_;

  // If html contains multiple frame/iframe, we track domain of each frame by
  // using this map, where keys are the urls mentioned in the html content,
  // values are the domains of the corresponding frames.
  std::unordered_map<std::string, std::string> url_to_frame_domain_map_;
};

class TestChromeContentRendererClient : public ChromeContentRendererClient {
 public:
  TestChromeContentRendererClient() {}
  ~TestChromeContentRendererClient() override {}
  // Since visited_link_slave_ in ChromeContentRenderClient never get initiated,
  // overrides VisitedLinkedHash() function to prevent crashing.
  uint64_t VisitedLinkHash(const char* canonical_url, size_t length) override {
    return 0;
  }
};

class PhishingDOMFeatureExtractorTest : public ChromeRenderViewTest {
 public:
  PhishingDOMFeatureExtractorTest()
      : success_(false), message_loop_(new content::MessageLoopRunner) {}

  bool GetSuccess() { return success_; }
  void ResetTest() {
    success_ = false;
    message_loop_ = new content::MessageLoopRunner;
    extractor_->Reset();
  }

  void ExtractFeaturesAcrossFrames(
      const std::string& html_content,
      FeatureMap* features,
      const std::unordered_map<std::string, std::string>&
          url_frame_domain_map) {
    extractor_->SetURLToFrameDomainCheckingMap(url_frame_domain_map);
    LoadHTML(html_content.c_str());

    extractor_->ExtractFeatures(
        GetMainFrame()->GetDocument(), features,
        base::BindOnce(&PhishingDOMFeatureExtractorTest::AnotherExtractionDone,
                       weak_factory_.GetWeakPtr()));
    message_loop_->Run();
  }

  void ExtractFeatures(const std::string& document_domain,
                       const std::string& html_content,
                       FeatureMap* features) {
    extractor_->SetDocumentDomain(document_domain);
    LoadHTML(html_content.c_str());

    extractor_->ExtractFeatures(
        GetMainFrame()->GetDocument(), features,
        base::BindOnce(&PhishingDOMFeatureExtractorTest::AnotherExtractionDone,
                       weak_factory_.GetWeakPtr()));
    message_loop_->Run();
  }

  // Helper for the SubframeRemoval test that posts a message to remove
  // the iframe "frame1" from the document.
  void ScheduleRemoveIframe() {
    GetMainFrame()
        ->GetTaskRunner(blink::TaskType::kInternalTest)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&PhishingDOMFeatureExtractorTest::RemoveIframe,
                           weak_factory_.GetWeakPtr()));
  }

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    WebRuntimeFeatures::EnableOverlayScrollbars(
        ui::IsOverlayScrollbarEnabled());
    extractor_.reset(new TestPhishingDOMFeatureExtractor(&clock_));
  }

  void TearDown() override {
    extractor_.reset(nullptr);
    ChromeRenderViewTest::TearDown();
  }

  content::ContentRendererClient* CreateContentRendererClient() override {
    ChromeContentRendererClient* client = new TestChromeContentRendererClient();
    InitChromeContentRendererClient(client);
    return client;
  }

  void AnotherExtractionDone(bool success) {
    success_ = success;
    message_loop_->QuitClosure().Run();
  }

  // Does the actual work of removing the iframe "frame1" from the document.
  void RemoveIframe() {
    blink::WebLocalFrame* main_frame = GetMainFrame();
    ASSERT_TRUE(main_frame);
    main_frame->ExecuteScript(blink::WebString(
        "document.body.removeChild(document.getElementById('frame1'));"));
  }

  MockFeatureExtractorClock clock_;
  bool success_;
  std::unique_ptr<TestPhishingDOMFeatureExtractor> extractor_;
  scoped_refptr<content::MessageLoopRunner> message_loop_;
  base::WeakPtrFactory<PhishingDOMFeatureExtractorTest> weak_factory_{this};
};

TEST_F(PhishingDOMFeatureExtractorTest, FormFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);
  expected_features.AddBooleanFeature(features::kPageHasCheckInputs);
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://cgi.host.com/submit"));
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://other.com/"));

  GURL url("http://host.com/query");
  expected_features.AddBooleanFeature(features::kPageActionURL + url.spec());

  FeatureMap features;
  EXPECT_FALSE(GetSuccess());

  ExtractFeatures(
      "host.com",
      "<html><head><body>"
      "<form action=\"query\"><input type=text><input type=checkbox></form>"
      "<form action=\"http://cgi.host.com/submit\"></form>"
      "<form action=\"http://other.com/\"></form>"
      "<form action=\"query\"></form>"
      "<form></form></body></html>",
      &features);
  EXPECT_TRUE(GetSuccess());
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasRadioInputs);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);

  features.Clear();
  ResetTest();
  EXPECT_FALSE(GetSuccess());
  ExtractFeatures("host.com",
                  "<html><head><body>"
                  "<input type=\"radio\"><input type=password></body></html>",
                  &features);
  EXPECT_TRUE(GetSuccess());
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);

  features.Clear();
  ResetTest();
  EXPECT_FALSE(GetSuccess());
  ExtractFeatures("host.com", "<html><head><body><input></body></html>",
                  &features);
  EXPECT_TRUE(GetSuccess());
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);

  features.Clear();
  ResetTest();
  EXPECT_FALSE(GetSuccess());
  ExtractFeatures("host.com",
                  "<html><head><body><input type=\"invalid\"></body></html>",
                  &features);
  EXPECT_TRUE(GetSuccess());
  ExpectFeatureMapsAreEqual(features, expected_features);
}

TEST_F(PhishingDOMFeatureExtractorTest, LinkFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  FeatureMap expected_features;
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.5);
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.0);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("chromium.org"));

  FeatureMap features;
  ExtractFeatures("host.com",
                  "<html><head><body>"
                  "<a href=\"http://www2.host.com/abc\">link</a>"
                  "<a name=page_anchor></a>"
                  "<a href=\"http://www.chromium.org/\">chromium</a>"
                  "</body></html>",
                  &features);
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.25);
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("chromium.org"));
  features.Clear();
  ResetTest();
  ExtractFeatures("host.com",
                  "<html><head><body>"
                  "<a href=\"login\">this is not secure</a>"
                  "<a href=\"http://host.com\">not secure</a>"
                  "<a href=\"http://chromium.org/\">also not secure</a>"
                  "<a href=\"https://www2.host.com/login\"> this secure</a>"
                  "</body></html>",
                  &features);
  ExpectFeatureMapsAreEqual(features, expected_features);
}

TEST_F(PhishingDOMFeatureExtractorTest, ScriptAndImageFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);

  FeatureMap features;
  ExtractFeatures(
      "host.com",
      "<html><head><script></script><script></script></head></html>",
      &features);
  ExpectFeatureMapsAreEqual(features, expected_features);

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTSix);
  expected_features.AddRealFeature(features::kPageImgOtherDomainFreq, 0.5);

  features.Clear();
  ResetTest();
  std::string html(
      "<html><head><script></script><script></script><script></script>"
      "<script></script><script></script><script></script><script></script>"
      "</head><body>"
      "<img src=\"file:///C:/other.png\">"
      "<img src=\"img/header.png\">"
      "</body></html>");
  ExtractFeatures("host.com", html, &features);
  ExpectFeatureMapsAreEqual(features, expected_features);
}

// A page with nested iframes.
//         html
// iframe2 /  \ iframe1
//              \ iframe3
TEST_F(PhishingDOMFeatureExtractorTest, SubFrames) {
  // This test doesn't exercise the extraction timing.
  // Test that features are aggregated across all frames.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  const char urlprefix[] = "data:text/html;charset=utf-8,";
  std::unordered_map<std::string, std::string> url_iframe_map;
  std::string iframe1_nested_html(
      "<html><body><input type=password>"
      "<a href=\"https://host3.com/submit\">link</a>"
      "<a href=\"relative\">link</a>"
      "</body></html>");
  GURL iframe1_nested_url(urlprefix + iframe1_nested_html);
  // iframe1_nested is on host1.com.
  url_iframe_map["https://host3.com/submit"] = "host1.com";
  url_iframe_map["relative"] = "host1.com";

  std::string iframe1_html(
      "<html><head><script></script><body>"
      "<form action=\"http://host3.com/home\"><input type=checkbox></form>"
      "<form action=\"http://host1.com/submit\"></form>"
      "<a href=\"http://www.host1.com/reset\">link</a>"
      "<iframe src=\"" +
      net::EscapeForHTML(iframe1_nested_url.spec()) +
      "\"></iframe></head></html>");
  GURL iframe1_url(urlprefix + iframe1_html);
  // iframe1 is on host1.com too.
  url_iframe_map["http://host3.com/home"] = "host1.com";
  url_iframe_map["http://host1.com/submit"] = "host1.com";
  url_iframe_map["http://www.host1.com/reset"] = "host1.com";

  std::string iframe2_html(
      "<html><head><script></script><body>"
      "<img src=\"file:///C:/other.html\">"
      "</body></html>");
  GURL iframe2_url(urlprefix + iframe2_html);
  // iframe2 is on host2.com
  url_iframe_map["file:///C:/other.html"] = "host2.com";

  std::string html(
      "<html><body><input type=text>"
      "<a href=\"info.html\">link</a>"
      "<iframe src=\"" +
      net::EscapeForHTML(iframe1_url.spec()) + "\"></iframe><iframe src=\"" +
      net::EscapeForHTML(iframe2_url.spec()) + "\"></iframe></body></html>");
  // The entire html is hosted on host.com
  url_iframe_map["info.html"] = "host.com";

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  // Form action domains are compared to the URL of the document they're in,
  // not the URL of the toplevel page.  So http://host1.com/ has two form
  // actions, one of which is external.
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);
  expected_features.AddBooleanFeature(features::kPageHasCheckInputs);
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("host3.com"));
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);
  expected_features.AddRealFeature(features::kPageImgOtherDomainFreq, 1.0);
  expected_features.AddBooleanFeature(features::kPageActionURL +
                                      std::string("http://host1.com/submit"));
  expected_features.AddBooleanFeature(features::kPageActionURL +
                                      std::string("http://host3.com/home"));

  FeatureMap features;
  ExtractFeaturesAcrossFrames(html, &features, url_iframe_map);
  ExpectFeatureMapsAreEqual(features, expected_features);
}

TEST_F(PhishingDOMFeatureExtractorTest, Continuation) {
  // For this test, we'll cause the feature extraction to run multiple
  // iterations by incrementing the clock.

  // This page has a total of 50 elements.  For the external forms feature to
  // be computed correctly, the extractor has to examine the whole document.
  // Note: the empty HEAD is important -- WebKit will synthesize a HEAD if
  // there isn't one present, which can be confusing for the element counts.
  std::string html =
      "<html><head></head><body>"
      "<form action=\"ondomain\"></form>";
  for (int i = 0; i < 45; ++i) {
    html.append("<p>");
  }
  html.append("<form action=\"http://host2.com/\"></form></body></html>");

  // Advance the clock 6 ms every 10 elements processed, 10 ms between chunks.
  // Note that this assumes kClockCheckGranularity = 10 and
  // kMaxTimePerChunkMs = 10.
  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(6)))
      // Time check after the next 10 elements.  This is over the chunk
      // time limit, so a continuation task will be posted.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(12)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(22)))
      // Time check after resuming iteration for the second chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(24)))
      // Time check after the next 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(30)))
      // Time check after the next 10 elements.  This will trigger another
      // continuation task.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(36)))
      // Time check at the start of the third chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(46)))
      // Time check after resuming iteration for the third chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(48)))
      // Time check after the last 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(54)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(56)));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageActionURL +
                                      std::string("http://host.com/ondomain"));
  expected_features.AddBooleanFeature(features::kPageActionURL +
      std::string("http://host2.com/"));

  FeatureMap features;
  ExtractFeatures("host.com", html, &features);
  ExpectFeatureMapsAreEqual(features, expected_features);
  // Make sure none of the mock expectations carry over to the next test.
  ::testing::Mock::VerifyAndClearExpectations(&clock_);

  // Now repeat the test with the same page, but advance the clock faster so
  // that the extraction time exceeds the maximum total time for the feature
  // extractor.  Extraction should fail.  Note that this assumes
  // kMaxTotalTimeMs = 500.
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 10 elements.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(300)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(350)))
      // Time check after resuming iteration for the second chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(360)))
      // Time check after the next 10 elements.  This is over the limit.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(600)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(620)));

  features.Clear();
  ResetTest();
  ExtractFeatures("host.com", html, &features);
  EXPECT_FALSE(GetSuccess());
}

TEST_F(PhishingDOMFeatureExtractorTest, SubframeRemoval) {
  // In this test, we'll advance the feature extractor so that it is positioned
  // inside an iframe, and have it pause due to exceeding the chunk time limit.
  // Then, prior to continuation, the iframe is removed from the document.
  // As currently implemented, this should finish extraction from the removed
  // iframe document.
  const char urlprefix[] = "data:text/html;charset=utf-8,";
  std::string iframe1_html(
      "<html><body><p><p><p><input type=password></body></html>");
  GURL iframe1_url(urlprefix + iframe1_html);

  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 10 elements.  Enough time has passed
      // to stop extraction.  Schedule the iframe removal to happen as soon as
      // the feature extractor returns control to the message loop.
      .WillOnce(DoAll(
          Invoke(this, &PhishingDOMFeatureExtractorTest::ScheduleRemoveIframe),
          Return(now + base::TimeDelta::FromMilliseconds(21))))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(25)))
      // Time check after resuming iteration for the second chunk.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(27)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(33)));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);

  FeatureMap features;
  std::string html(
      "<html><head></head><body>"
      "<iframe src=\"" +
      net::EscapeForHTML(iframe1_url.spec()) +
      "\" id=\"frame1\"></iframe>"
      "<form></form></body></html>");
  ExtractFeatures("host.com", html, &features);
  ExpectFeatureMapsAreEqual(features, expected_features);
}

}  // namespace safe_browsing
