// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/shared_highlighting/ios/parsing_utils.h"

#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

namespace {
const CGFloat kCaretWidth = 4.0;
}  // namespace

namespace shared_highlighting {

BOOL IsValidDictValue(const base::Value* value) {
  return value && value->is_dict() && !value->GetDict().empty();
}

std::optional<CGRect> ParseRect(const base::Value::Dict* dict) {
  if (!dict || dict->empty()) {
    return std::nullopt;
  }

  std::optional<double> xValue = dict->FindDouble("x");
  std::optional<double> yValue = dict->FindDouble("y");
  std::optional<double> widthValue = dict->FindDouble("width");
  std::optional<double> heightValue = dict->FindDouble("height");

  if (!xValue || !yValue || !widthValue || !heightValue) {
    return std::nullopt;
  }

  return CGRectMake(*xValue, *yValue, *widthValue, *heightValue);
}

std::optional<GURL> ParseURL(const std::string* url_value) {
  if (!url_value) {
    return std::nullopt;
  }

  GURL url(*url_value);
  if (!url.is_empty() && url.is_valid()) {
    return url;
  }

  return std::nullopt;
}

CGRect ConvertToBrowserRect(CGRect web_view_rect, web::WebState* web_state) {
  if (CGRectEqualToRect(web_view_rect, CGRectZero) || !web_state) {
    return web_view_rect;
  }

  id<CRWWebViewProxy> web_view_proxy = web_state->GetWebViewProxy();
  CGFloat zoom_scale = web_view_proxy.scrollViewProxy.zoomScale;
  UIEdgeInsets inset = web_view_proxy.scrollViewProxy.contentInset;

  return CGRectMake((web_view_rect.origin.x * zoom_scale) + inset.left,
                    (web_view_rect.origin.y * zoom_scale) + inset.top,
                    (web_view_rect.size.width * zoom_scale) + kCaretWidth,
                    web_view_rect.size.height * zoom_scale);
}

}  // namespace shared_highlighting
