// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_BROWSER_TUTORIAL_SERVICE_H_
#define CHROME_BROWSER_USER_EDUCATION_BROWSER_TUTORIAL_SERVICE_H_

#include "components/user_education/common/tutorial_service.h"

namespace user_education {
class TutorialRegistry;
class HelpBubbleFactoryRegistry;
}  // namespace user_education

class BrowserTutorialService : public user_education::TutorialService {
 public:
  BrowserTutorialService(
      user_education::TutorialRegistry* tutorial_registry,
      user_education::HelpBubbleFactoryRegistry* help_bubble_factory_registry);
  ~BrowserTutorialService() override;

  // TutorialService:
  std::u16string GetBodyIconAltText(bool is_last_step) const override;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_BROWSER_TUTORIAL_SERVICE_H_
