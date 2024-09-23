// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_VIEW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace ui {
namespace ime {

// CandidateView renderes a row of a candidate.
class UI_CHROMEOS_EXPORT CandidateView : public views::Button {
  METADATA_HEADER(CandidateView, views::Button)

 public:
  CandidateView(PressedCallback callback,
                ui::CandidateWindow::Orientation orientation);
  CandidateView(const CandidateView&) = delete;
  CandidateView& operator=(const CandidateView&) = delete;
  ~CandidateView() override {}

  void GetPreferredWidths(int* shortcut_width, int* candidate_width);

  void SetWidths(int shortcut_width, int candidate_width);

  void SetEntry(const ui::CandidateWindow::Entry& entry);

  // Sets infolist icon.
  void SetInfolistIcon(bool enable);

  void SetHighlighted(bool highlighted);

  void SetPositionData(int index, int total);

 private:
  friend class CandidateWindowViewTest;
  FRIEND_TEST_ALL_PREFIXES(CandidateWindowViewTest, ShortcutSettingTest);

  // Overridden from views::Button:
  void StateChanged(ButtonState old_state) override;

  // Overridden from View:
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // The orientation of the candidate view.
  ui::CandidateWindow::Orientation orientation_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  // The shortcut label renders shortcut numbers like 1, 2, and 3.
  raw_ptr<views::Label> shortcut_label_ = nullptr;
  // The candidate label renders candidates.
  raw_ptr<views::Label> candidate_label_ = nullptr;
  // The annotation label renders annotations.
  raw_ptr<views::Label> annotation_label_ = nullptr;
  // The infolist icon.
  raw_ptr<views::View> infolist_icon_ = nullptr;

  int shortcut_width_ = 0;
  int candidate_width_ = 0;
  bool highlighted_ = false;

  // 0-based index of this candidate e.g. [0, total_candidates_ -1].
  int candidate_index_ = 0;
  int total_candidates_ = 0;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT, CandidateView, views::Button)
VIEW_BUILDER_PROPERTY(bool, InfolistIcon)
VIEW_BUILDER_PROPERTY(bool, Highlighted)
VIEW_BUILDER_PROPERTY(const ui::CandidateWindow::Entry&, Entry)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::CandidateView)

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_CANDIDATE_VIEW_H_
