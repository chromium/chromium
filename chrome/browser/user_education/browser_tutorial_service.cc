// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/browser_tutorial_service.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

BrowserTutorialService::BrowserTutorialService(
    user_education::TutorialRegistry* tutorial_registry,
    user_education::HelpBubbleFactoryRegistry* help_bubble_factory_registry)
    : TutorialService(tutorial_registry, help_bubble_factory_registry) {}

BrowserTutorialService::~BrowserTutorialService() = default;

std::u16string BrowserTutorialService::GetBodyIconAltText(
    bool is_last_step) const {
  return l10n_util::GetStringUTF16(IDS_CHROME_TIP);
}
