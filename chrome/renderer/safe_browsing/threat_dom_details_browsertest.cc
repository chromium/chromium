// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/renderer/threat_dom_details.h"

#include <memory>
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing/features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/renderer/render_view.h"
#include "net/base/escape.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "ui/native_theme/native_theme_features.h"

namespace {

std::unique_ptr<base::test::ScopedFeatureList> SetupTagAndAttributeFeature() {
  std::map<std::string, std::string> feature_params;
  feature_params[std::string(safe_browsing::kTagAndAttributeParamName)] =
      "div,foo,div,baz,div,attr2,div,attr3,div,longattr4,div,attr5,div,attr6";
  std::unique_ptr<base::test::ScopedFeatureList> scoped_list(
      new base::test::ScopedFeatureList);
  scoped_list->InitWithFeaturesAndParameters(
      {{safe_browsing::kThreatDomDetailsTagAndAttributeFeature,
        feature_params}},
      {});
  return scoped_list;
}

}  // namespace

using ThreatDOMDetailsTest = ChromeRenderViewTest;

using testing::ElementsAre;

TEST_F(ThreatDOMDetailsTest, Everything) {
  blink::WebRuntimeFeatures::EnableOverlayScrollbars(
      ui::IsOverlayScrollbarEnabled());
  // Configure a field trial to collect divs with attribute foo.
  std::unique_ptr<base::test::ScopedFeatureList> feature_list =
      SetupTagAndAttributeFeature();
  std::unique_ptr<safe_browsing::ThreatDOMDetails> details(
      safe_browsing::ThreatDOMDetails::Create(view_->GetMainRenderFrame(),
                                              registry_.get()));
  // Lower kMaxNodes and kMaxAttributes for the test. Loading 500 subframes in a
  // debug build takes a while.
  safe_browsing::ThreatDOMDetails::kMaxNodes = 50;
  safe_browsing::ThreatDOMDetails::kMaxAttributes = 5;

  const char kUrlPrefix[] = "data:text/html;charset=utf-8,";
  {
    // A page with an internal script
    std::string html = "<html><head><script></script></head></html>";
    LoadHTML(html.c_str());
    base::HistogramTester histograms;
    std::vector<safe_browsing::mojom::ThreatDOMDetailsNodePtr> params;
    details->ExtractResources(&params);
    ASSERT_EQ(1u, params.size());
    auto* param = params[0].get();
    EXPECT_EQ(GURL(kUrlPrefix + net::EscapeQueryParamValue(html, false)),
              param->url);
    EXPECT_EQ(0, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
  }

  {
    // A page with 2 external scripts.
    // Note: This part of the test causes 2 leaks: LEAK: 5 WebCoreNode
    // LEAK: 2 CachedResource.
    GURL script1_url("data:text/javascript;charset=utf-8,var a=1;");
    GURL script2_url("data:text/javascript;charset=utf-8,var b=2;");
    std::string html = "<html><head><script src=\"" + script1_url.spec() +
                       "\"></script><script src=\"" + script2_url.spec() +
                       "\"></script></head></html>";
    GURL url(kUrlPrefix + net::EscapeQueryParamValue(html, false));

    LoadHTML(html.c_str());
    std::vector<safe_browsing::mojom::ThreatDOMDetailsNodePtr> params;
    details->ExtractResources(&params);
    ASSERT_EQ(3u, params.size());
    auto* param = params[0].get();
    EXPECT_EQ(script1_url, param->url);
    EXPECT_EQ("SCRIPT", param->tag_name);
    EXPECT_EQ(1, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());

    param = params[1].get();
    EXPECT_EQ(script2_url, param->url);
    EXPECT_EQ("SCRIPT", param->tag_name);
    EXPECT_EQ(2, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());

    param = params[2].get();
    EXPECT_EQ(url, param->url);
    EXPECT_EQ(0, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
  }

  {
    // A page with some divs containing an iframe which itself contains an
    // iframe. Tag "img foo" exists to ensure we honour both the tag name and
    // the attribute name when deciding which elements to collect.
    //  html
    //   \ div foo
    //    \ img foo, div bar
    //                \ div baz, iframe1
    //                            \ iframe2
    // Since ThreatDOMDetails is a RenderFrameObserver, it will only
    // extract resources from the frame it assigned to (in this case,
    // the main frame). Extracting resources from all frames within a
    // page is covered in SafeBrowsingBlockingPageBrowserTest.
    // In this example, ExtractResources() will still touch iframe1
    // since it is the direct child of the main frame, but it would not
    // go inside of iframe1.
    // We configure the test to collect divs with attribute foo and baz, but not
    // divs with attribute bar. So div foo will be collected and contain iframe1
    // and div baz as children.
    std::string iframe2_html = "<html><body>iframe2</body></html>";
    GURL iframe2_url(kUrlPrefix + iframe2_html);
    std::string iframe1_html = "<iframe src=\"" +
                               net::EscapeForHTML(iframe2_url.spec()) +
                               "\"></iframe>";
    GURL iframe1_url(kUrlPrefix + iframe1_html);
    std::string html =
        "<html><head><div foo=1 foo2=2><img foo=1><div bar=1><div baz=1></div>"
        "<iframe src=\"" +
        net::EscapeForHTML(iframe1_url.spec()) +
        "\"></iframe></div></div></head></html>";
    GURL url(kUrlPrefix + net::EscapeQueryParamValue(html, false));

    LoadHTML(html.c_str());
    std::vector<safe_browsing::mojom::ThreatDOMDetailsNodePtr> params;
    details->ExtractResources(&params);
    ASSERT_EQ(4u, params.size());

    auto* param = params[0].get();
    EXPECT_TRUE(param->url.is_empty());
    EXPECT_EQ(url, param->parent);
    EXPECT_EQ("DIV", param->tag_name);
    // The children field contains URLs, but this mapping is not currently
    // maintained among the interior nodes. The summary node is the parent of
    // all elements in the frame.
    EXPECT_TRUE(param->children.empty());
    EXPECT_EQ(1, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_THAT(param->child_node_ids, ElementsAre(2, 3));
    EXPECT_EQ(1u, param->attributes.size());
    EXPECT_EQ("foo", param->attributes[0]->name);
    EXPECT_EQ("1", param->attributes[0]->value);

    param = params[1].get();
    EXPECT_TRUE(param->url.is_empty());
    EXPECT_EQ(url, param->parent);
    EXPECT_EQ("DIV", param->tag_name);
    EXPECT_TRUE(param->children.empty());
    EXPECT_EQ(2, param->node_id);
    EXPECT_EQ(1, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
    EXPECT_EQ(1u, param->attributes.size());
    EXPECT_EQ("baz", param->attributes[0]->name);
    EXPECT_EQ("1", param->attributes[0]->value);

    param = params[2].get();
    EXPECT_EQ(iframe1_url, param->url);
    EXPECT_EQ(url, param->parent);
    EXPECT_EQ("IFRAME", param->tag_name);
    EXPECT_TRUE(param->children.empty());
    EXPECT_EQ(3, param->node_id);
    EXPECT_EQ(1, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
    EXPECT_TRUE(param->attributes.empty());

    param = params[3].get();
    EXPECT_EQ(url, param->url);
    EXPECT_EQ(GURL(), param->parent);
    EXPECT_THAT(param->children, ElementsAre(iframe1_url));
    EXPECT_EQ(0, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
  }

  {
    // Test >50 subframes.
    std::string html;
    for (int i = 0; i < 55; ++i) {
      // The iframe contents is just a number.
      GURL iframe_url(base::StringPrintf("%s%d", kUrlPrefix, i));
      html += "<iframe src=\"" + net::EscapeForHTML(iframe_url.spec()) +
              "\"></iframe>";
    }
    GURL url(kUrlPrefix + html);

    LoadHTML(html.c_str());
    base::HistogramTester histograms;
    std::vector<safe_browsing::mojom::ThreatDOMDetailsNodePtr> params;
    details->ExtractResources(&params);
    ASSERT_EQ(51u, params.size());

    // The element nodes should all have node IDs.
    for (size_t i = 0; i < params.size() - 1; ++i) {
      auto& param = *params[i];
      const int expected_id = i + 1;
      EXPECT_EQ(expected_id, param.node_id);
      EXPECT_EQ(0, param.parent_node_id);
      EXPECT_TRUE(param.child_node_ids.empty());
    }
  }

  {
    // A page with >50 scripts, to verify kMaxNodes.
    std::string html;
    for (int i = 0; i < 55; ++i) {
      // The iframe contents is just a number.
      GURL script_url(base::StringPrintf("%s%d", kUrlPrefix, i));
      html += "<script src=\"" + net::EscapeForHTML(script_url.spec()) +
              "\"></script>";
    }
    GURL url(kUrlPrefix + html);

    LoadHTML(html.c_str());
    base::HistogramTester histograms;
    std::vector<safe_browsing::mojom::ThreatDOMDetailsNodePtr> params;
    details->ExtractResources(&params);
    ASSERT_EQ(51u, params.size());

    // The element nodes should all have node IDs.
    for (size_t i = 0; i < params.size() - 1; ++i) {
      auto& param = *params[i];
      const int expected_id = i + 1;
      EXPECT_EQ(expected_id, param.node_id);
      EXPECT_EQ(0, param.parent_node_id);
      EXPECT_TRUE(param.child_node_ids.empty());
    }
  }

  {
    // Check the limit on the number of attributes collected and their lengths.
    safe_browsing::ThreatDOMDetails::kMaxAttributeStringLength = 5;
    std::string html =
        "<html><head><div foo=1 attr2=2 attr3=3 longattr4=4 attr5=longvalue5 "
        "attr6=6></div></head></html>";
    LoadHTML(html.c_str());
    std::vector<safe_browsing::mojom::ThreatDOMDetailsNodePtr> params;
    details->ExtractResources(&params);
    GURL url = GURL(kUrlPrefix + net::EscapeQueryParamValue(html, false));
    ASSERT_EQ(2u, params.size());
    auto* param = params[0].get();
    EXPECT_TRUE(param->url.is_empty());
    EXPECT_EQ(url, param->parent);
    EXPECT_EQ("DIV", param->tag_name);
    EXPECT_TRUE(param->children.empty());
    EXPECT_EQ(1, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
    EXPECT_EQ(5u, param->attributes.size());
    EXPECT_EQ("foo", param->attributes[0]->name);
    EXPECT_EQ("1", param->attributes[0]->value);
    EXPECT_EQ("attr2", param->attributes[1]->name);
    EXPECT_EQ("2", param->attributes[1]->value);
    EXPECT_EQ("attr3", param->attributes[2]->name);
    EXPECT_EQ("3", param->attributes[2]->value);
    EXPECT_EQ("longattr4", param->attributes[3]->name);
    EXPECT_EQ("4", param->attributes[3]->value);
    EXPECT_EQ("attr5", param->attributes[4]->name);
    EXPECT_EQ("lo...", param->attributes[4]->value);
    param = params[1].get();
    EXPECT_EQ(url, param->url);
    EXPECT_EQ(0, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
  }
}

TEST_F(ThreatDOMDetailsTest, DefaultTagAndAttributesList) {
  // Verify that the default tag and attribute list is initialized and used
  // when the Finch feature (ThreatDomDetailsTagAttributes) is disabled.
  blink::WebRuntimeFeatures::EnableOverlayScrollbars(
      ui::IsOverlayScrollbarEnabled());
  std::unique_ptr<base::test::ScopedFeatureList> feature_list(
      new base::test::ScopedFeatureList);
  feature_list->InitAndDisableFeature(
      safe_browsing::kThreatDomDetailsTagAndAttributeFeature);
  std::unique_ptr<safe_browsing::ThreatDOMDetails> details(
      safe_browsing::ThreatDOMDetails::Create(view_->GetMainRenderFrame(),
                                              registry_.get()));
  const char kUrlPrefix[] = "data:text/html;charset=utf-8,";

  // A page with some divs containing an iframe which itself contains an
  // iframe. Tag "img foo" exists to ensure we honour both the tag name and
  // the attribute name when deciding which elements to collect.
  //  html
  //   \ div[data-google-query-id=foo]
  //    \ div[id=bar]
  //     \ iframe[id=baz]
  //      \ iframe2
  // Since ThreatDOMDetails is a RenderFrameObserver, it will only
  // extract resources from the frame it assigned to (in this case,
  // the main frame). Extracting resources from all frames within a
  // page is covered in SafeBrowsingBlockingPageBrowserTest.
  // In this example, ExtractResources() will still touch iframe[baz]
  // since it is the direct child of the main frame, but it would not
  // go inside of the iframe.
  std::string iframe2_html = "<html><body>iframe2</body></html>";
  GURL iframe2_url(kUrlPrefix + iframe2_html);
  std::string html =
      "<html><head><div data-google-query-id=foo><div id=bar>"
      "<iframe id=baz><iframe src=\"" +
      net::EscapeForHTML(iframe2_url.spec()) +
      "\"></iframe></iframe></div></div></head></html>";
  GURL url(kUrlPrefix + net::EscapeQueryParamValue(html, false));

  LoadHTML(html.c_str());
  std::vector<safe_browsing::mojom::ThreatDOMDetailsNodePtr> params;
  details->ExtractResources(&params);
  ASSERT_EQ(4u, params.size());

  auto* param = params[0].get();
  EXPECT_TRUE(param->url.is_empty());
  EXPECT_EQ(url, param->parent);
  EXPECT_EQ("DIV", param->tag_name);
  // The children field contains URLs, but this mapping is not currently
  // maintained among the interior nodes. The summary node (last in the list) is
  // the parent of all elements in the frame.
  EXPECT_TRUE(param->children.empty());
  EXPECT_EQ(1, param->node_id);
  EXPECT_EQ(0, param->parent_node_id);
  EXPECT_THAT(param->child_node_ids, ElementsAre(2));
  EXPECT_EQ(1u, param->attributes.size());
  EXPECT_EQ("data-google-query-id", param->attributes[0]->name);
  EXPECT_EQ("foo", param->attributes[0]->value);

  param = params[1].get();
  EXPECT_EQ("id", param->attributes[0]->name);
  EXPECT_EQ("bar", param->attributes[0]->value);
  EXPECT_TRUE(param->url.is_empty());
  EXPECT_EQ(url, param->parent);
  EXPECT_EQ("DIV", param->tag_name);
  EXPECT_TRUE(param->children.empty());
  EXPECT_EQ(2, param->node_id);
  EXPECT_EQ(1, param->parent_node_id);
  EXPECT_THAT(param->child_node_ids, ElementsAre(3));
  EXPECT_EQ(1u, param->attributes.size());
  EXPECT_EQ("id", param->attributes[0]->name);
  EXPECT_EQ("bar", param->attributes[0]->value);

  param = params[2].get();
  EXPECT_TRUE(param->url.is_empty());
  EXPECT_EQ(url, param->parent);
  EXPECT_EQ("IFRAME", param->tag_name);
  EXPECT_TRUE(param->children.empty());
  EXPECT_EQ(3, param->node_id);
  EXPECT_EQ(2, param->parent_node_id);
  EXPECT_TRUE(param->child_node_ids.empty());
  EXPECT_EQ(1u, param->attributes.size());
  EXPECT_EQ("id", param->attributes[0]->name);
  EXPECT_EQ("baz", param->attributes[0]->value);

  param = params[3].get();
  EXPECT_EQ(url, param->url);
  EXPECT_EQ(GURL(), param->parent);
  EXPECT_TRUE(param->children.empty());
  EXPECT_EQ(0, param->node_id);
  EXPECT_EQ(0, param->parent_node_id);
  EXPECT_TRUE(param->child_node_ids.empty());
}

TEST_F(ThreatDOMDetailsTest, CheckTagAndAttributeListIsSorted) {
  std::unique_ptr<base::test::ScopedFeatureList> scoped_list(
      new base::test::ScopedFeatureList);
  scoped_list->InitAndEnableFeature(
      safe_browsing::kCaptureInlineJavascriptForGoogleAds);

  std::unique_ptr<safe_browsing::ThreatDOMDetails> details(
      safe_browsing::ThreatDOMDetails::Create(view_->GetMainRenderFrame(),
                                              registry_.get()));
  std::vector<safe_browsing::TagAndAttributesItem> tag_and_attr_list =
      details->GetTagAndAttributesListForTest();
  bool is_sorted;
  std::vector<std::string> tag_names;
  for (auto item : tag_and_attr_list) {
    tag_names.push_back(item.tag_name);
    // Check that list of attributes is sorted.
    is_sorted = std::is_sorted(item.attributes.begin(), item.attributes.end());
    EXPECT_TRUE(is_sorted);
  }
  // Check that the tags are sorted.
  is_sorted = std::is_sorted(tag_names.begin(), tag_names.end());
  EXPECT_TRUE(is_sorted);
}

TEST_F(ThreatDOMDetailsTest, CaptureInnerHtmlContent) {
  std::unique_ptr<base::test::ScopedFeatureList> scoped_list(
      new base::test::ScopedFeatureList);
  scoped_list->InitAndEnableFeature(
      safe_browsing::kCaptureInlineJavascriptForGoogleAds);
  std::unique_ptr<safe_browsing::ThreatDOMDetails> details(
      safe_browsing::ThreatDOMDetails::Create(view_->GetMainRenderFrame(),
                                              registry_.get()));

  const char kUrlPrefix[] = "data:text/html;charset=utf-8,";
  {
    // A page with a html element without an onclick element, html element with
    // an onclick element, an internal script. Html elements without onclick
    // elements should not be recorded in ThreatDomDetails.
    std::string html =
        "<html><head><a></a><a onclick=\"var y = 2;\"></a><img onclick=\"var z "
        "= 3;\"></img><script>var x = 1;</script></head></html>";
    LoadHTML(html.c_str());
    base::HistogramTester histograms;
    std::vector<safe_browsing::mojom::ThreatDOMDetailsNodePtr> params;

    details->ExtractResources(&params);
    ASSERT_EQ(4u, params.size());
    auto* param = params[0].get();
    EXPECT_EQ(GURL(), param->url);
    EXPECT_EQ("A", param->tag_name);
    EXPECT_EQ(1, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
    EXPECT_EQ(1u, param->attributes.size());
    EXPECT_EQ("onclick", param->attributes[0]->name);
    EXPECT_EQ("var y = 2;", param->attributes[0]->value);

    param = params[1].get();
    EXPECT_EQ(GURL(), param->url);
    EXPECT_EQ("IMG", param->tag_name);
    EXPECT_EQ(2, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
    EXPECT_TRUE(param->child_node_ids.empty());
    EXPECT_EQ(1u, param->attributes.size());
    EXPECT_EQ("onclick", param->attributes[0]->name);
    EXPECT_EQ("var z = 3;", param->attributes[0]->value);

    param = params[2].get();
    EXPECT_EQ(GURL(), param->url);
    EXPECT_EQ("SCRIPT", param->tag_name);
    EXPECT_EQ(3, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
    EXPECT_EQ("var x = 1;", param->inner_html);

    param = params[3].get();
    EXPECT_EQ(GURL(kUrlPrefix + net::EscapeQueryParamValue(html, false)),
              param->url);
    EXPECT_EQ(0, param->node_id);
    EXPECT_EQ(0, param->parent_node_id);
    EXPECT_TRUE(param->child_node_ids.empty());
  }
}
