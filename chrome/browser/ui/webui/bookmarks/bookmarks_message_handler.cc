// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_message_handler.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr int kBatchUploadBookmarkPromoMaxDismissCount = 3;
constexpr base::TimeDelta
    kBatchUploadBookmarkPromoMinimumDelayToShowAfterDismiss = base::Days(7);

GaiaId GetPrimaryAccountGaiaId(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  // Identity manager is null in incognito mode.
  if (!identity_manager) {
    return GaiaId();
  }
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .gaia;
}

// Computes whether the promo can be shown based on previous occurrences of it
// being shown.
bool CanShowBatchUploadPromo(Profile* profile) {
  GaiaId gaia_id = GetPrimaryAccountGaiaId(profile);
  if (gaia_id.empty()) {
    return false;
  }

  auto [dismiss_count, last_dismiss_time] =
      SigninPrefs(*profile->GetPrefs())
          .GetBookmarkBatchUploadPromoDismissCountWithLastTime(gaia_id);

  if (dismiss_count > kBatchUploadBookmarkPromoMaxDismissCount) {
    return false;
  }

  // If no dismiss were recorded yet, then the promo can always be shown.
  // Otherwise, we can only show the promo if a minimum duration has passed
  // since the last dismiss time.
  return !last_dismiss_time.has_value() ||
         (base::Time::Now() - last_dismiss_time.value() >
          kBatchUploadBookmarkPromoMinimumDelayToShowAfterDismiss);
}

base::Value::Dict GetBatchUploadPromoData(bool can_show,
                                          int local_bookmark_count,
                                          bool has_non_bookmark_local_data) {
  base::Value::Dict promo_data;
  promo_data.Set("canShow", can_show);
#if !BUILDFLAG(IS_CHROMEOS)
  promo_data.Set("promoSubtitle",
                 l10n_util::GetPluralStringFUTF16(
                     has_non_bookmark_local_data
                         ? IDS_BATCH_UPLOAD_PROMO_SUBTITLE_BOOKMARKS_COMBO
                         : IDS_BATCH_UPLOAD_PROMO_SUBTITLE_BOOKMARKS,
                     local_bookmark_count));
#endif
  return promo_data;
}

// Return an empty result; should not show the promo.
base::Value::Dict GetEmptyBatchUploadPromoData() {
  return GetBatchUploadPromoData(/*can_show=*/false,
                                 /*local_bookmark_count=*/0,
                                 /*has_non_bookmark_local_data=*/false);
}

base::Value::Dict GetBatchUploadDataFromProfileAndLocalData(
    Profile* profile,
    const std::map<syncer::DataType, syncer::LocalDataDescription>&
        local_data) {
  int local_bookmark_count =
      local_data.contains(syncer::BOOKMARKS)
          ? local_data.at(syncer::BOOKMARKS).local_data_models.size()
          : 0;

  bool has_non_bookmark_local_data = std::ranges::any_of(
      local_data, [](const std::pair<syncer::DataType,
                                     syncer::LocalDataDescription>& data) {
        return data.first != syncer::BOOKMARKS &&
               !data.second.local_data_models.empty();
      });

  bool can_show = local_bookmark_count != 0 && CanShowBatchUploadPromo(profile);
  return GetBatchUploadPromoData(can_show, local_bookmark_count,
                                 has_non_bookmark_local_data);
}

}  // namespace

BookmarksMessageHandler::BookmarksMessageHandler() = default;

BookmarksMessageHandler::~BookmarksMessageHandler() = default;

void BookmarksMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getIncognitoAvailability",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleGetIncognitoAvailability,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCanEditBookmarks",
      base::BindRepeating(&BookmarksMessageHandler::HandleGetCanEditBookmarks,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCanUploadBookmarkToAccountStorage",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleGetCanUploadBookmarkToAccountStorage,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onSingleBookmarkUploadClicked",
      base::BindRepeating(&BookmarksMessageHandler::HandleSingleUploadClicked,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getBatchUploadPromoInfo",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleGetBatchUploadPromoData,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onBatchUploadPromoClicked",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleOnBatchUploadPromoClicked,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onBatchUploadPromoDismissed",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleOnBatchUploadPromoDismissed,
          base::Unretained(this)));
}

void BookmarksMessageHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(!profile->IsGuestSession(),
        base::NotFatalUntil(base::NotFatalUntil::M140));
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::BindRepeating(&BookmarksMessageHandler::UpdateIncognitoAvailability,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::BindRepeating(&BookmarksMessageHandler::UpdateCanEditBookmarks,
                          base::Unretained(this)));

  // Identity manager is null in incognito mode.
  if (auto* identtiy_manager = IdentityManagerFactory::GetForProfile(profile)) {
    identity_manager_observation_.Observe(identtiy_manager);
  }
  // Sync Service is null in incognito mode.
  if (auto* sync_service = SyncServiceFactory::GetForProfile(profile)) {
    sync_service_observation_.Observe(sync_service);
  }
  bookmark_model_observation_.Observe(
      BookmarkModelFactory::GetForBrowserContext(profile));
}

void BookmarksMessageHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();
  identity_manager_observation_.Reset();
  sync_service_observation_.Reset();
  bookmark_model_observation_.Reset();
}

int BookmarksMessageHandler::GetIncognitoAvailability() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetInteger(policy::policy_prefs::kIncognitoModeAvailability);
}

void BookmarksMessageHandler::HandleGetIncognitoAvailability(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();

  ResolveJavascriptCallback(callback_id,
                            base::Value(GetIncognitoAvailability()));
}

void BookmarksMessageHandler::UpdateIncognitoAvailability() {
  FireWebUIListener("incognito-availability-changed",
                    base::Value(GetIncognitoAvailability()));
}

bool BookmarksMessageHandler::CanEditBookmarks() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
}

void BookmarksMessageHandler::HandleGetCanEditBookmarks(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();

  ResolveJavascriptCallback(callback_id, base::Value(CanEditBookmarks()));
}

bool BookmarksMessageHandler::CanUploadBookmarkToAccountStorage(
    const std::string& id_string) {
  int64_t id;

  // Check if the bookmark's id is valid.
  if (!base::StringToInt64(id_string, &id)) {
    return false;
  }

  // Do not proceed if bookmarks cannot be edited.
  if (!CanEditBookmarks()) {
    return false;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  // Incognito profile should not show the upload button. The action is
  // possible, but it should not be promoted.
  if (profile->IsOffTheRecord()) {
    return false;
  }

  // Identity manager should always be valid since Incognito and Guest mode are
  // filtered out above.
  // Only signed in users may see the upload button.
  if (signin_util::GetSignedInState(IdentityManagerFactory::GetForProfile(
          profile)) != signin_util::SignedInState::kSignedIn) {
    return false;
  }

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);

  // Do not proceed if the bookmark does not exist.
  if (!node) {
    return false;
  }

  // Do not proceed if the node is a permanent node.
  if (model->is_permanent_node(node)) {
    return false;
  }

  // Do not proceed if the user is not using account storage.
  if (!model->account_other_node()) {
    return false;
  }

  // Do not proceed if the bookmark is managed.
  if (ManagedBookmarkServiceFactory::GetForProfile(profile)->IsNodeManaged(
          node)) {
    return false;
  }

  // Do not proceed if the bookmark is already in the account storage, or if the
  // user is syncing.
  if (!model->IsLocalOnlyNode(*node)) {
    return false;
  }

  return true;
}

void BookmarksMessageHandler::HandleGetCanUploadBookmarkToAccountStorage(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& id = args[1].GetString();

  AllowJavascript();

  ResolveJavascriptCallback(callback_id,
                            base::Value(CanUploadBookmarkToAccountStorage(id)));
}

void BookmarksMessageHandler::HandleSingleUploadClicked(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& id_string = args[0].GetString();
  int64_t id;
  base::StringToInt64(id_string, &id);

  Profile* profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  // Do not continue if account nodes are no longer available. This can happen
  // if the user signs out and the UI is not updated properly.
  // TODO(crbug.com/413637312): Remove this once the icon is no longer visible
  // upon sign out.
  if (!model->account_other_node()) {
    return;
  }

  // All conditions for uploading to account storage should be met at this
  // point.
  CHECK(CanUploadBookmarkToAccountStorage(id_string));

  // Show the dialog asking the user to confirm their choice to move the
  // bookmark.
  ShowBookmarkAccountStorageUploadDialog(
      chrome::FindLastActiveWithProfile(profile),
      bookmarks::GetBookmarkNodeByID(model, id));
}

void BookmarksMessageHandler::UpdateCanEditBookmarks() {
  FireWebUIListener("can-edit-bookmarks-changed",
                    base::Value(CanEditBookmarks()));
}

