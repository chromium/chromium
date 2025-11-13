// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_LIVE_CAPTION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_LIVE_CAPTION_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Combobox;
class Label;
class ToggleButton;
class View;
}  // namespace views

class PrefChangeRegistrar;
class Profile;

class OverlayWindowLiveCaptionDialog : public views::View {
  METADATA_HEADER(OverlayWindowLiveCaptionDialog, views::View)

 public:
  explicit OverlayWindowLiveCaptionDialog(Profile* profile);
  OverlayWindowLiveCaptionDialog(const OverlayWindowLiveCaptionDialog&) =
      delete;
  OverlayWindowLiveCaptionDialog& operator=(
      const OverlayWindowLiveCaptionDialog&) = delete;
  ~OverlayWindowLiveCaptionDialog() override;

  void OnLiveCaptionButtonPressed();
  void OnLiveCaptionEnabledChanged();
  void OnLiveTranslateButtonPressed();
  void OnLiveTranslateEnabledChanged();
  void TargetLanguageChanged();

  views::ToggleButton* live_caption_button_for_testing() const {
    return live_caption_button_;
  }
  views::ToggleButton* live_translate_button_for_testing() const {
    return live_translate_button_;
  }
  views::Combobox* target_language_combobox_for_testing() const {
    return target_language_combobox_;
  }

 private:
  const raw_ptr<Profile> profile_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  raw_ptr<views::Label> live_caption_title_ = nullptr;
  raw_ptr<views::ToggleButton> live_caption_button_ = nullptr;
  raw_ptr<views::Label> live_translate_title_ = nullptr;
  raw_ptr<views::ToggleButton> live_translate_button_ = nullptr;
  raw_ptr<views::View> live_translate_label_wrapper_ = nullptr;
  raw_ptr<views::Combobox> target_language_combobox_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_LIVE_CAPTION_DIALOG_H_
