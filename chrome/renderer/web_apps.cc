// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/web_apps.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebString;

namespace web_apps {
namespace {

void AddInstallIcon(const WebElement& link,
                    std::vector<WebApplicationIconInfo>* icons) {
  WebString href = link.GetAttribute("href");
  if (href.IsNull() || href.IsEmpty())
    return;

  // Get complete url.
  GURL url = link.GetDocument().CompleteURL(href);
  if (!url.is_valid())
    return;

  WebApplicationIconInfo icon_info;
  if (link.HasAttribute("sizes")) {
    blink::WebVector<blink::WebSize> icon_sizes =
        blink::WebIconSizesParser::ParseIconSizes(link.GetAttribute("sizes"));
    if (icon_sizes.size() == 1 && icon_sizes[0].width != 0 &&
        icon_sizes[0].height == icon_sizes[0].width) {
      icon_info.square_size_px = icon_sizes[0].width;
    }
  }
  icon_info.url = url;
  icons->push_back(icon_info);
}

}  // namespace

void ParseWebAppFromWebDocument(WebLocalFrame* frame,
                                WebApplicationInfo* app_info) {
  WebDocument document = frame->GetDocument();
  if (document.IsNull())
    return;

  WebElement head = document.Head();
  if (head.IsNull())
    return;

  GURL document_url = document.Url();
  for (WebNode child = head.FirstChild(); !child.IsNull();
       child = child.NextSibling()) {
    if (!child.IsElementNode())
      continue;
    WebElement elem = child.To<WebElement>();

    if (elem.HasHTMLTagName("link")) {
      std::string rel = elem.GetAttribute("rel").Utf8();
      // "rel" attribute may use either "icon" or "shortcut icon".
      // see also
      //   <http://en.wikipedia.org/wiki/Favicon>
      //   <http://dev.w3.org/html5/spec/Overview.html#rel-icon>
      //
      // Bookmark apps also support "apple-touch-icon" and
      // "apple-touch-icon-precomposed".
      if (base::LowerCaseEqualsASCII(rel, "icon") ||
          base::LowerCaseEqualsASCII(rel, "shortcut icon") ||
          base::LowerCaseEqualsASCII(rel, "apple-touch-icon") ||
          base::LowerCaseEqualsASCII(rel, "apple-touch-icon-precomposed")) {
        AddInstallIcon(elem, &app_info->icon_infos);
      }
    } else if (elem.HasHTMLTagName("meta") && elem.HasAttribute("name")) {
      std::string name = elem.GetAttribute("name").Utf8();
      WebString content = elem.GetAttribute("content");
      if (name == "application-name") {
        app_info->title = content.Utf16();
      } else if (name == "description") {
        app_info->description = content.Utf16();
      } else if (name == "application-url") {
        std::string url = content.Utf8();
        app_info->start_url =
            document_url.is_valid() ? document_url.Resolve(url) : GURL(url);
        if (!app_info->start_url.is_valid())
          app_info->start_url = GURL();
      } else if (name == "mobile-web-app-capable" &&
                 base::LowerCaseEqualsASCII(content.Utf16(), "yes")) {
        app_info->mobile_capable = WebApplicationInfo::MOBILE_CAPABLE;
      } else if (name == "apple-mobile-web-app-capable" &&
                 base::LowerCaseEqualsASCII(content.Utf16(), "yes") &&
                 app_info->mobile_capable ==
                     WebApplicationInfo::MOBILE_CAPABLE_UNSPECIFIED) {
        app_info->mobile_capable = WebApplicationInfo::MOBILE_CAPABLE_APPLE;
      }
    }
  }
}

}  // namespace web_apps
