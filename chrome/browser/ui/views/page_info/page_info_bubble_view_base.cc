// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"

#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/buildflags.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace {

// Per-WebContents tracker for the open Page Info bubble view.
class PageInfoBubbleTracker
    : public content::WebContentsUserData<PageInfoBubbleTracker> {
 public:
  PageInfoBubbleTracker(const PageInfoBubbleTracker&) = delete;
  PageInfoBubbleTracker& operator=(const PageInfoBubbleTracker&) = delete;
  ~PageInfoBubbleTracker() override = default;

  static PageInfoBubbleViewBase* GetBubble(content::WebContents* web_contents) {
    if (!web_contents) {
      return nullptr;
    }
    auto* tracker = PageInfoBubbleTracker::FromWebContents(web_contents);
    return tracker ? tracker->GetBubble() : nullptr;
  }

  void SetBubble(PageInfoBubbleViewBase* bubble) {
    if (PageInfoBubbleViewBase* const current_bubble = GetBubble();
        bubble && current_bubble && current_bubble != bubble) {
      current_bubble->GetWidget()->Close();
    }
    tracker_.SetView(bubble);
  }

 private:
  explicit PageInfoBubbleTracker(content::WebContents* web_contents)
      : content::WebContentsUserData<PageInfoBubbleTracker>(*web_contents) {}
  friend class content::WebContentsUserData<PageInfoBubbleTracker>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  PageInfoBubbleViewBase* GetBubble() {
    return static_cast<PageInfoBubbleViewBase*>(tracker_.view());
  }

  views::ViewTracker tracker_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageInfoBubbleTracker);

// TODO(olesiamarukhno): Remove g_shown_bubble_type and g_page_info_bubble.
// The following two process-wide variables are legacy fallbacks kept for tests
// that do not pass a WebContents. Production code and new tests track Page Info
// bubbles per-WebContents via PageInfoBubbleTracker.
PageInfoBubbleViewBase::BubbleType g_shown_bubble_type =
    PageInfoBubbleViewBase::BUBBLE_NONE;
PageInfoBubbleViewBase* g_page_info_bubble = nullptr;

}  // namespace

// static
bool PageInfoBubbleViewBase::IsShowing(content::WebContents* web_contents) {
  return PageInfoBubbleTracker::GetBubble(web_contents) != nullptr;
}

// static
PageInfoBubbleViewBase::BubbleType
PageInfoBubbleViewBase::GetShownBubbleType() {
  return g_shown_bubble_type;
}

// static
PageInfoBubbleViewBase::BubbleType PageInfoBubbleViewBase::GetShownBubbleType(
    content::WebContents* web_contents) {
  PageInfoBubbleViewBase* const bubble =
      PageInfoBubbleTracker::GetBubble(web_contents);
  return bubble ? bubble->type() : BUBBLE_NONE;
}

// static
views::BubbleDialogDelegateView*
PageInfoBubbleViewBase::GetPageInfoBubbleForTesting() {
  return g_page_info_bubble;
}

// static
views::BubbleDialogDelegateView*
PageInfoBubbleViewBase::GetPageInfoBubbleForTesting(
    content::WebContents* web_contents) {
  return PageInfoBubbleTracker::GetBubble(web_contents);
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoBubbleViewBase,
                                      kPageInfoBubbleElementIdentifier);
PageInfoBubbleViewBase::PageInfoBubbleViewBase(
    views::BubbleAnchor anchor,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    PageInfoBubbleViewBase::BubbleType type,
    content::WebContents* web_contents)
    : BubbleDialogDelegateView(anchor,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::DIALOG_SHADOW,
                               /*autosize=*/false),
      content::WebContentsObserver(web_contents),
      type_(type) {
  g_shown_bubble_type = type;
  g_page_info_bubble = this;

  if (web_contents) {
    PageInfoBubbleTracker::CreateForWebContents(web_contents);
    PageInfoBubbleTracker::FromWebContents(web_contents)->SetBubble(this);
  }

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);

  // If anchored to a specific view, skip set_parent_window() so that
  // BubbleDialogDelegateView automatically parents to the anchor view's widget.
  // In Mac immersive fullscreen, this ensures the bubble is parented to the
  // top container overlay_widget rather than the main browser window.
  if (anchor.IsNull()) {
    set_parent_window(parent_window);
    SetAnchorRect(anchor_rect);
  }
  SetProperty(views::kElementIdentifierKey, kPageInfoBubbleElementIdentifier);
}

void PageInfoBubbleViewBase::OnWidgetDestroying(views::Widget* widget) {
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
  if (g_page_info_bubble == this) {
    g_shown_bubble_type = BUBBLE_NONE;
    g_page_info_bubble = nullptr;
  }
  if (PageInfoBubbleTracker::GetBubble(web_contents()) == this) {
    PageInfoBubbleTracker::FromWebContents(web_contents())->SetBubble(nullptr);
  }
}

void PageInfoBubbleViewBase::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    GetWidget()->Close();
  }
}

void PageInfoBubbleViewBase::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    GetWidget()->Close();
  }
}

void PageInfoBubbleViewBase::PrimaryPageChanged(content::Page& page) {
  GetWidget()->Close();
}

void PageInfoBubbleViewBase::DidChangeVisibleSecurityState() {
  // Subclasses may update instead, but this the only safe general option.
  GetWidget()->Close();
}

void PageInfoBubbleViewBase::WebContentsDestroyed() {
  GetWidget()->Close();
}

BEGIN_METADATA(PageInfoBubbleViewBase)
END_METADATA
