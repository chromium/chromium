// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

class Profile;

namespace media_router {

// CastDialogSinkView is a view for the cast dialog that contains buttons for
// interacting with a cast sink.
//
// If the sink is not connected, the view contains CastDialogSinkButton, which
// calls `sink_pressed_callback` when pressed.
//
// If the sink is connected (actively casting), then the view contains a label
// which consists of an icon, device friendly name, and status text that mimics
// the look of the label in a CastDialogSinkButton. Additionally, the view
// contains buttons which trigger actions on the sink. The stop_button_ triggers
// `stop_pressed_callback` when pressed, and the freeze_button_ triggers
// `freeze_pressed_callback` when pressed.
class CastDialogSinkView : public views::View {
  METADATA_HEADER(CastDialogSinkView, views::View)

 public:
  CastDialogSinkView(Profile* profile,
                     const UIMediaSink& sink,
                     views::Button::PressedCallback sink_pressed_callback,
                     views::Button::PressedCallback issue_pressed_callback,
                     views::Button::PressedCallback stop_pressed_callback,
                     views::Button::PressedCallback freeze_pressed_callback);
  CastDialogSinkView(const CastDialogSinkView&) = delete;
  CastDialogSinkView& operator=(const CastDialogSinkView) = delete;
  ~CastDialogSinkView() override;

  // views::View SetEnabled(bool enabled) cannot be overridden.
  void SetEnabledState(bool enabled);

  // views::View:
  void RequestFocus() override;

  const UIMediaSink& sink() const { return sink_; }

  // Used only for testing:
  CastDialogSinkButton* cast_sink_button_for_test() {
    return cast_sink_button_;
  }
  views::MdTextButton* freeze_button_for_test() { return freeze_button_; }
  views::MdTextButton* stop_button_for_test() { return stop_button_; }
  views::StyledLabel* title_for_test() { return title_; }
  views::View* subtitle_for_test() { return subtitle_; }
  void set_sink_for_test(const UIMediaSink& sink) { sink_ = sink; }

 private:
  std::unique_ptr<views::View> CreateButtonsView(
      views::Button::PressedCallback stop_pressed_callback,
      views::Button::PressedCallback freeze_pressed_callback);
  std::unique_ptr<views::View> CreateLabelView(
      const UIMediaSink& sink,
      views::Button::PressedCallback issue_pressed_callback);

  // Gets accessible names for buttons based on if the current route is tab
  // mirroring, screen mirroring, or something else.
  std::u16string GetFreezeButtonAccessibleName() const;
  std::u16string GetStopButtonAccessibleName() const;

  const raw_ptr<Profile> profile_;
  UIMediaSink sink_;

  raw_ptr<CastDialogSinkButton> cast_sink_button_ = nullptr;
  raw_ptr<views::MdTextButton> freeze_button_ = nullptr;
  raw_ptr<views::MdTextButton> stop_button_ = nullptr;

  raw_ptr<views::StyledLabel> title_ = nullptr;
  raw_ptr<views::View> subtitle_ = nullptr;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_VIEW_H_
