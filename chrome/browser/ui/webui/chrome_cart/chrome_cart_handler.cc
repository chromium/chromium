// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_cart/chrome_cart_handler.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

using bookmarks::BookmarkNode;

ChromeCartHandler::ChromeCartHandler(
    mojo::PendingReceiver<chrome_cart::mojom::ChromeCartHandler> handler,
    Profile* profile)
    : handler_(this, std::move(handler)) {
  bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile);
}

ChromeCartHandler::~ChromeCartHandler() = default;

double getTimeStampForBookmark(const BookmarkNode* bn) {
  std::string time_string;
  bn->GetMetaInfo("time_stamp", &time_string);
  double time_double;
  base::StringToDouble(std::move(time_string), &time_double);
  return time_double;
}

bool compareTimeStamp(const BookmarkNode* bn1, const BookmarkNode* bn2) {
  return getTimeStampForBookmark(bn1) > getTimeStampForBookmark(bn2);
}

void ChromeCartHandler::GetData(GetDataCallback callback) {
  std::vector<const BookmarkNode*> nodes;
  bookmark_model_->GetChromeCartNodes(nodes);
  std::sort(nodes.begin(), nodes.end(), compareTimeStamp);
  std::vector<chrome_cart::mojom::ChromeCartDataItemPtr> data;
  for (const BookmarkNode* bookmark_node : nodes) {
    auto data_item = chrome_cart::mojom::ChromeCartDataItem::New();
    data_item->merchant =
        base::UTF16ToUTF8(std::move(bookmark_node->GetTitledUrlNodeTitle()));
    data_item->cart_url = bookmark_node->url().spec();
    std::vector<std::string> image_urls;
    int image_count_int;
    std::string image_count_str;
    bookmark_node->GetMetaInfo("image_count", &image_count_str);
    base::StringToInt(image_count_str, &image_count_int);
    for (int i = 0; i < image_count_int; i++) {
      std::string image_url;
      bookmark_node->GetMetaInfo("image_url_" + base::NumberToString(i),
                                 &image_url);
      if (image_url.size() != 0) {
        image_urls.push_back(std::move(image_url));
      }
    }
    data_item->image_urls = image_urls;
    data.push_back(std::move(data_item));
  }
  std::move(callback).Run(std::move(data));
}

void ChromeCartHandler::ShouldShowModule(ShouldShowModuleCallback callback) {
  std::vector<const BookmarkNode*> nodes;
  bookmark_model_->GetChromeCartNodes(nodes);
  std::move(callback).Run(nodes.size() != 0);
}
