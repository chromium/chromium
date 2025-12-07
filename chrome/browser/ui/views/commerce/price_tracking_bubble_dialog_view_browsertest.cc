
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace {
const char kTestURL[] = "http://www.google.com";
}  // namespace

class PriceTrackingBubbleDialogViewBrowserTest : public DialogBrowserTest {
 public:
  PriceTrackingBubbleDialogViewBrowserTest() = default;
  PriceTrackingBubbleDialogViewBrowserTest(
      const PriceTrackingBubbleDialogViewBrowserTest&) = delete;
  PriceTrackingBubbleDialogViewBrowserTest& operator=(
      const PriceTrackingBubbleDialogViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    signin::ConsentLevel consent_level =
        base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
            ? signin::ConsentLevel::kSignin
            : signin::ConsentLevel::kSync;
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(get_profile()), "test@email.com",
        consent_level);

    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(get_profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    // If we are not syncing, we need to add account nodes in order to use the
    // price tracking service.
    if (consent_level == signin::ConsentLevel::kSignin) {
      bookmark_model->CreateAccountPermanentFolders();
    }

    // Enable anonymized data collection.
    PrefService* prefs = get_profile()->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  void ShowUi(const std::string& name) override {
    if (name == "FUEBubble_NoExistingBookmark") {
      CreateBubbleViewAndShow(
          PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);
    } else if (name == "FUEBubble_hasBookmarked") {
      CreateBubbleViewAndShow(
          PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE,
          std::optional<std::u16string>(u"Other bookmark"));
    } else if (name == "NormalBubble_WithoutBookmarkCreation") {
      CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);
    } else if (name == "NormalBubble_WithBookmarkCreation") {
      CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type::TYPE_NORMAL,
                              std::optional<std::u16string>(u"Shopping list"));
    }
  }

  void CreateBubbleViewAndShow(
      PriceTrackingBubbleDialogView::Type type,
      std::optional<std::u16string> bookmark_folder_name = std::nullopt) {
    views::View* const anchor_view =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();
    // Create the coordinator using the anchor view.
    // The coordinator manages the bubble's lifecycle.
    coordinator_ =
        std::make_unique<PriceTrackingBubbleCoordinator>(anchor_view);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);

    coordinator_->Show(
        web_contents(), get_profile(), GURL(kTestURL),
        ui::ImageModel::FromImage(
            gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap))),
        base::DoNothing(), base::DoNothing(), type, bookmark_folder_name);
  }

  void TearDownOnMainThread() override {
    coordinator_.reset();
    DialogBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* get_profile() { return chrome_test_utils::GetProfile(this); }

  std::unique_ptr<PriceTrackingBubbleCoordinator> coordinator_;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
#endif
};

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleDialogViewBrowserTest,
                       InvokeUi_FUEBubble_NoExistingBookmark) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleDialogViewBrowserTest,
                       InvokeUi_FUEBubble_hasBookmarked) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleDialogViewBrowserTest,
                       InvokeUi_NormalBubble_WithoutBookmarkCreation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PriceTrackingBubbleDialogViewBrowserTest,
                       InvokeUi_NormalBubble_WithBookmarkCreation) {
  ShowAndVerifyUi();
}