void BookmarksMessageHandler::HandleGetBatchUploadPromoData(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  Profile* profile = Profile::FromWebUI(web_ui());
  if (!SyncServiceFactory::IsSyncAllowed(profile) ||
      !CanShowBatchUploadPromo(profile) || !CanEditBookmarks()) {
    ResolveJavascriptCallback(callback_id, GetEmptyBatchUploadPromoData());
    return;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(profile);
  CHECK(batch_upload);
  batch_upload->GetLocalDataDescriptionsForAvailableTypes(base::BindOnce(
      &BookmarksMessageHandler::OnGetLocalDataDescriptionReceived,
      weak_ptr_factory_.GetWeakPtr(), callback_id.Clone()));
#endif
}

void BookmarksMessageHandler::OnGetLocalDataDescriptionReceived(
    base::Value callback_id,
    std::map<syncer::DataType, syncer::LocalDataDescription> local_data) {
  ResolveJavascriptCallback(callback_id,
                            GetBatchUploadDataFromProfileAndLocalData(
                                Profile::FromWebUI(web_ui()), local_data));
}

void BookmarksMessageHandler::FireOnGetLocalDataDescriptionReceived(
    std::map<syncer::DataType, syncer::LocalDataDescription> local_data) {
  FireWebUIListener("batch-upload-promo-info-updated",
                    GetBatchUploadDataFromProfileAndLocalData(
                        Profile::FromWebUI(web_ui()), local_data));
}

void BookmarksMessageHandler::RequestLocalDataDescriptionsUpdate() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!SyncServiceFactory::IsSyncAllowed(profile) ||
      !CanShowBatchUploadPromo(profile) || !CanEditBookmarks()) {
    FireOnGetLocalDataDescriptionReceived(/*data=*/{});
    return;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(profile);
  CHECK(batch_upload);
  batch_upload->GetLocalDataDescriptionsForAvailableTypes(base::BindOnce(
      &BookmarksMessageHandler::FireOnGetLocalDataDescriptionReceived,
      weak_ptr_factory_.GetWeakPtr()));
#endif
}

void BookmarksMessageHandler::HandleOnBatchUploadPromoClicked(
    const base::Value::List& args) {
#if !BUILDFLAG(IS_CHROMEOS)
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(CanEditBookmarks());
  CHECK(SyncServiceFactory::IsSyncAllowed(profile));
  CHECK(CanShowBatchUploadPromo(profile));

  BatchUploadService* service =
      BatchUploadServiceFactory::GetForProfile(profile);
  CHECK(service);
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  service->OpenBatchUpload(
      browser, BatchUploadService::EntryPoint::kBookmarksManagerPromoCard);
#endif
}

void BookmarksMessageHandler::HandleOnBatchUploadPromoDismissed(
    const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  GaiaId gaia_id = GetPrimaryAccountGaiaId(profile);
  CHECK(!gaia_id.empty());
  SigninPrefs(*profile->GetPrefs())
      .IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(gaia_id);
}

void BookmarksMessageHandler::OnRefreshTokensLoaded() {
  RequestLocalDataDescriptionsUpdate();
}

void BookmarksMessageHandler::OnStateChanged(
    syncer::SyncService* sync_service) {
  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::CONFIGURING) {
    RequestLocalDataDescriptionsUpdate();

    // Check if the bookmark sync state has changed.
    const bool new_active_state =
        sync_service->GetActiveDataTypes().Has(syncer::BOOKMARKS);
    if (is_bookmarks_sync_active_ != new_active_state) {
      is_bookmarks_sync_active_ = new_active_state;
      FireWebUIListener("bookmarks-sync-state-changed");
    }
  }
}

void BookmarksMessageHandler::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  // Unreachable, since this class is tied to UI which gets destroyed before the
  // Profile and its KeyedServices.
  NOTREACHED();
}

void BookmarksMessageHandler::ExtensiveBookmarkChangesBeginning() {
  batch_updates_ongoing_ = true;
}

void BookmarksMessageHandler::ExtensiveBookmarkChangesEnded() {
  batch_updates_ongoing_ = false;

  if (need_local_count_update_) {
    RequestLocalDataDescriptionsUpdate();
    need_local_count_update_ = false;
  }
}

void BookmarksMessageHandler::BookmarkModelLoaded(bool ids_reassigned) {
  RequestLocalDataDescriptionsUpdate();
}

void BookmarksMessageHandler::RequestUpdateOrWaitForBatchUpdateEnd() {
  if (batch_updates_ongoing_) {
    need_local_count_update_ = true;
  } else {
    RequestLocalDataDescriptionsUpdate();
  }
}

void BookmarksMessageHandler::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(Profile::FromWebUI(web_ui()));
  // Only treat bookmarks that were moved from local to account storages.
  if (model->IsLocalOnlyNode(*old_parent) &&
      !model->IsLocalOnlyNode(*new_parent)) {
    RequestUpdateOrWaitForBatchUpdateEnd();
  }
}

void BookmarksMessageHandler::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked,
    const base::Location& location) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(Profile::FromWebUI(web_ui()));
  // Only attempt to request an update if a local node is removed.
  if (model->IsLocalOnlyNode(*node)) {
    RequestUpdateOrWaitForBatchUpdateEnd();
  }
}

void BookmarksMessageHandler::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(Profile::FromWebUI(web_ui()));
  // Only attempt to request an update if the added node is local.
  if (model->IsLocalOnlyNode(*parent->children()[index].get())) {
    RequestUpdateOrWaitForBatchUpdateEnd();
  }
}
