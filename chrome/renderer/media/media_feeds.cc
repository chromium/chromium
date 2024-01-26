// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/media_feeds.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "url/gurl.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebNode;
using blink::WebString;

std::optional<GURL> MediaFeeds::GetMediaFeedURL(content::RenderFrame* frame) {
  // Media Feeds are only discovered on the main frame.
  if (!frame->IsMainFrame())
    return std::nullopt;

  WebDocument document = frame->GetWebFrame()->GetDocument();
  if (document.IsNull())
    return std::nullopt;

  WebElement head = document.Head();
  if (head.IsNull())
    return std::nullopt;

  url::Origin document_origin = document.GetSecurityOrigin();

  // Look for a <link> element in the <head> of the document.
  for (WebNode child = head.FirstChild(); !child.IsNull();
       child = child.NextSibling()) {
    if (!child.IsElementNode())
      continue;

    WebElement elem = child.To<WebElement>();
    if (!elem.HasHTMLTagName("link"))
      continue;

    // The <link> rel must be media-feed.
    std::string rel = elem.GetAttribute("rel").Utf8();
    if (!base::EqualsCaseInsensitiveASCII(rel, "media-feed"))
      continue;

    WebString href = elem.GetAttribute("href");
    if (href.IsNull() || href.IsEmpty())
      continue;

    // If the URL is not valid then we should throw an error.
    GURL url = document.CompleteURL(href);
    if (!url.is_valid()) {
      frame->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                                 "The Media Feed URL is not a valid URL.");

      return std::nullopt;
    }

    // If the URL is not the same origin as the document then we should throw
    // and error.
    if (!document_origin.IsSameOriginWith(url)) {
      frame->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                                 "The Media Feed URL needs to be the same "
                                 "origin as the document URL.");

      return std::nullopt;
    }

    return url;
  }

  return std::nullopt;
}
