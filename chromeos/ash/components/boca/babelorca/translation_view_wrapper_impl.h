// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSLATION_VIEW_WRAPPER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSLATION_VIEW_WRAPPER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/babelorca/caption_bubble_settings_impl.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"

namespace views {
class MdTextButton;
class View;
}  // namespace views

namespace ash::babelorca {

class TranslationViewWrapperImpl : public captions::TranslationViewWrapperBase {
 public:
  explicit TranslationViewWrapperImpl(
      CaptionBubbleSettingsImpl* caption_bubble_settings);

  TranslationViewWrapperImpl(const TranslationViewWrapperImpl&) = delete;
  TranslationViewWrapperImpl& operator=(const TranslationViewWrapperImpl&) =
      delete;

  ~TranslationViewWrapperImpl() override;

  views::MdTextButton* GetTranslateToggleButtonForTesting();
  void SimulateTranslateToggleButtonClickForTesting();

 private:
  // captions::TranslationViewWrapperBase:
  captions::CaptionBubbleSettings* caption_bubble_settings() override;
  void MaybeAddChildViews(views::View* translate_container) override;
  void UpdateLanguageLabelInternal() override;
  void SetTranslationsViewVisible(bool live_translate_enabled) override;

  void OnTranslateToggleButtonPressed();

  const raw_ptr<CaptionBubbleSettingsImpl> caption_bubble_settings_;
  int translate_toggle_button_index_ = -1;
  base::WeakPtrFactory<TranslationViewWrapperImpl> weak_ptr_factory_{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSLATION_VIEW_WRAPPER_IMPL_H_
