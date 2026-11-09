// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/core/web_distiller_request_view_base.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "components/strings/grit/components_strings.h"
#include "components/web_distiller/core/distilled_page_prefs.h"
#include "components/web_distiller/core/experiments.h"
#include "components/web_distiller/core/task_tracker.h"
#include "components/web_distiller/core/viewer.h"
#include "components/web_distiller/core/web_distiller_features.h"
#include "components/web_distiller/core/web_distiller_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace web_distiller {

namespace {
float GetBaseFontSize() {
  return 16.0f;
}
}  // namespace

WebDistillerRequestViewBase::WebDistillerRequestViewBase(
    DistilledPagePrefs* distilled_page_prefs)
    : distilled_page_prefs_(distilled_page_prefs), is_error_page_(false) {}

WebDistillerRequestViewBase::~WebDistillerRequestViewBase() = default;

void WebDistillerRequestViewBase::FlagAsErrorPage() {
  // Viewer handle is not passed to this in the case of error pages
  // so send all JavaScript now.
  SendCommonJavaScript();
  SendJavaScript(viewer::GetErrorPageJs());

  is_error_page_ = true;
}

bool WebDistillerRequestViewBase::IsErrorPage() {
  return is_error_page_;
}

void WebDistillerRequestViewBase::OnArticleReady(
    const DistilledArticleProto* article_proto) {
  const std::string text_direction =
      article_proto->pages().empty() ? "auto"
                                     : article_proto->pages(0).text_direction();
  SendJavaScript(viewer::GetSetTitleJs(article_proto->title()));
  SendJavaScript(viewer::GetSetTextDirectionJs(text_direction));
  SendJavaScript(viewer::GetUnsafeArticleContentJs(article_proto));
  SendJavaScript(viewer::GetDistilledPageFontScalingJs(
      distilled_page_prefs_->GetFontScaling(), /* restoreCenter= */ false));
  SendJavaScript(viewer::GetDistilledPageLinksEnabledJs(
      distilled_page_prefs_->GetLinksEnabled()));
  // No need to hold on to the ViewerHandle now that distillation is complete.
  viewer_handle_.reset();
}

void WebDistillerRequestViewBase::OnChangeTheme(
    mojom::Theme new_theme,
    ThemeSettingsUpdateSource source) {
  SendJavaScript(viewer::GetDistilledPageThemeJs(new_theme));
}

void WebDistillerRequestViewBase::OnChangeFontFamily(
    mojom::FontFamily new_font) {
  SendJavaScript(viewer::GetDistilledPageFontFamilyJs(new_font));
}

void WebDistillerRequestViewBase::OnChangeFontScaling(float scaling) {
  SendJavaScript(viewer::GetDistilledPageFontScalingJs(
      scaling, /* restoreCenter= */ true));
}

void WebDistillerRequestViewBase::OnChangeLinksEnabled(bool enabled) {
  SendJavaScript(viewer::GetDistilledPageLinksEnabledJs(enabled));
}

void WebDistillerRequestViewBase::TakeViewerHandle(
    std::unique_ptr<ViewerHandle> viewer_handle) {
  viewer_handle_ = std::move(viewer_handle);
  // Getting the viewer handle means this is not an error page, send
  // the viewer JavaScript and show the loading indicator.
  SendCommonJavaScript();
}

void WebDistillerRequestViewBase::SendCommonJavaScript() {
  SendJavaScript(viewer::GetJavaScript());
  SendJavaScript(viewer::GetDistilledPageFontScalingJs(
      distilled_page_prefs_->GetFontScaling(), /* restoreCenter= */ false));
  SendJavaScript(viewer::SetDistilledPageBaseFontSize(GetBaseFontSize()));
  SendJavaScript(viewer::GetDistilledPageLinksEnabledJs(
      distilled_page_prefs_->GetLinksEnabled()));
}

}  // namespace web_distiller
