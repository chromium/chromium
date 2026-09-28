// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_message_handler.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/tabs/public/tab_interface.h"

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
      "getPromoData",
      base::BindRepeating(&BookmarksMessageHandler::HandleGetPromoData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onPromoShown",
      base::BindRepeating(&BookmarksMessageHandler::HandleOnPromoShown,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onPromoClicked",
      base::BindRepeating(&BookmarksMessageHandler::HandleOnPromoClicked,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onPromoDismissed",
      base::BindRepeating(&BookmarksMessageHandler::HandleOnPromoDismissed,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openBookmarks",
      base::BindRepeating(&BookmarksMessageHandler::HandleOpenBookmarks,
                          base::Unretained(this)));
}

void BookmarksMessageHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  CHECK(!profile->IsGuestSession(),
        base::NotFatalUntil(base::NotFatalUntil::M140));
  EnsurePromoDelegatesInitialized();

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::BindRepeating(&BookmarksMessageHandler::UpdateIncognitoAvailability,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::BindRepeating(&BookmarksMessageHandler::UpdateCanEditBookmarks,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kAccountPreviewPreference,
      base::BindRepeating(&BookmarksMessageHandler::RequestPromoDataUpdate,
                          base::Unretained(this)));

  // Identity manager is null in incognito mode.
  if (auto* identity_manager = IdentityManagerFactory::GetForProfile(profile)) {
    identity_manager_observation_.Observe(identity_manager);
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
  active_promo_delegate_ = nullptr;
  promo_delegates_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void BookmarksMessageHandler::EnsurePromoDelegatesInitialized() {
  if (!promo_delegates_.empty()) {
    return;
  }
  // Currently, the registered promo delegates are mutually exclusive (e.g., by
  // sign-in state). If non-mutually-exclusive promos are added in the future,
  // a sequential fallback mechanism (evaluating the next delegate when an
  // earlier delegate asynchronously resolves `can_show` to false) will be
  // needed to enforce promo priority.
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* pref_service = profile->GetPrefs();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::IsSyncAllowed(profile)
          ? SyncServiceFactory::GetForProfile(profile)
          : nullptr;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  promo_delegates_.push_back(std::make_unique<AccountAwareSignInPromoDelegate>(
      pref_service, identity_manager, sync_service,
      AccountPreviewDataServiceFactory::GetForProfile(profile)));
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  promo_delegates_.push_back(std::make_unique<BatchUploadPromoDelegate>(
      pref_service, identity_manager, sync_service,
      BatchUploadServiceFactory::GetForProfile(profile)));
}

void BookmarksMessageHandler::UpdateActivePromoDelegate() {
  active_promo_delegate_ = nullptr;
  if (!CanEditBookmarks()) {
    return;
  }
  EnsurePromoDelegatesInitialized();
  for (const auto& delegate : promo_delegates_) {
    if (delegate->CanShowPromo()) {
      active_promo_delegate_ = delegate.get();
      return;
    }
  }
}

int BookmarksMessageHandler::GetIncognitoAvailability() {
  // `IncognitoModePrefs::GetAvailability()` resolves the availability of both
  // standard Incognito mode and Enterprise Isolated Mode, which replaces
  // Incognito and takes precedence over the IncognitoModeAvailability
  // preference.
  return static_cast<int>(
      IncognitoModePrefs::GetAvailability(Profile::FromWebUI(web_ui())));
}

void BookmarksMessageHandler::HandleGetIncognitoAvailability(
    const base::ListValue& args) {
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
    const base::ListValue& args) {
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
    const base::ListValue& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& id = args[1].GetString();

  AllowJavascript();

  ResolveJavascriptCallback(callback_id,
                            base::Value(CanUploadBookmarkToAccountStorage(id)));
}

void BookmarksMessageHandler::HandleSingleUploadClicked(
    const base::ListValue& args) {
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
  BrowserWindowInterface* const browser =
      ProfileBrowserCollection::GetForProfile(profile)->GetLastActiveBrowser();
  ShowBookmarkAccountStorageUploadDialog(
      browser, bookmarks::GetBookmarkNodeByID(model, id));
}

void BookmarksMessageHandler::UpdateCanEditBookmarks() {
  FireWebUIListener("can-edit-bookmarks-changed",
                    base::Value(CanEditBookmarks()));
  RequestPromoDataUpdate();
}

void BookmarksMessageHandler::HandleGetPromoData(const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  UpdateActivePromoDelegate();
  if (!active_promo_delegate_) {
    ResolveJavascriptCallback(callback_id, BookmarkPromoData().ToDict());
    return;
  }

  BookmarkPromoDelegate* delegate = active_promo_delegate_.get();
  delegate->GetPromoData(base::BindOnce(
      &BookmarksMessageHandler::OnPromoDataReceived,
      weak_ptr_factory_.GetWeakPtr(), callback_id.Clone(), delegate));
}

void BookmarksMessageHandler::OnPromoDataReceived(
    base::Value callback_id,
    const BookmarkPromoDelegate* delegate,
    BookmarkPromoData promo_data) {
  if (delegate != active_promo_delegate_.get()) {
    return;
  }
  if (!promo_data.can_show) {
    active_promo_delegate_ = nullptr;
  }
  ResolveJavascriptCallback(callback_id, promo_data.ToDict());
}

void BookmarksMessageHandler::FirePromoDataUpdated(
    const BookmarkPromoDelegate* delegate,
    BookmarkPromoData promo_data) {
  if (delegate != active_promo_delegate_.get()) {
    return;
  }
  if (!promo_data.can_show) {
    active_promo_delegate_ = nullptr;
  }
  FireWebUIListener("promo-data-updated", promo_data.ToDict());
}

void BookmarksMessageHandler::RequestPromoDataUpdate() {
  UpdateActivePromoDelegate();
  if (!active_promo_delegate_) {
    FirePromoDataUpdated(/*delegate=*/nullptr, BookmarkPromoData());
    return;
  }

  BookmarkPromoDelegate* delegate = active_promo_delegate_.get();
  delegate->GetPromoData(
      base::BindOnce(&BookmarksMessageHandler::FirePromoDataUpdated,
                     weak_ptr_factory_.GetWeakPtr(), delegate));
}

void BookmarksMessageHandler::HandleOnPromoShown(const base::ListValue& args) {
  if (!CanEditBookmarks() || !active_promo_delegate_) {
    return;
  }
  active_promo_delegate_->OnPromoShown();
}

void BookmarksMessageHandler::HandleOnPromoClicked(
    const base::ListValue& args) {
  if (!CanEditBookmarks() || !active_promo_delegate_) {
    RequestPromoDataUpdate();
    return;
  }
  auto* tab =
      tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
  BrowserWindowInterface* browser_window =
      tab ? tab->GetBrowserWindowInterface()
          : ProfileBrowserCollection::GetForProfile(
                Profile::FromWebUI(web_ui()))
                ->GetLastActiveBrowser();
  if (!browser_window) {
    return;
  }
  active_promo_delegate_->OnPromoClicked(browser_window);
}

void BookmarksMessageHandler::HandleOnPromoDismissed(
    const base::ListValue& args) {
  if (!active_promo_delegate_) {
    return;
  }
  active_promo_delegate_->OnPromoDismissed();
  active_promo_delegate_ = nullptr;
}

void BookmarksMessageHandler::OnRefreshTokensLoaded() {
  RequestPromoDataUpdate();
}

void BookmarksMessageHandler::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  RequestPromoDataUpdate();
}

void BookmarksMessageHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  RequestPromoDataUpdate();
}

void BookmarksMessageHandler::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  RequestPromoDataUpdate();
}

void BookmarksMessageHandler::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  RequestPromoDataUpdate();
}

