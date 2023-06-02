// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FADE_LABEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FADE_LABEL_VIEW_H_

#include "chrome/browser/ui/views/tabs/fade_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"

struct FadeLabelViewData {
  std::u16string text;
  bool is_filename = false;
};

// Label that is able to fade when used in conjunction with FadeView
class FadeLabel : public FadeWrapper<views::Label, FadeLabelViewData> {
 public:
  template <typename... Args>
  explicit FadeLabel(Args&&... args)
      : FadeWrapper<views::Label, FadeLabelViewData>(
            std::forward<Args>(args)...) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetVerticalAlignment(gfx::ALIGN_TOP);
  }

  ~FadeLabel() override = default;

  // FadeWrapper:
  void SetData(const FadeLabelViewData& data) override;
  void SetFade(double percent) override;

  // Renders the label's background in a solid color so that the label can be
  // placed in front of a nother label and animate a fade out
  void SetPaintBackground(bool paint_background);

  // views::Label:
  void OnPaintBackground(gfx::Canvas* canvas) override;

 private:
  // Returns a version of the text that's middle-elided on two lines.
  std::u16string TruncateFilenameToTwoLines(const std::u16string& text) const;

  bool paint_background_ = false;
};

// This view overlays and fades out an old version of the text of a label,
// while displaying the new text underneath. It is used to fade out the old
// value of the title and domain labels on the hover card when the tab switches
// or the tab title changes.
class FadeLabelView : public FadeView<FadeLabel, FadeLabel, FadeLabelViewData> {
 public:
  FadeLabelView(int context, int num_lines);

  ~FadeLabelView() override = default;

  std::u16string GetText();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FADE_LABEL_VIEW_H_
