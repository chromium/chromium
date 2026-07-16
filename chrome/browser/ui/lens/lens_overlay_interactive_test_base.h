// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_INTERACTIVE_TEST_BASE_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_INTERACTIVE_TEST_BASE_H_

#include <utility>

#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "ui/gfx/geometry/point.h"

class PrefService;
class TemplateURLService;

// A test AimEligibilityService that returns a fixed eligibility value.
class TestingAimEligibilityService : public ChromeAimEligibilityService {
 public:
  explicit TestingAimEligibilityService(
      bool is_aim_eligible,
      bool is_cobrowse_eligible,
      PrefService& pref_service,
      TemplateURLService* template_url_service);

  ~TestingAimEligibilityService() override;

  variations::VariationsService* GetVariationsService() const override;

  bool IsAimEligible() const override;
  bool IsCobrowseServerEligible() const override;
  bool IsCobrowseEligible() const override;

 private:
  bool is_aim_eligible_;
  bool is_cobrowse_eligible_;
};

class TestingContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService {
 public:
  TestingContextualTasksUiService(
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service,
      std::unique_ptr<contextual_tasks::ContextualTasksCookieSynchronizer>
          cookie_synchronizer);
  ~TestingContextualTasksUiService() override;

  bool CookieJarContainsPrimaryAccount() override;

  void SetCookieJarContainsPrimaryAccount(bool contains);

 private:
  bool cookie_jar_contains_primary_account_ = true;
};

extern const char kDocumentWithNamedElement[];
extern const char kDocumentWithImage[];
extern const char kDocumentWithVideo[];
extern const char kPdfDocument[];

class LensOverlayInteractiveTestBase : public InteractiveFeaturePromoTest {
 public:
  template <typename... Args>
  explicit LensOverlayInteractiveTestBase(Args&&... args)
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos({std::forward<Args>(args)...})) {
    lens_search_controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating([](tabs::TabInterface& tab) {
              return std::make_unique<lens::TestLensSearchController>(&tab);
            }));
  }
  ~LensOverlayInteractiveTestBase() override;

  void SetUp() override;
  virtual void SetUpFeatureList();
  void WaitForTemplateURLServiceToLoad();
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  InteractiveTestApi::MultiStep OpenArbitraryNewTab();
  InteractiveTestApi::MultiStep OpenLensOverlay();
  InteractiveTestApi::MultiStep OpenLensOverlayFromImage();
  virtual InteractiveTestApi::MultiStep OpenLensOverlayFromVideo();

  InteractiveTestApi::MultiStep WaitForScreenshotRendered(
      ui::ElementIdentifier overlayId);
  InteractiveTestApi::MultiStep FinishScreenshotUpload(int tab_id = 0);

  InteractiveTestApi::MultiStep OpenLensOverlayWithRegionSearch(
      ui::ElementIdentifier tab_id,
      ui::ElementIdentifier overlay_id,
      base::OnceCallback<gfx::Point()> target_point,
      int tab_id_int = 0);

  bool TriggerLenOverlayHomeworkPageAction();

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  ui::UserDataFactory::ScopedOverride lens_search_controller_override_;
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_INTERACTIVE_TEST_BASE_H_