void BookmarksMessageHandler::OnStateChanged(
    syncer::SyncService* sync_service) {
  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::CONFIGURING) {
    RequestPromoDataUpdate();

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
  FireWebUIListener("import-began");
}

void BookmarksMessageHandler::ExtensiveBookmarkChangesEnded() {
  batch_updates_ongoing_ = false;

  if (need_promo_data_update_) {
    RequestPromoDataUpdate();
    need_promo_data_update_ = false;
  }
  FireWebUIListener("import-ended");
}

void BookmarksMessageHandler::BookmarkModelLoaded(bool ids_reassigned) {
  RequestPromoDataUpdate();
}

void BookmarksMessageHandler::RequestUpdateOrWaitForBatchUpdateEnd() {
  if (batch_updates_ongoing_) {
    need_promo_data_update_ = true;
  } else {
    RequestPromoDataUpdate();
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

void BookmarksMessageHandler::HandleOpenBookmarks(const base::ListValue& args) {
  CHECK_GE(args.size(), 3U);
  const base::ListValue& uuid_list = args[0].GetList();
  WindowOpenDisposition disposition =
      static_cast<WindowOpenDisposition>(args[1].GetInt());
  bookmarks::OpenAllBookmarksContext context =
      static_cast<bookmarks::OpenAllBookmarksContext>(args[2].GetInt());

  Profile* profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>> nodes;
  for (const auto& val : uuid_list) {
    const std::string& uuid_str = val.GetString();
    base::Uuid uuid = base::Uuid::ParseLowercase(uuid_str);
    if (!uuid.is_valid()) {
      continue;
    }
    const bookmarks::BookmarkNode* node = model->GetNodeByUuid(
        uuid, bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
    if (!node) {
      node = model->GetNodeByUuid(
          uuid, bookmarks::BookmarkModel::NodeTypeForUuidLookup::
                    kLocalOrSyncableNodes);
    }
    if (node) {
      nodes.push_back(node);
    }
  }

  if (nodes.empty()) {
    return;
  }

  auto* tab = tabs::TabInterface::GetFromContents(web_ui()->GetWebContents());
  if (!tab) {
    return;
  }
  auto* browser_window = tab->GetBrowserWindowInterface();
  if (!browser_window) {
    return;
  }

  bookmarks::OpenAllIfAllowed(browser_window, nodes, disposition, context);
}
