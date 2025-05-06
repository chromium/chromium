// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_message_handler.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"
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

namespace {

constexpr int kBatchUploadBookmarkPromoMaxDismissCount = 3;
constexpr base::TimeDelta
    kBatchUploadBookmarkPromoMinimumDelayToShowAfterDismiss = base::Days(7);

GaiaId GetPrimaryAccountGaiaId(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
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
                                          int local_bookmark_count) {
  base::Value::Dict promo_data;
  promo_data.Set("canShow", can_show);
  promo_data.Set("localBookmarksCount", local_bookmark_count);
  return promo_data;
}

// Return an empty result; should not show the promo.
base::Value::Dict GetEmptyBatchUploadPromoData() {
  return GetBatchUploadPromoData(/*can_show=*/false,
                                 /*local_bookmark_count=*/0);
}

base::Value::Dict GetBatchUploadDataFromProfileAndLocalData(
    Profile* profile,
    const std::map<syncer::DataType, syncer::LocalDataDescription>&
        local_data) {
  int local_bookmark_count =
      local_data.contains(syncer::BOOKMARKS)
          ? local_data.at(syncer::BOOKMARKS).local_data_models.size()
          : 0;
  bool can_show = local_bookmark_count != 0 && CanShowBatchUploadPromo(profile);
  return GetBatchUploadPromoData(can_show, local_bookmark_count);
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
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::BindRepeating(&BookmarksMessageHandler::UpdateIncognitoAvailability,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::BindRepeating(&BookmarksMessageHandler::UpdateCanEditBookmarks,
                          base::Unretained(this)));

  identity_manager_observation_.Observe(
      IdentityManagerFactory::GetForProfile(profile));
  sync_service_observation_.Observe(SyncServiceFactory::GetForProfile(profile));
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
  std::string id = args[1].GetString();

  AllowJavascript();

  ResolveJavascriptCallback(callback_id,
                            base::Value(CanUploadBookmarkToAccountStorage(id)));
}

void BookmarksMessageHandler::HandleSingleUploadClicked(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  std::string id_string = args[0].GetString();
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

  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);

  // If the dialog is accepted, move it to the permanent account node
  // corresponding to the permanent local node it is saved under.
  const bookmarks::BookmarkPermanentNode* parent_node = nullptr;
  if (node->HasAncestor(model->other_node())) {
    parent_node = model->account_other_node();
  } else if (node->HasAncestor(model->bookmark_bar_node())) {
    parent_node = model->account_bookmark_bar_node();
  } else if (node->HasAncestor(model->mobile_node())) {
    parent_node = model->account_mobile_node();
  }
  CHECK(parent_node);

  // Show the dialog asking the user to confirm their choice to move the
  // bookmark.
  ShowBookmarkAccountStorageMoveDialog(
      chrome::FindLastActiveWithProfile(profile), node, parent_node,
      parent_node->children().size(),
      BookmarkAccountStorageMoveDialogType::kUpload);
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

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  sync_service->GetLocalDataDescriptions(
      {syncer::BOOKMARKS},
      base::BindOnce(
          &BookmarksMessageHandler::OnGetLocalDataDescriptionReceived,
          weak_ptr_factory_.GetWeakPtr(), callback_id.Clone()));
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

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  sync_service->GetLocalDataDescriptions(
      {syncer::BOOKMARKS},
      base::BindOnce(
          &BookmarksMessageHandler::FireOnGetLocalDataDescriptionReceived,
          weak_ptr_factory_.GetWeakPtr()));
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
  }
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
