// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_UI_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_UI_H_

namespace autofill {

// The type of Mandatory Reauth bubble to display.
enum class MandatoryReauthBubbleType {
  // There is no bubble to show anymore. This also indicates that the icon
  // should not be visible.
  kInactive = 0,

  // Bubble prompting the user to enable mandatory reauth.
  kOptIn = 1,

  // Bubble confirming that the user has opted into mandatory reauth.
  kConfirmation = 2,
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_UI_H_
