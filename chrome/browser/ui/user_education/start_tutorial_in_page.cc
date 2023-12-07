// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/start_tutorial_in_page.h"

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

class StartTutorialInPageImpl : public StartTutorialInPage {
 public:
  explicit StartTutorialInPageImpl(Browser* browser, Params params)
      : browser_(browser->AsWeakPtr()), callback_(std::move(params.callback)) {
    DCHECK(callback_);
    DCHECK(browser_);
    DCHECK(params.tutorial_id.has_value());
    tutorial_id_ = params.tutorial_id.value();

    auto* tutorial_service = GetTutorialService();
    if (!tutorial_service) {
      std::move(callback_).Run(tutorial_service);
      return;
    }

    auto context = GetUiElementContext();
    if (!context.has_value()) {
      std::move(callback_).Run(tutorial_service);
      return;
    }

    tutorial_service->StartTutorial(
        params.tutorial_id.value(), context.value(),
        base::BindOnce(&StartTutorialInPageImpl::OnTutorialCompleted,
                       GetWeakPtr()),
        base::BindOnce(&StartTutorialInPageImpl::OnTutorialAborted,
                       GetWeakPtr()));
    std::move(callback_).Run(tutorial_service);

    if (params.target_url.has_value()) {
      NavigateParams navigate_params(browser, params.target_url.value(),
                                     ui::PAGE_TRANSITION_LINK);
      // This does not work
      // Try the handle stuff from show_promo_in_page?
      navigate_params.disposition =
          params.overwrite_active_tab
              ? WindowOpenDisposition::CURRENT_TAB
              : WindowOpenDisposition::NEW_FOREGROUND_TAB;
      Navigate(&navigate_params);
    }
  }

  ~StartTutorialInPageImpl() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  base::WeakPtr<StartTutorialInPageImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::optional<ui::ElementContext> GetUiElementContext() {
    if (browser_) {
      return browser_->window()->GetElementContext();
    }
    return std::nullopt;
  }

  user_education::TutorialService* GetTutorialService() {
    if (browser_) {
      UserEducationService* const service =
          UserEducationServiceFactory::GetForBrowserContext(
              browser_->profile());
      if (service) {
        return &service->tutorial_service();
      }
    }
    return nullptr;
  }

  void OnTutorialCompleted() { delete this; }
  void OnTutorialAborted() { delete this; }

  const base::WeakPtr<Browser> browser_;
  user_education::TutorialIdentifier tutorial_id_;
  Callback callback_;
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<StartTutorialInPageImpl> weak_ptr_factory_{this};
};

}  // namespace

StartTutorialInPage::Params::Params() = default;
StartTutorialInPage::Params::Params(Params&& other) noexcept = default;
StartTutorialInPage::Params& StartTutorialInPage::Params::operator=(
    Params&& other) noexcept = default;
StartTutorialInPage::Params::~Params() = default;

StartTutorialInPage::StartTutorialInPage() = default;
StartTutorialInPage::~StartTutorialInPage() = default;

base::WeakPtr<StartTutorialInPage> StartTutorialInPage::Start(Browser* browser,
                                                              Params params) {
  return (new StartTutorialInPageImpl(browser, std::move(params)))
      ->GetWeakPtr();
}
