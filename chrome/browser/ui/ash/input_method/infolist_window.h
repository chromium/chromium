// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_INFOLIST_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_INFOLIST_WINDOW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/ime/infolist_entry.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/gfx/font_list.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ui {
namespace ime {

class InfolistEntryView;

// A widget delegate representing the infolist window UI.
class UI_CHROMEOS_EXPORT InfolistWindow
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(InfolistWindow, views::BubbleDialogDelegateView)

 public:
  InfolistWindow(views::View* candidate_window,
                 const std::vector<ui::InfolistEntry>& entries);
  InfolistWindow(const InfolistWindow&) = delete;
  InfolistWindow& operator=(const InfolistWindow&) = delete;
  ~InfolistWindow() override;
  void InitWidget();

  // Updates infolist contents with |entries|.
  void Relayout(const std::vector<ui::InfolistEntry>& entries);

  // Show/hide itself with a delay.
  void ShowWithDelay();
  void HideWithDelay();

  // Show/hide without delays.
  void ShowImmediately();
  void HideImmediately();

 private:
  // views::WidgetDelegate implementation.
  void WindowClosing() override;

  // The list of visible entries. Owned by views hierarchy.
  std::vector<raw_ptr<InfolistEntryView, VectorExperimental>> entry_views_;

  // Information title font.
  gfx::FontList title_font_list_;

  // Information description font.
  gfx::FontList description_font_list_;

  base::OneShotTimer show_hide_timer_;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT,
                   InfolistWindow,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::InfolistWindow)

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_INFOLIST_WINDOW_H_
