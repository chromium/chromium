// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/renderer/search_result_extractor_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

namespace continuous_search {

namespace {

// Extracts results from cards.
bool ExtractResultCards(blink::WebElement node, mojom::ResultGroupPtr& group) {
  if (node.IsNull()) {
    return false;
  }
  bool had_results = false;

  blink::WebElementCollection divs = node.GetElementsByHTMLTagName("div");
  if (divs.IsNull()) {
    return false;
  }

  // Find the first anchor after class="mnr-c" then extract the url/title.
  for (blink::WebElement div = divs.FirstItem(); !div.IsNull();
       div = divs.NextItem()) {
    if (!div.HasAttribute("class")) {
      continue;
    }
    blink::WebString clazz = div.GetAttribute("class");
    if (!base::StartsWith(clazz.Utf8(), "mnr-c")) {
      continue;
    }

    blink::WebElement link_anchor;
    {
      blink::WebElementCollection anchors = div.GetElementsByHTMLTagName("a");
      if (anchors.IsNull()) {
        continue;
      }
      link_anchor = anchors.FirstItem();
    }
    if (link_anchor.IsNull() || !link_anchor.HasAttribute("href")) {
      continue;
    }
    blink::WebString string_url = link_anchor.GetAttribute("href");
    if (string_url.IsNull() || string_url.IsEmpty()) {
      continue;
    }

    GURL url = GURL(string_url.Utf8());
    if (!url.is_valid() || url.is_empty() || !url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    base::string16 title;
    blink::WebElementCollection inner_divs =
        link_anchor.GetElementsByHTMLTagName("div");
    if (inner_divs.IsNull()) {
      continue;
    }
    for (blink::WebElement inner_div = inner_divs.FirstItem();
         !inner_div.IsNull(); inner_div = inner_divs.NextItem()) {
      if (!inner_div.HasAttribute("role") ||
          inner_div.GetAttribute("role").Utf8() != "heading") {
        continue;
      }
      title = inner_div.TextContent().Utf16();
      break;
    }

    if (title.empty()) {
      continue;
    }
    auto result = mojom::SearchResult::New();
    result->link = url;
    base::string16 trimmed_title;
    base::TrimWhitespace(title, base::TRIM_ALL, &trimmed_title);
    result->title = base::UTF16ToUTF8(trimmed_title);
    group->results.push_back(std::move(result));

    had_results = true;
  }

  return had_results;
}

bool ExtractAds(blink::WebDocument document,
                mojom::CategoryResultsPtr& results) {
  // Top ads ignoring carousels.
  blink::WebElement ads = document.GetElementById("tads");
  if (ads.IsNull()) {
    return false;
  }

  auto group = mojom::ResultGroup::New();
  group->is_ad_group = true;
  group->label = "Ads";
  if (!ExtractResultCards(ads, group)) {
    return false;
  }

  results->groups.push_back(std::move(group));
  return true;
}

bool ExtractResults(blink::WebDocument document,
                    mojom::CategoryResultsPtr& results) {
  blink::WebElement cards = document.GetElementById("rso");
  if (cards.IsNull()) {
    return false;
  }

  auto group = mojom::ResultGroup::New();
  group->is_ad_group = false;
  group->label = "Search Results";
  if (!ExtractResultCards(cards, group)) {
    return false;
  }

  results->category_type = mojom::Category::kOrganic;
  results->groups.push_back(std::move(group));
  return true;
}

}  // namespace

// static
SearchResultExtractorImpl* SearchResultExtractorImpl::Create(
    content::RenderFrame* render_frame) {
  DCHECK(render_frame->IsMainFrame());
  return new SearchResultExtractorImpl(render_frame);
}

SearchResultExtractorImpl::SearchResultExtractorImpl(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&SearchResultExtractorImpl::BindSearchResultExtractor,
                          weak_ptr_factory_.GetWeakPtr()));
}

SearchResultExtractorImpl::~SearchResultExtractorImpl() = default;

void SearchResultExtractorImpl::ExtractCurrentSearchResults(
    ExtractCurrentSearchResultsCallback callback) {
  auto category_result = mojom::CategoryResults::New();

  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  category_result->document_url = GURL(document.Url());

  ExtractAds(document, category_result);
  if (!ExtractResults(document, category_result)) {
    std::move(callback).Run(mojom::SearchResultExtractor::Status::kNoResults,
                            std::move(category_result));
    return;
  }

  std::move(callback).Run(mojom::SearchResultExtractor::Status::kSuccess,
                          std::move(category_result));
}

void SearchResultExtractorImpl::OnDestruct() {
  receiver_.reset();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void SearchResultExtractorImpl::BindSearchResultExtractor(
    mojo::PendingAssociatedReceiver<mojom::SearchResultExtractor> receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace continuous_search
