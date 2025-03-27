// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_COMBINED_SELECTOR_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_COMBINED_SELECTOR_SHEET_VIEW_H_

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/combined_selector_views.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// Web Authentication request dialog sheet view for selecting a credential for
// immediate mediation requests.
class CombinedSelectorSheetView : public AuthenticatorRequestSheetView,
                                  public CombinedSelectorRadioButton::Delegate {
  METADATA_HEADER(CombinedSelectorSheetView, AuthenticatorRequestSheetView)

 public:
  static constexpr int kTopPadding = 8;

  explicit CombinedSelectorSheetView(
      std::unique_ptr<CombinedSelectorSheetModel> model);

  CombinedSelectorSheetView(const CombinedSelectorSheetView&) = delete;
  CombinedSelectorSheetView& operator=(const CombinedSelectorSheetView&) =
      delete;

  ~CombinedSelectorSheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificHeader() override;
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override;

  // CombinedSelectorRadioButton::Delegate
  void OnRadioButtonChecked(int index) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_COMBINED_SELECTOR_SHEET_VIEW_H_
