// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/internals/user_education/user_education_internals.mojom.h"
#include "components/user_education/common/tutorial_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

class UserEducationInternalsPageHandlerImpl
    : public mojom::user_education_internals::
          UserEducationInternalsPageHandler {
 public:
  UserEducationInternalsPageHandlerImpl(
      content::WebUI* web_ui,
      Profile* profile,
      mojo::PendingReceiver<
          mojom::user_education_internals::UserEducationInternalsPageHandler>
          receiver);
  ~UserEducationInternalsPageHandlerImpl() override;

  UserEducationInternalsPageHandlerImpl(
      const UserEducationInternalsPageHandlerImpl&) = delete;
  UserEducationInternalsPageHandlerImpl& operator=(
      const UserEducationInternalsPageHandlerImpl&) = delete;

  // mojom::user_education_internals::UserEducationInternalsPageHandler:
  void GetTutorials(GetTutorialsCallback callback) override;
  void StartTutorial(const std::string& tutorial_id,
                     StartTutorialCallback callback) override;

  void GetSessionData(GetSessionDataCallback callback) override;
  void GetFeaturePromos(GetFeaturePromosCallback callback) override;
  void ShowFeaturePromo(const std::string& feature_name,
                        ShowFeaturePromoCallback callback) override;
  void ClearFeaturePromoData(const std::string& feature_name,
                             ClearFeaturePromoDataCallback callback) override;
  void ClearSessionData(ClearSessionDataCallback callback) override;
  void GetNewBadges(GetNewBadgesCallback callback) override;
  void ClearNewBadgeData(const std::string& feature_name,
                         ClearNewBadgeDataCallback callback) override;
  void GetWhatsNewModules(GetWhatsNewModulesCallback callback) override;
  void GetWhatsNewEditions(GetWhatsNewEditionsCallback callback) override;
  void ClearWhatsNewData(ClearWhatsNewDataCallback callback) override;
  void LaunchWhatsNewStaging() override;

 private:
  raw_ptr<content::WebUI> web_ui_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  mojo::Receiver<
      mojom::user_education_internals::UserEducationInternalsPageHandler>
      receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_
