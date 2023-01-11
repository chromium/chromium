// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/renderer/search_result_extractor_impl.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/continuous_search/common/title_validator.h"
#include "components/continuous_search/renderer/config.h"
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

    std::u16string title;
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
    result->title = ValidateTitle(title);
    group->results.push_back(std::move(result));

    had_results = true;
  }

  return had_results;
}

bool ExtractResults(blink::WebDocument document,
                    mojom::CategoryResultsPtr& results) {
  blink::WebElement cards = document.GetElementById("rso");
  if (cards.IsNull()) {
    return false;
  }

  auto group = mojom::ResultGroup::New();
  group->type = mojom::ResultType::kSearchResults;
  if (!ExtractResultCards(cards, group)) {
    return false;
  }

  results->category_type = mojom::Category::kOrganic;
  results->groups.push_back(std::move(group));
  return true;
}

bool ExtractRelatedSearches(blink::WebDocument document,
                            mojom::CategoryResultsPtr& results) {
  auto group = mojom::ResultGroup::New();
  group->type = mojom::ResultType::kRelatedSearches;

  blink::WebElement related_searches_container = document.GetElementById(
      blink::WebString::FromUTF8(GetConfig().related_searches_id));
  if (related_searches_container.IsNull()) {
    return false;
  }

  blink::WebElementCollection anchors =
      related_searches_container.GetElementsByHTMLTagName("a");
  if (anchors.IsNull()) {
    return false;
  }

  // Loop through the anchors that are children of the related searches div,
  // matching against the platform-appropriate classname, and extract the urls
  // and titles.
  for (blink::WebElement anchor = anchors.FirstItem(); !anchor.IsNull();
       anchor = anchors.NextItem()) {
    if (!anchor.HasAttribute("class") ||
        !base::Contains(anchor.GetAttribute("class").Utf8(),
                        GetConfig().related_searches_anchor_classname)) {
      continue;
    }

    blink::WebString string_url = anchor.GetAttribute("href");
    if (string_url.IsNull() || string_url.IsEmpty()) {
      continue;
    }

    GURL url = GURL(document.CompleteURL(string_url));
    if (!url.is_valid() || url.is_empty() || !url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    std::u16string title;
    blink::WebElementCollection inner_divs =
        anchor.GetElementsByHTMLTagName("div");
    if (inner_divs.IsNull()) {
      continue;
    }
    for (blink::WebElement inner_div = inner_divs.FirstItem();
         !inner_div.IsNull(); inner_div = inner_divs.NextItem()) {
      if (!inner_div.HasAttribute("class") ||
          !base::Contains(inner_div.GetAttribute("class").Utf8(),
                          GetConfig().related_searches_title_classname)) {
        continue;
      }
      title = inner_div.TextContent().Utf16();
      break;
    }

    title = base::CollapseWhitespace(title, true);
    if (title.empty()) {
      continue;
    }

    auto result = mojom::SearchResult::New();
    result->link = url;
    result->title = ValidateTitle(title);
    group->results.push_back(std::move(result));
  }

  if (group->results.empty()) {
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
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::SearchResultExtractor>(base::BindRepeating(
          &SearchResultExtractorImpl::BindSearchResultExtractor,
          weak_ptr_factory_.GetWeakPtr()));
}

SearchResultExtractorImpl::~SearchResultExtractorImpl() = default;

void SearchResultExtractorImpl::ExtractCurrentSearchResults(
    const std::vector<mojom::ResultType>& result_types,
    ExtractCurrentSearchResultsCallback callback) {
  auto category_result = mojom::CategoryResults::New();

  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  category_result->document_url = GURL(document.Url());

  for (const auto& result_type : result_types) {
    switch (result_type) {
      case mojom::ResultType::kSearchResults:
        if (!ExtractResults(document, category_result)) {
          // Extracting search results is a requirement, if requested.
          std::move(callback).Run(
              mojom::SearchResultExtractor::Status::kNoResults,
              std::move(category_result));
          return;
        }
        break;
      case mojom::ResultType::kRelatedSearches:
        if (!ExtractRelatedSearches(document, category_result)) {
          // Extracting related searches is a requirement, if requested.
          std::move(callback).Run(
              mojom::SearchResultExtractor::Status::kNoResults,
              std::move(category_result));
          return;
        }
        break;
    }
  }

  std::move(callback).Run(mojom::SearchResultExtractor::Status::kSuccess,
                          std::move(category_result));
}

void SearchResultExtractorImpl::OnDestruct() {
  receiver_.reset();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void SearchResultExtractorImpl::BindSearchResultExtractor(
    mojo::PendingAssociatedReceiver<mojom::SearchResultExtractor> receiver) {
  // Requests can occur multiple times on the same frame. If the browser has
  // released its endpoint and creates a new one this needs to be reset.
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace continuous_search
