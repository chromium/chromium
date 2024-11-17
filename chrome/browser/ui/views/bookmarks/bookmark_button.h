// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUTTON_H_

#include "base/timer/timer.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "content/public/browser/prerender_handle.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/view.h"

class Browser;

// Base class for buttons used on the bookmark bar.
class BookmarkButtonBase : public views::LabelButton {
  METADATA_HEADER(BookmarkButtonBase, views::LabelButton)

 public:
  BookmarkButtonBase(PressedCallback callback, const std::u16string& title);
  BookmarkButtonBase(const BookmarkButtonBase&) = delete;
  BookmarkButtonBase& operator=(const BookmarkButtonBase&) = delete;
  ~BookmarkButtonBase() override;

  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  bool IsTriggerableEvent(const ui::Event& e) override;

  // LabelButton:
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;

 private:
  std::unique_ptr<gfx::SlideAnimation> show_animation_;
};

// Buttons used on the bookmark bar.
class BookmarkButton : public BookmarkButtonBase {
  METADATA_HEADER(BookmarkButton, BookmarkButtonBase)

 public:
  BookmarkButton(PressedCallback callback,
                 const GURL& url,
                 const std::u16string& title,
                 const raw_ptr<Browser> browser);
  BookmarkButton(const BookmarkButton&) = delete;
  BookmarkButton& operator=(const BookmarkButton&) = delete;
  ~BookmarkButton() override;

  void OnButtonPressed(const ui::Event& event) { callback_.Run(event); }

  // views::View:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void AdjustAccessibleName(std::u16string& new_name,
                            ax::mojom::NameFrom& name_from) override;
  void SetText(const std::u16string& text) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;

 private:
  void StartPreconnecting(GURL url);
  void StartPrerendering(GURL url);

  // A cached value of maximum width for tooltip to skip generating
  // new tooltip text.
  mutable int max_tooltip_width_ = 0;
  mutable std::u16string tooltip_text_;
  PressedCallback callback_;
  const raw_ref<const GURL> url_;
  const raw_ptr<Browser> browser_;
  base::WeakPtr<content::PrerenderHandle> prerender_handle_;
  base::RetainingOneShotTimer preloading_timer_;
  base::WeakPtr<content::WebContents> prerender_web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUTTON_H_
