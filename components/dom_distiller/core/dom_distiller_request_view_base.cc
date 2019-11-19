// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_request_view_base.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/viewer.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace dom_distiller {

DomDistillerRequestViewBase::DomDistillerRequestViewBase(
    DistilledPagePrefs* distilled_page_prefs)
    : page_count_(0),
      distilled_page_prefs_(distilled_page_prefs),
      is_error_page_(false) {}

DomDistillerRequestViewBase::~DomDistillerRequestViewBase() {}

void DomDistillerRequestViewBase::FlagAsErrorPage() {
  // Viewer handle is not passed to this in the case of error pages
  // so send all JavaScript now.
  SendCommonJavaScript();
  SendJavaScript(viewer::GetErrorPageJs());

  is_error_page_ = true;
}

bool DomDistillerRequestViewBase::IsErrorPage() {
  return is_error_page_;
}

void DomDistillerRequestViewBase::OnArticleReady(
    const DistilledArticleProto* article_proto) {
  if (page_count_ == 0) {
    std::string text_direction;
    if (article_proto->pages().size() > 0) {
      text_direction = article_proto->pages(0).text_direction();
    } else {
      text_direction = "auto";
    }
    // Send first page, title, and text direction to client.
    SendJavaScript(viewer::GetSetTitleJs(article_proto->title()));
    SendJavaScript(viewer::GetSetTextDirectionJs(text_direction));
    SendJavaScript(viewer::GetUnsafeArticleContentJs(article_proto));
  } else {
    // It's possible that we didn't get some incremental updates from the
    // distiller. Ensure all remaining pages are flushed to the viewer.
    for (; page_count_ < article_proto->pages_size(); page_count_++) {
      const DistilledPageProto& page = article_proto->pages(page_count_);
      SendJavaScript(viewer::GetUnsafeIncrementalDistilledPageJs(
          &page, page_count_ == article_proto->pages_size()));
    }
  }
  // We may still be showing the "Loading" indicator.
  SendJavaScript(viewer::GetToggleLoadingIndicatorJs(true));
  // No need to hold on to the ViewerHandle now that distillation is complete.
  viewer_handle_.reset();
}

void DomDistillerRequestViewBase::OnArticleUpdated(
    ArticleDistillationUpdate article_update) {
  for (; page_count_ < static_cast<int>(article_update.GetPagesSize());
       page_count_++) {
    const DistilledPageProto& page =
        article_update.GetDistilledPage(page_count_);
    // Send the page content to the client. This will execute after the page is
    // ready.
    SendJavaScript(viewer::GetUnsafeIncrementalDistilledPageJs(
        &page, !article_update.HasNextPage()));

    if (page_count_ == 0) {
      // This is the first page, so send the title and text direction to the
      // client.
      SendJavaScript(viewer::GetSetTitleJs(page.title()));
      SendJavaScript(viewer::GetSetTextDirectionJs(page.text_direction()));
    }
  }
}

void DomDistillerRequestViewBase::OnChangeTheme(
    DistilledPagePrefs::Theme new_theme) {
  SendJavaScript(viewer::GetDistilledPageThemeJs(new_theme));
}

void DomDistillerRequestViewBase::OnChangeFontFamily(
    DistilledPagePrefs::FontFamily new_font) {
  SendJavaScript(viewer::GetDistilledPageFontFamilyJs(new_font));
}

void DomDistillerRequestViewBase::OnChangeFontScaling(float scaling) {
  SendJavaScript(viewer::GetDistilledPageFontScalingJs(scaling));
}

void DomDistillerRequestViewBase::TakeViewerHandle(
    std::unique_ptr<ViewerHandle> viewer_handle) {
  viewer_handle_ = std::move(viewer_handle);
  // Getting the viewer handle means this is not an error page, send
  // the viewer JavaScript and show the loading indicator.
  SendCommonJavaScript();
}

void DomDistillerRequestViewBase::SendCommonJavaScript() {
  SendJavaScript(viewer::GetJavaScript());
  SendJavaScript(viewer::GetDistilledPageFontScalingJs(
      distilled_page_prefs_->GetFontScaling()));
}

}  // namespace dom_distiller
