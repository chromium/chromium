// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_test_base.h"

#include <memory>
#include <optional>
#include <utility>

namespace autofill {

AutofillExternalDelegateForPopupTest::AutofillExternalDelegateForPopupTest(
    BrowserAutofillManager* autofill_manager)
    : AutofillExternalDelegate(autofill_manager) {}

AutofillExternalDelegateForPopupTest::~AutofillExternalDelegateForPopupTest() =
    default;

AutofillPopupControllerForPopupTest::AutofillPopupControllerForPopupTest(
    base::WeakPtr<AutofillExternalDelegate> external_delegate,
    content::WebContents* web_contents,
    const gfx::RectF& element_bounds
#if BUILDFLAG(IS_ANDROID)
    ,
    ShowPasswordMigrationWarningCallback show_pwd_migration_warning_callback
#endif
    )
    : AutofillPopupControllerForPopupTestBase(
          external_delegate,
          web_contents,
          PopupControllerCommon(element_bounds,
                                base::i18n::UNKNOWN_DIRECTION,
                                nullptr),
#if !BUILDFLAG(IS_ANDROID)
          /*form_control_ax_id=*/0
#else
          std::move(show_pwd_migration_warning_callback)
#endif
      ) {
}

AutofillPopupControllerForPopupTest::~AutofillPopupControllerForPopupTest() =
    default;

}  // namespace autofill
