// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include <vector>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"

// TODO(crbug.com/40909106): Remove unused constructor when the
// ReadAnythingLocalSidePanel flag is removed.
ReadAnythingController::ReadAnythingController(ReadAnythingModel* model,
                                               Browser* browser)
    : model_(model), browser_(browser) {}

ReadAnythingController::ReadAnythingController(
    ReadAnythingModel* model,
    content::WebContents* web_contents)
    : model_(model), web_contents_(web_contents) {}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingFontCombobox::Delegate:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::OnFontChoiceChanged(int new_index) {
  if (!model_->GetFontModel()->IsValidFontIndex(new_index)) {
    return;
  }

  if (!features::IsReadAnythingWebUIToolbarEnabled()) {
    base::UmaHistogramEnumeration(
        string_constants::kSettingsChangeHistogramName,
        ReadAnythingSettingsChange::kFontChange);
  }
  model_->SetSelectedFontByIndex(new_index);

  GetProfile()->GetPrefs()->SetString(
      prefs::kAccessibilityReadAnythingFontName,
      model_->GetFontModel()->GetFontNameAt(new_index));
}

ReadAnythingFontModel* ReadAnythingController::GetFontComboboxModel() {
  return model_->GetFontModel();
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingToolbarView::Delegate:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::OnFontSizeChanged(bool increase) {
  if (increase) {
    model_->IncreaseTextSize();
  } else {
    model_->DecreaseTextSize();
  }

  if (!features::IsReadAnythingWebUIToolbarEnabled()) {
    base::UmaHistogramEnumeration(
        string_constants::kSettingsChangeHistogramName,
        ReadAnythingSettingsChange::kFontSizeChange);
  }
  GetProfile()->GetPrefs()->SetDouble(
      prefs::kAccessibilityReadAnythingFontScale, model_->GetFontScale());
}

void ReadAnythingController::OnColorsChanged(int new_index) {
  PrefService* prefs = GetProfile()->GetPrefs();
  if (!model_->GetColorsModel()->IsValidIndex(new_index) ||
      prefs->GetInteger(prefs::kAccessibilityReadAnythingColorInfo) ==
          new_index) {
    return;
  }

  if (!features::IsReadAnythingWebUIToolbarEnabled()) {
    base::UmaHistogramEnumeration(
        string_constants::kSettingsChangeHistogramName,
        ReadAnythingSettingsChange::kThemeChange);
  }
  model_->SetSelectedColorsByIndex(new_index);

  prefs->SetInteger(prefs::kAccessibilityReadAnythingColorInfo, new_index);
}

ReadAnythingMenuModel* ReadAnythingController::GetColorsModel() {
  return model_->GetColorsModel();
}

void ReadAnythingController::OnLineSpacingChanged(int new_index) {
  if (!model_->GetLineSpacingModel()->IsValidIndex(new_index)) {
    return;
  }

  if (!features::IsReadAnythingWebUIToolbarEnabled()) {
    base::UmaHistogramEnumeration(
        string_constants::kSettingsChangeHistogramName,
        ReadAnythingSettingsChange::kLineHeightChange);
  }
  model_->SetSelectedLineSpacingByIndex(new_index);

  // Saved preferences correspond to LineSpacing. However, since it contains a
  // deprecated value, the drop-down indices don't correspond exactly.
  LineSpacing line_spacing =
      model_->GetLineSpacingModel()->GetLineSpacingAt(new_index);
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLineSpacing,
      static_cast<size_t>(line_spacing));
}

ReadAnythingMenuModel* ReadAnythingController::GetLineSpacingModel() {
  return model_->GetLineSpacingModel();
}

void ReadAnythingController::OnLetterSpacingChanged(int new_index) {
  if (!model_->GetLetterSpacingModel()->IsValidIndex(new_index)) {
    return;
  }

  if (!features::IsReadAnythingWebUIToolbarEnabled()) {
    base::UmaHistogramEnumeration(
        string_constants::kSettingsChangeHistogramName,
        ReadAnythingSettingsChange::kLetterSpacingChange);
  }
  model_->SetSelectedLetterSpacingByIndex(new_index);

  // Saved preferences correspond to LetterSpacing. However, since it contains a
  // deprecated value, the drop-down indices don't correspond exactly.
  LetterSpacing letter_spacing =
      model_->GetLetterSpacingModel()->GetLetterSpacingAt(new_index);
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing,
      static_cast<size_t>(letter_spacing));
}

void ReadAnythingController::OnLinksEnabledChanged(bool is_enabled) {
  model_->SetLinksEnabled(is_enabled);

  PrefService* prefs = browser_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityReadAnythingLinksEnabled, is_enabled);
}

bool ReadAnythingController::GetLinksEnabled() {
  return model_->GetLinksEnabled();
}

void ReadAnythingController::OnImagesEnabledChanged(bool is_enabled) {
  model_->SetImagesEnabled(is_enabled);

  PrefService* prefs = browser_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityReadAnythingImagesEnabled, is_enabled);
}

bool ReadAnythingController::GetImagesEnabled() {
  return model_->GetImagesEnabled();
}

ReadAnythingMenuModel* ReadAnythingController::GetLetterSpacingModel() {
  return model_->GetLetterSpacingModel();
}

void ReadAnythingController::OnSystemThemeChanged() {
  model_->OnSystemThemeChanged();
}

Profile* ReadAnythingController::GetProfile() {
  if (features::IsReadAnythingLocalSidePanelEnabled() && web_contents_) {
    return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  } else {
    CHECK(browser_);
    return browser_->profile();
  }
}
