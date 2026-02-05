// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace skills {
namespace {

constexpr float kCornerRadius = 12.0f;
constexpr int kWebViewWidth = 448;
constexpr int kWebViewHeight = 474;
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SkillsDialogView, kSkillsDialogElementId);

SkillsDialogView::SkillsDialogView(Profile* profile) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Create the webview.
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->SetProperty(views::kElementIdentifierKey, kSkillsDialogElementId);
  web_view_ = web_view.get();

  web_view->SetPreferredSize(gfx::Size(kWebViewWidth, kWebViewHeight));
  web_view_->SetPaintToLayer();
  web_view_->layer()->SetFillsBoundsOpaquely(false);
  web_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadius));
  web_view_->layer()->SetMasksToBounds(true);
  web_view_->LoadInitialURL(GURL(std::string(chrome::kChromeUISkillsURL) +
                                 chrome::kChromeUISkillsDialogPath));
  AddChildView(std::move(web_view));
}

SkillsDialogView::~SkillsDialogView() = default;

gfx::Size SkillsDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetLayoutManager()->GetPreferredSize(this, available_size);
}

BEGIN_METADATA(SkillsDialogView)
END_METADATA

}  // namespace skills
