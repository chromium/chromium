// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_SUBPAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_SUBPAGE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
// class Button;
class Label;
class BubbleFrameView;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSubpageViewId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSubpageBackButtonElementId);

// A sub-page View for the PageSwitcherView in a standard preferred width
// bubble. This view contains:
//   * a header that has a page navigation back button, a title label, a close
//   button
//   * a content view
// *------------------------------------------------*
// | Back | |title|                           Close |
// |________________________________________________|
// ||content view|                                  |
// *-------------------------------------------------*
class SubpageView : public views::View, public views::ViewObserver {
  METADATA_HEADER(SubpageView, views::View)

 public:
  enum SubpageViewID {
    VIEW_ID_SUBPAGE_BACK_BUTTON = 0,
    VIEW_ID_SUBPAGE_CONTENT_VIEW
  };

  explicit SubpageView(views::Button::PressedCallback callback,
                       views::BubbleFrameView* bubble_frame_view);
  ~SubpageView() override;

  void SetTitle(const std::u16string& title);
  void SetContentView(std::unique_ptr<views::View> content);
  void SetHeaderView(std::unique_ptr<views::View> header_view);
  void SetFootnoteView(std::unique_ptr<views::View> footnote_view);

 private:
  void SetUpSubpageTitle(views::Button::PressedCallback callback);

  // ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  const raw_ptr<views::BubbleFrameView> bubble_frame_view_;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::View> content_view_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, SubpageView, views::View)
VIEW_BUILDER_PROPERTY(const std::u16string&, Title)
VIEW_BUILDER_METHOD(SetContentView, std::unique_ptr<views::View>)
VIEW_BUILDER_METHOD(SetHeaderView, std::unique_ptr<views::View>)
VIEW_BUILDER_METHOD(SetFootnoteView, std::unique_ptr<views::View>)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, SubpageView)

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_SUBPAGE_VIEW_H_
