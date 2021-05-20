// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/renderer/rss_link_reader.h"

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
// url is contained.
GURL GetRssUrlFromLinkElement(const blink::WebDocument& document,
                              const blink::WebElement& link_element) {
  GURL result;
  if (link_element.HasAttribute("type") && link_element.HasAttribute("href")) {
    blink::WebString type = link_element.GetAttribute("type");
    if (type.Equals("application/rss+xml") ||
        type.Equals("application/rss+atom") ||
        type.Equals("application/atom+xml")) {
      blink::WebURL url =
          document.CompleteURL(link_element.GetAttribute("href"));
      if (url.IsValid())
        result = url;
    }
  }
  return result;
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
  std::vector<GURL> rss_urls;
  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  const blink::WebElement head = document.Head();
  blink::WebElementCollection link_iter = head.GetElementsByHTMLTagName("link");
  for (blink::WebElement element = link_iter.FirstItem();
       !element.IsNull() && rss_urls.size() < kMaxRssUrls;
       element = link_iter.NextItem()) {
    GURL url = GetRssUrlFromLinkElement(document, element);
    if (url.is_valid())
      rss_urls.push_back(url);
  }
  std::move(callback).Run(
      mojom::RssLinks::New(document.Url(), std::move(rss_urls)));
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
