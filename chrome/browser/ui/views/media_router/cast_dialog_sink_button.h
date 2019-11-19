// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_

#include "base/bind.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/hover_button.h"

namespace ui {
class MouseEvent;
}

namespace media_router {

// A button representing a sink in the Cast dialog. It is highlighted when
// hovered.
class CastDialogSinkButton : public HoverButton {
 public:
  CastDialogSinkButton(views::ButtonListener* button_listener,
                       const UIMediaSink& sink,
                       int button_tag);
  ~CastDialogSinkButton() override;

  void OverrideStatusText(const base::string16& status_text);
  void RestoreStatusText();

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void RequestFocus() override;
  void OnFocus() override;
  void OnBlur() override;

  const UIMediaSink& sink() const { return sink_; }

 private:
  friend class MediaRouterUiForTest;
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest, OverrideStatusText);
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest,
                           SetStatusLabelForActiveSink);
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest,
                           SetStatusLabelForAvailableSink);
  FRIEND_TEST_ALL_PREFIXES(CastDialogSinkButtonTest,
                           SetStatusLabelForSinkWithIssue);

  void OnEnabledChanged();

  const UIMediaSink sink_;
  base::Optional<base::string16> saved_status_text_;
  views::PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&CastDialogSinkButton::OnEnabledChanged,
                              base::Unretained(this)));

  DISALLOW_COPY_AND_ASSIGN(CastDialogSinkButton);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
