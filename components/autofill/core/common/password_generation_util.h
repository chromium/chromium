// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_GENERATION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_GENERATION_UTIL_H_

#include <string>

#include "base/i18n/rtl.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

namespace password_generation {

// Minimal length of the generated password.
inline constexpr int kMinimumPasswordLength = 9;

// Minimum password length that allows to label the password as strong in the
// UI. Must stay in sync with LENGTH_SUFFICIENT_FOR_STRONG_LABEL in
// TouchToFillPasswordGenerationView.java
inline constexpr int kLengthSufficientForStrongLabel = 12;

// Enumerates various events related to the password generation process.
// Do not remove items from this enum as they are used for UMA stats logging.
enum PasswordGenerationEvent {
  // No Account creation form is detected.
  NO_SIGN_UP_DETECTED,

  // Account creation form is detected.
  SIGN_UP_DETECTED,

  // DEPRECATED: Password generation icon shown (old UI).
  DEPRECATED_ICON_SHOWN,

  // DEPRECATED: Password generation bubble shown (old UI).
  DEPRECATED_BUBBLE_SHOWN,

  // Password generation could be triggered if the user selects the appropriate
  // element.
  GENERATION_AVAILABLE,

  // Password generation popup is shown after user focuses the appropriate
  // password field.
  // DEPRECATED: These reports were triggered when the popup could have shown
  // not when it did show so they paint an unreliable picture. Newer stats
  // are only incremented per page, which is more useful to judge the
  // effectiveness of the UI.
  DEPRECATED_GENERATION_POPUP_SHOWN,

  // Generated password was accepted by the user.
  PASSWORD_ACCEPTED,

  // User focused the password field containing the generated password.
  // DEPRECATED: These reports were triggered when the popup could have shown
  // not when it did show so they paint an unreliable picture. Newer stats
  // are only incremented per page, which is more useful to judge the
  // effectiveness of the UI.
  DEPRECATED_EDITING_POPUP_SHOWN,

  // Password was changed after generation.
  PASSWORD_EDITED,

  // Generated password was deleted by the user
  PASSWORD_DELETED,

  // Password generation popup is shown after user focuses the appropriate
  // password field.
  GENERATION_POPUP_SHOWN,

  // User focused the password field containing the generated password.
  EDITING_POPUP_SHOWN,

  // Generation enabled because autocomplete attributes for new-password is set.
  AUTOCOMPLETE_ATTRIBUTES_ENABLED_GENERATION,

  // Generation is triggered by the user from the context menu.
  PASSWORD_GENERATION_CONTEXT_MENU_PRESSED,

  // Context menu with generation item was shown.
  PASSWORD_GENERATION_CONTEXT_MENU_SHOWN,

  // The generated password was removed from the field because a credential
  // was autofilled.
  PASSWORD_DELETED_BY_AUTOFILLING,

  // Number of enum entries, used for UMA histogram reporting macros.
  EVENT_ENUM_COUNT
};

// The enum, which identifies where the password generation was triggered.
// Used to determine the histogram name for metrics reporting.
enum class PasswordGenerationType {
  // The possibility for automatic generation was detected and the user
  // requested the generation.
  kAutomatic,
  // The possibility for automatic generation was not detected and the user
  // manually requested the password generation.
  kManual,
  // The possibility for automatic generation was detected and the password
  // generation bottom sheet was automatically triggered without the user
  // choice.
  kTouchToFill,
};

// Wrapper to store the user interactions with the password generation bubble.
struct PasswordGenerationActions {
  // Whether the user has clicked on the learn more link.
  bool learn_more_visited;

  // Whether the user has accepted the generated password.
  bool password_accepted;

  // Whether the user has manually edited password entry.
  bool password_edited;

  // Whether the user has clicked on the regenerate button.
  bool password_regenerated;

  PasswordGenerationActions();
  ~PasswordGenerationActions();
};

struct PasswordGenerationUIData {
  PasswordGenerationUIData(const gfx::RectF& bounds,
                           int max_length,
                           const std::u16string& generation_element,
                           FieldRendererId generation_element_id,
                           bool is_generation_element_password_type,
                           base::i18n::TextDirection text_direction,
                           const FormData& form_data,
                           bool input_field_empty);
  PasswordGenerationUIData();
  PasswordGenerationUIData(const PasswordGenerationUIData& rhs);
  PasswordGenerationUIData(PasswordGenerationUIData&& rhs);
  PasswordGenerationUIData& operator=(const PasswordGenerationUIData& rhs);
  PasswordGenerationUIData& operator=(PasswordGenerationUIData&& rhs);
  ~PasswordGenerationUIData();

  // Location at which to display a popup if needed. This location is specified
  // in the renderer's coordinate system. The popup will be anchored at
  // |bounds|.
  gfx::RectF bounds;

  // Maximum length of the generated password.
  int max_length = 0;

  // Name of the password field to which the generation popup is attached.
  std::u16string generation_element;

  // Renderer ID of the generation element.
  FieldRendererId generation_element_id;

  // Is the generation element |type=password|.
  bool is_generation_element_password_type = false;

  // Direction of the text for |generation_element|.
  base::i18n::TextDirection text_direction =
      base::i18n::TextDirection::UNKNOWN_DIRECTION;

  // The form associated with the password field.
  FormData form_data;

  // Whether the password input field is empty.
  bool input_field_empty = false;
};

void LogPasswordGenerationEvent(PasswordGenerationEvent event);

}  // namespace password_generation
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_GENERATION_UTIL_H_
