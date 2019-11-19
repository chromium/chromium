// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_VIEWER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_VIEWER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace dom_distiller {

class DistilledArticleProto;
class DistilledPageProto;
class DomDistillerServiceInterface;
class ViewerHandle;
class ViewRequestDelegate;

namespace viewer {

// Returns an HTML template page based on the given |page_proto| which provides
// basic information about the page (i.e. title, text direction, etc.). This is
// supposed to be displayed to the end user. The returned HTML should be
// considered unsafe, so callers must ensure rendering it does not compromise
// Chrome.
const std::string GetUnsafeArticleTemplateHtml(
    const std::string& original_url,
    DistilledPagePrefs::Theme theme,
    DistilledPagePrefs::FontFamily font_family);

// Returns the JavaScript to place a full article's HTML on the page. The
// returned HTML should be considered unsafe, so callers must ensure
// rendering it does not compromise Chrome.
const std::string GetUnsafeArticleContentJs(
    const DistilledArticleProto* article_proto);

// Returns a JavaScript blob for updating a partial view request with additional
// distilled content. Meant for use when viewing a slow or long multi-page
// article. |is_last_page| indicates whether this is the last page of the
// article.
const std::string GetUnsafeIncrementalDistilledPageJs(
    const DistilledPageProto* page_proto,
    bool is_last_page);

// Returns the JavaScript to set the title of the distilled article page.
const std::string GetSetTitleJs(std::string title);

// Return the JavaScript to set the text direction of the distiller page.
const std::string GetSetTextDirectionJs(const std::string& direction);

// Returns a JavaScript blob for updating a view request with error page
// contents.
const std::string GetErrorPageJs();

// Returns a JavaScript blob for controlling the "in-progress" indicator when
// viewing a partially-distilled page. |is_last_page| indicates whether this is
// the last page of the article (i.e. loading indicator should be removed).
const std::string GetToggleLoadingIndicatorJs(bool is_last_page);

// Returns the CSS to use for a viewer.
const std::string GetCss();

// Returns the animated SVG loading image for a viewer.
const std::string GetLoadingImage();

// Returns the default JS to be used for a viewer.
const std::string GetJavaScript();

// Based on the given path, calls into the DomDistillerServiceInterface for
// viewing distilled content based on the |path|.
std::unique_ptr<ViewerHandle> CreateViewRequest(
    DomDistillerServiceInterface* dom_distiller_service,
    const GURL& url,
    ViewRequestDelegate* view_request_delegate,
    const gfx::Size& render_view_size);

// Returns JavaScript corresponding to setting the font family.
const std::string GetDistilledPageFontFamilyJs(
    DistilledPagePrefs::FontFamily font);

// Returns JavaScript corresponding to setting a specific theme.
const std::string GetDistilledPageThemeJs(DistilledPagePrefs::Theme theme);

// Returns JavaScript corresponding to setting the font scaling.
const std::string GetDistilledPageFontScalingJs(float scaling);

}  // namespace viewer
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_VIEWER_H_
