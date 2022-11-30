// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/renderer/rss_link_reader.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace feed {
namespace {

// Maximum number of RSS URLs to read.
constexpr int kMaxRssUrls = 50;

// Returns the RSS URL embedded in `link_element`, or any empty GURL if no RSS
// url is contained. This code is based on this internal implementation:
// http://shortn/_PIHj2svUjF
GURL GetRssUrlFromLinkElement(const blink::WebElement& link_element) {
  if (!(link_element.HasAttribute("type") && link_element.HasAttribute("rel") &&
        link_element.HasAttribute("href"))) {
    return GURL();
  }
  blink::WebString web_type = link_element.GetAttribute("type");
  blink::WebString web_rel = link_element.GetAttribute("rel");
  if (!web_type.ContainsOnlyASCII() || !web_rel.ContainsOnlyASCII())
    return GURL();
  std::string type = web_type.Ascii();
  if (!(base::EqualsCaseInsensitiveASCII(type, "application/rss+xml") ||
        base::EqualsCaseInsensitiveASCII(type, "application/rss+atom") ||
        base::EqualsCaseInsensitiveASCII(type, "application/atom+xml"))) {
    return GURL();
  }
  std::string rel = web_rel.Ascii();
  if (!(base::EqualsCaseInsensitiveASCII(rel, "alternate") ||
        base::EqualsCaseInsensitiveASCII(rel, "service.feed"))) {
    return GURL();
  }
  blink::WebURL url =
      link_element.GetDocument().CompleteURL(link_element.GetAttribute("href"));
  return url.IsValid() ? GURL(url) : GURL();
}

void AddRssLinksFromElement(const blink::WebElement& element,
                            std::vector<GURL>* rss_urls) {
  if (element.IsNull())
    return;
  blink::WebElementCollection link_iter =
      element.GetElementsByHTMLTagName("link");
  if (link_iter.IsNull())
    return;
  for (blink::WebElement link = link_iter.FirstItem();
       !link.IsNull() && rss_urls->size() < kMaxRssUrls;
       link = link_iter.NextItem()) {
    GURL url = GetRssUrlFromLinkElement(link);
    if (url.is_valid())
      rss_urls->push_back(url);
  }
}

std::vector<GURL> GetRssLinksFromDocument(blink::WebDocument document) {
  std::vector<GURL> rss_urls;
  if (document.IsNull())
    return rss_urls;
  AddRssLinksFromElement(document.Head(), &rss_urls);
  // If head contained no RSS feed links, fallback to scanning the body.
  if (rss_urls.empty())
    AddRssLinksFromElement(document.Body(), &rss_urls);
  return rss_urls;
}

}  // namespace

RssLinkReader::RssLinkReader(content::RenderFrame* render_frame,
                             service_manager::BinderRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  // Being a RenderFrameObserver, this object is scoped to the RenderFrame.
  // Unretained is safe here because `registry` is also scoped to the
  // RenderFrame.
  registry->AddInterface(base::BindRepeating(&RssLinkReader::BindReceiver,
                                             base::Unretained(this)));
}

RssLinkReader::~RssLinkReader() = default;

void RssLinkReader::GetRssLinks(GetRssLinksCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  std::move(callback).Run(
      mojom::RssLinks::New(document.Url(), GetRssLinksFromDocument(document)));
  base::UmaHistogramMicrosecondsTimes(
      "ContentSuggestions.Feed.WebFeed.GetRssLinksRendererTime",
      base::TimeTicks::Now() - start_time);
}

void RssLinkReader::OnDestruct() {
  delete this;
}

void RssLinkReader::BindReceiver(
    mojo::PendingReceiver<mojom::RssLinkReader> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace feed
