// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ui {
class MouseEvent;
}

namespace media_router {

// A button representing a sink in the Cast dialog. It is highlighted when
// hovered.
class CastDialogSinkButton : public HoverButton {
  METADATA_HEADER(CastDialogSinkButton, HoverButton)

 public:
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
  void OnThemeChanged() override;

  const UIMediaSink& sink() const { return sink_; }

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

  // views::Button:
  void OnEnabledChanged() override;

  void UpdateTitleTextStyle();

  const UIMediaSink sink_;
  std::optional<std::u16string> saved_status_text_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
