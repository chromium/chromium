// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button_menu_highlighter.h"

#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "ui/views/view_class_properties.h"

void ToolbarButtonMenuHighlighter::MaybeHighlight(
    Browser* browser,
    ToolbarButton* button,
    user_education::HighlightingSimpleMenuModelDelegate* menu_model) {
  if (auto* const service = UserEducationServiceFactory::GetForBrowserContext(
          browser->GetProfile())) {
    HighlightingMenuButtonHelper::MaybeHighlight(
        service->GetFeaturePromoController(
            base::PassKey<ToolbarButtonMenuHighlighter>()),
        button->GetProperty(views::kElementIdentifierKey), menu_model);
  }
}
