// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_MOCK_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_MOCK_HATS_SERVICE_H_

#include <memory>

#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

class KeyedService;
class Profile;

class MockHatsService : public HatsServiceDesktop {
 public:
  explicit MockHatsService(Profile* profile);
  ~MockHatsService() override;

  MOCK_METHOD(void,
              LaunchSurvey,
              (const std::string& trigger,
               base::OnceClosure success_callback,
               base::OnceClosure failure_callback,
               (const SurveyBitsData&)survey_specific_bits_data,
               (const SurveyStringData&)survey_specific_string_data),
              (override));
  MOCK_METHOD(void,
              LaunchSurveyForWebContents,
              (const std::string& trigger,
               (content::WebContents*)web_contents,
               (const SurveyBitsData&)survey_specific_bits_data,
               (const SurveyStringData&)survey_specific_string_data,
               base::OnceClosure success_callback,
               base::OnceClosure failure_callback,
               const std::optional<std::string>& supplied_trigger_id,
               const HatsService::SurveyOptions& survey_options),
              (override));
  MOCK_METHOD(bool,
              LaunchDelayedSurvey,
              (const std::string& trigger,
               int timeout_ms,
               (const SurveyBitsData&)survey_specific_bits_data,
               (const SurveyStringData&)survey_specific_string_data),
              (override));
  MOCK_METHOD(bool,
              LaunchDelayedSurveyForWebContents,
              (const std::string& trigger,
               content::WebContents* web_contents,
               int timeout_ms,
               (const SurveyBitsData&)survey_specific_bits_data,
               (const SurveyStringData&)survey_specific_string_data,
               (HatsService::NavigationBehaviour)navigation_behaviour,
               base::OnceClosure success_callback,
               base::OnceClosure failure_callback,
               const std::optional<std::string>& supplied_trigger_id,
               const HatsService::SurveyOptions& survey_options),
              (override));
  MOCK_METHOD(void, HatsNextDialogClosed, (), (override));
  MOCK_METHOD(bool, CanShowAnySurvey, (bool user_prompted), (const override));
};

std::unique_ptr<KeyedService> BuildMockHatsService(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_UI_HATS_MOCK_HATS_SERVICE_H_
