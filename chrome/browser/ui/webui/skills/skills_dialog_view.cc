// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace skills {
namespace {

constexpr float kCornerRadius = 12.0f;
constexpr int kWebViewWidth = 448;
constexpr int kWebViewMinHeight = 442;
constexpr int kWebViewMaxHeight = 516;  // Extra space needed for errors and
                                        // multi-line user account info.
gfx::Size kWebViewMinSize = gfx::Size(kWebViewWidth, kWebViewMinHeight);
gfx::Size kWebViewMaxSize = gfx::Size(kWebViewWidth, kWebViewMaxHeight);
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SkillsDialogView, kSkillsDialogElementId);

SkillsDialogView::SkillsDialogView(Profile* profile) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Create the webview.
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->SetProperty(views::kElementIdentifierKey, kSkillsDialogElementId);
  web_view_ = web_view.get();

  web_view->SetPreferredSize(kWebViewMinSize);
  web_view_->SetPaintToLayer();
  web_view_->layer()->SetFillsBoundsOpaquely(false);
  web_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadius));
  web_view_->layer()->SetMasksToBounds(true);
  web_view_->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_view_->LoadInitialURL(GURL(std::string(chrome::kChromeUISkillsURL) +
                                 chrome::kChromeUISkillsDialogPath));
  web_view_->GetWebContents()->SetDelegate(this);
  web_view_->EnableSizingFromWebContents(kWebViewMinSize, kWebViewMaxSize);
  AddChildView(std::move(web_view));
}

SkillsDialogView::~SkillsDialogView() {
  if (web_view_ && web_view_->GetWebContents()) {
    web_view_->GetWebContents()->SetDelegate(nullptr);
  }
}

void SkillsDialogView::ChildPreferredSizeChanged(views::View* child) {
  views::View::ChildPreferredSizeChanged(child);
  if (GetWidget()) {
    GetWidget()->SetSize(GetPreferredSize());
  }
}

gfx::Size SkillsDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetLayoutManager()->GetPreferredSize(this, available_size);
}

bool SkillsDialogView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (!web_view_) {
    return false;
  }

  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, web_view_->GetFocusManager());
}

void SkillsDialogView::ResizeDueToAutoResize(content::WebContents* web_contents,
                                             const gfx::Size& new_size) {
  if (web_view_) {
    web_view_->SetPreferredSize(new_size);
  }
}

BEGIN_METADATA(SkillsDialogView)
END_METADATA

}  // namespace skills
