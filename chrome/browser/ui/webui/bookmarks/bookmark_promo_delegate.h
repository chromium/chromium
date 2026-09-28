// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARK_PROMO_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARK_PROMO_DELEGATE_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/local_data_description.h"
#include "google_apis/gaia/gaia_id.h"

class BatchUploadService;
class BrowserWindowInterface;
class PrefService;

namespace signin {
class AccountPreviewDataService;
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// LINT.IfChange(BookmarkPromoType)
enum class BookmarkPromoType {
  kNone = 0,
  kBatchUpload = 1,
  kAccountAwareSignIn = 2,
};
// LINT.ThenChange(//chrome/browser/resources/bookmarks/constants.ts:BookmarkPromoType)

struct BookmarkPromoData {
  BookmarkPromoData();
  ~BookmarkPromoData();
  BookmarkPromoData(const BookmarkPromoData&);
  BookmarkPromoData& operator=(const BookmarkPromoData&);
  BookmarkPromoData(BookmarkPromoData&&);
  BookmarkPromoData& operator=(BookmarkPromoData&&);

  BookmarkPromoType promo_type = BookmarkPromoType::kNone;
  bool can_show = false;
  std::u16string promo_title;
  std::u16string promo_subtitle;
  std::u16string action_button_text;
  std::string promo_avatar_url;

  base::DictValue ToDict() const;
};

// Abstract interface for promos displayed in the Bookmarks Manager promo card.
class BookmarkPromoDelegate {
 public:
  using PromoDataCallback = base::OnceCallback<void(BookmarkPromoData)>;

  virtual ~BookmarkPromoDelegate() = default;

  // Returns whether this promo delegate is currently eligible to show a promo.
  virtual bool CanShowPromo() const = 0;

  // Computes the promo data and invokes `callback`.
  virtual void GetPromoData(PromoDataCallback callback) const = 0;

  // Invoked when the promo card is shown to the user.
  virtual void OnPromoShown() {}

  // Invoked when the user clicks the primary action button on the promo card.
  virtual void OnPromoClicked(BrowserWindowInterface* browser_window) = 0;

  // Invoked when the user dismisses the promo card.
  virtual void OnPromoDismissed() = 0;
};

// Delegate for the Batch Upload promo card (shown to signed-in users with local
// bookmarks).
class BatchUploadPromoDelegate : public BookmarkPromoDelegate {
 public:
  BatchUploadPromoDelegate(PrefService* pref_service,
                           signin::IdentityManager* identity_manager,
                           syncer::SyncService* sync_service,
                           BatchUploadService* batch_upload_service);
  BatchUploadPromoDelegate(const BatchUploadPromoDelegate&) = delete;
  BatchUploadPromoDelegate& operator=(const BatchUploadPromoDelegate&) = delete;
  ~BatchUploadPromoDelegate() override;

  // BookmarkPromoDelegate:
  bool CanShowPromo() const override;
  void GetPromoData(PromoDataCallback callback) const override;
  void OnPromoClicked(BrowserWindowInterface* browser_window) override;
  void OnPromoDismissed() override;

 private:
  void OnLocalDataDescriptionsReceived(
      PromoDataCallback callback,
      std::map<syncer::DataType, syncer::LocalDataDescription> local_data)
      const;

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<BatchUploadService> batch_upload_service_;
  base::WeakPtrFactory<BatchUploadPromoDelegate> weak_ptr_factory_{this};
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Delegate for the Account-Aware Sign-In promo card (shown to web-only
// signed-in users whose preferred promo account has bookmark preview data).
class AccountAwareSignInPromoDelegate : public BookmarkPromoDelegate {
 public:
  AccountAwareSignInPromoDelegate(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      signin::AccountPreviewDataService* account_preview_data_service);
  AccountAwareSignInPromoDelegate(const AccountAwareSignInPromoDelegate&) =
      delete;
  AccountAwareSignInPromoDelegate& operator=(
      const AccountAwareSignInPromoDelegate&) = delete;
  ~AccountAwareSignInPromoDelegate() override;

  // BookmarkPromoDelegate:
  bool CanShowPromo() const override;
  void GetPromoData(PromoDataCallback callback) const override;
  void OnPromoShown() override;
  void OnPromoClicked(BrowserWindowInterface* browser_window) override;
  void OnPromoDismissed() override;

 private:
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<signin::AccountPreviewDataService>
      account_preview_data_service_;
  GaiaId shown_for_gaia_id_;
  bool dismissed_in_session_ = false;
};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#endif  // CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARK_PROMO_DELEGATE_H_
