// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_

#include <memory>

#include "base/bind.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Profile;

namespace ui {
class MouseEvent;
}

namespace media_router {

// A button representing a sink in the Cast dialog. It is highlighted when
// hovered.
class CastDialogSinkButton : public HoverButton {
 public:
  METADATA_HEADER(CastDialogSinkButton);
  CastDialogSinkButton(PressedCallback callback, const UIMediaSink& sink);
  CastDialogSinkButton(const CastDialogSinkButton&) = delete;
  CastDialogSinkButton& operator=(const CastDialogSinkButton&) = delete;
  ~CastDialogSinkButton() override;

  void OverrideStatusText(const std::u16string& status_text);
  void RestoreStatusText();

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void RequestFocus() override;
  void OnFocus() override;
  void OnBlur() override;

  const UIMediaSink& sink() const { return sink_; }

  // If this button will cast to a meeting, creates a view showing a warning
  // about the feature being deprecated.  Otherwise returns nullptr.  The
  // |profile| parameter is used to open the meeting tab the the user clicks on
  // the link in the warning.
  std::unique_ptr<views::View> MakeCastToMeetingDeprecationWarningView(
      Profile* profile);

  static const gfx::VectorIcon* GetVectorIcon(SinkIconType icon_type);
  static const gfx::VectorIcon* GetVectorIcon(UIMediaSink sink);

 private:
  friend class MediaRouterUiForTest;
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest, OverrideStatusText);
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest,
                           SetStatusLabelForActiveSink);
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest,
                           SetStatusLabelForAvailableSink);
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest,
                           SetStatusLabelForSinkWithIssue);
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest,
                           SetStatusLabelForDialSinks);

  void OnEnabledChanged();

  const UIMediaSink sink_;
  base::Optional<std::u16string> saved_status_text_;
  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&CastDialogSinkButton::OnEnabledChanged,
                              base::Unretained(this)));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
