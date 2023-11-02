// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_TEXT_FIELD_H_
#define CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_TEXT_FIELD_H_

#include "base/callback_forward.h"
#include "chrome/browser/vr/elements/text_input.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

class VR_UI_EXPORT OmniboxTextField : public TextInput {
 public:
  OmniboxTextField(float font_height_meters,
                   OnInputEditedCallback input_edit_callback,
                   base::RepeatingCallback<void(const AutocompleteRequest&)>
                       autocomplete_start_callback,
                   base::RepeatingCallback<void()> autocomplete_stop_callback);

  OmniboxTextField(const OmniboxTextField&) = delete;
  OmniboxTextField& operator=(const OmniboxTextField&) = delete;

  ~OmniboxTextField() override;

  // This element uses its enabled status to manage outstanding autocomplete
  // sessions, which may need to persist regardless of visibility.
  void SetEnabled(bool enabled);

  void SetAutocompletion(const Autocompletion& autocompletion);

  void set_allow_inline_autocomplete(bool allowed) {
    allow_inline_autocomplete_ = allowed;
  }

 private:
  void OnUpdateInput(const EditedText& info) override;

  base::RepeatingCallback<void(const AutocompleteRequest&)>
      autocomplete_start_callback_;
  base::RepeatingCallback<void()> autocomplete_stop_callback_;

  bool allow_inline_autocomplete_ = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_TEXT_FIELD_H_
