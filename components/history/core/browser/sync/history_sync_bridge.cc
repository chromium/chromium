// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_sync_bridge.h"

#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/sync/history_sync_metadata_database.h"
#include "components/history/core/browser/sync/visit_id_remapper.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/browser/visit_annotations_database.h"
#include "components/sync/base/page_transition_conversion.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "ui/base/page_transition_types.h"

namespace history {

namespace {

constexpr base::TimeDelta kMaxWriteToTheFuture = base::Days(2);

// Redirect chains have theoretically unbounded size, and in excessive cases
// they can become so large that the whole entity may fail to sync due to its
// size. Avoid even trying to commit such entities.
constexpr int kMaxRedirectsPerEntity = 10;

// Some pages embed the favicon image itself in the URL, using the data: scheme.
// These cases, or more generally any favicon URL that is unreasonably large,
// should simply be ignored, because it otherwise runs into the risk that the
// whole entity may fail to sync due to max size limits imposed by the sync
// server. And after all, the favicon is somewhat optional.
constexpr int kMaxFaviconUrlSizeToSync = 2048;

bool ShouldSync(const GURL& url) {
  // Note: Several types of uninteresting/undesired URLs are already excluded by
  // the history system itself via CanAddURLToHistory(). No need to exclude them
  // again here.
  // "file:", "filesystem:", or "blob:" URLs don't make sense to sync.
  if (url.SchemeIsFile() || url.SchemeIsFileSystem() || url.SchemeIsBlob()) {
    return false;
  }
  // "data:" URLs can be arbitrarily large, and thus shouldn't be synced.
  // (It's also questionable if it'd be at all useful to sync them.)
  if (url.SchemeIs(url::kDataScheme)) {
    return false;
  }
  return true;
}

std::string GetStorageKeyFromVisitRow(const VisitRow& row) {
  DCHECK(!row.visit_time.is_null());
  return HistorySyncMetadataDatabase::StorageKeyFromVisitTime(row.visit_time);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncHistoryDatabaseError)
enum class SyncHistoryDatabaseError {
  kApplyIncrementalSyncChangesAddSyncedVisit = 0,
  kApplyIncrementalSyncChangesWriteMetadata = 1,
  kOnDatabaseError = 2,
  kLoadMetadata = 3,
  // Deprecated (call sites were removed):
  // kOnURLVisitedGetVisit = 4,
  // kOnURLsDeletedReadMetadata = 5,
  // kOnVisitUpdatedGetURL = 6,
  kGetAllDataReadMetadata = 7,
  kMaxValue = kGetAllDataReadMetadata
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncHistoryDatabaseError)

void RecordDatabaseError(SyncHistoryDatabaseError error) {
  DLOG(ERROR) << "SyncHistoryBridge database error: "
              << static_cast<int>(error);
  base::UmaHistogramEnumeration("Sync.History.DatabaseError", error);
}

base::Time GetVisitTime(const sync_pb::HistorySpecifics& specifics) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.visit_time_windows_epoch_micros()));
}

sync_pb::SyncEnums::BrowserType BrowserTypeToProto(
    VisitContextAnnotations::BrowserType type) {
  switch (type) {
    case VisitContextAnnotations::BrowserType::kUnknown:
      return sync_pb::SyncEnums_BrowserType_BROWSER_TYPE_UNKNOWN;
    case VisitContextAnnotations::BrowserType::kTabbed:
      return sync_pb::SyncEnums_BrowserType_TYPE_TABBED;
    case VisitContextAnnotations::BrowserType::kPopup:
      return sync_pb::SyncEnums_BrowserType_TYPE_POPUP;
    case VisitContextAnnotations::BrowserType::kCustomTab:
      return sync_pb::SyncEnums_BrowserType_TYPE_CUSTOM_TAB;
    case VisitContextAnnotations::BrowserType::kAuthTab:
      return sync_pb::SyncEnums_BrowserType_TYPE_AUTH_TAB;
  }
  return sync_pb::SyncEnums_BrowserType_BROWSER_TYPE_UNKNOWN;
}

VisitContextAnnotations::BrowserType BrowserTypeFromProto(
    sync_pb::SyncEnums::BrowserType type) {
  switch (type) {
    case sync_pb::SyncEnums_BrowserType_BROWSER_TYPE_UNKNOWN:
      return VisitContextAnnotations::BrowserType::kUnknown;
    case sync_pb::SyncEnums_BrowserType_TYPE_TABBED:
      return VisitContextAnnotations::BrowserType::kTabbed;
    case sync_pb::SyncEnums_BrowserType_TYPE_POPUP:
      return VisitContextAnnotations::BrowserType::kPopup;
    case sync_pb::SyncEnums_BrowserType_TYPE_CUSTOM_TAB:
      return VisitContextAnnotations::BrowserType::kCustomTab;
    case sync_pb::SyncEnums_BrowserType_TYPE_AUTH_TAB:
      return VisitContextAnnotations::BrowserType::kAuthTab;
  }
  return VisitContextAnnotations::BrowserType::kUnknown;
}

VisitContentAnnotations::PasswordState PasswordStateFromProto(
    sync_pb::SyncEnums::PasswordState password_state) {
  switch (password_state) {
    case sync_pb::SyncEnums_PasswordState_PASSWORD_STATE_UNKNOWN:
      return VisitContentAnnotations::PasswordState::kUnknown;
    case sync_pb::SyncEnums_PasswordState_NO_PASSWORD_FIELD:
      return VisitContentAnnotations::PasswordState::kNoPasswordField;
    case sync_pb::SyncEnums_PasswordState_HAS_PASSWORD_FIELD:
      return VisitContentAnnotations::PasswordState::kHasPasswordField;
  }
  return VisitContentAnnotations::PasswordState::kUnknown;
}

sync_pb::SyncEnums::PasswordState PasswordStateToProto(
    VisitContentAnnotations::PasswordState password_state) {
  switch (password_state) {
    case VisitContentAnnotations::PasswordState::kUnknown:
      return sync_pb::SyncEnums_PasswordState_PASSWORD_STATE_UNKNOWN;
    case VisitContentAnnotations::PasswordState::kNoPasswordField:
      return sync_pb::SyncEnums_PasswordState_NO_PASSWORD_FIELD;
    case VisitContentAnnotations::PasswordState::kHasPasswordField:
      return sync_pb::SyncEnums_PasswordState_HAS_PASSWORD_FIELD;
  }
  return sync_pb::SyncEnums_PasswordState_PASSWORD_STATE_UNKNOWN;
}

// Creates a VisitRow out of a single redirect entry within the `specifics`.
// The `visit_id` and `url_id` will be unset; the HistoryBackend assigns those.
VisitRow MakeVisitRow(const sync_pb::HistorySpecifics& specifics,
                      int redirect_index) {
  DCHECK_GE(redirect_index, 0);
  DCHECK_LT(redirect_index, specifics.redirect_entries_size());

  VisitRow row;
  // Required fields: `visit_time` and `originator_cache_guid`.
  DCHECK_NE(specifics.visit_time_windows_epoch_micros(), 0);
  DCHECK(!specifics.originator_cache_guid().empty());
  row.visit_time = GetVisitTime(specifics);
  row.originator_cache_guid = specifics.originator_cache_guid();

  // The `originator_visit_id` should always exist for visits coming from modern
  // clients, but it may be missing in legacy visits (i.e. those from clients
  // committing history data via the SESSIONS data type).
  row.originator_visit_id =
      specifics.redirect_entries(redirect_index).originator_visit_id();

  // Definitionally, any visit from Sync is known to sync.
  row.is_known_to_sync = true;

  // Transfer app_id if present.
  if (specifics.has_app_id()) {
    row.app_id = specifics.app_id();
  }

  // Reconstruct the page transition - first get the core type.
  int page_transition = syncer::FromSyncPageTransition(
      specifics.page_transition().core_transition());
  // Then add qualifiers (stored in separate proto fields).
  if (specifics.page_transition().blocked()) {
    page_transition |= ui::PAGE_TRANSITION_BLOCKED;
  }
  if (specifics.page_transition().forward_back()) {
    page_transition |= ui::PAGE_TRANSITION_FORWARD_BACK;
  }
  if (specifics.page_transition().from_address_bar()) {
    page_transition |= ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  }
  if (specifics.page_transition().home_page()) {
    page_transition |= ui::PAGE_TRANSITION_HOME_PAGE;
  }
  // Then add redirect markers as appropriate.
  // First, chain start/end markers. Note that these only apply to the
  // first/last visit per entity, respectively.
  if (redirect_index == 0 && !specifics.redirect_chain_start_incomplete()) {
    page_transition |= ui::PAGE_TRANSITION_CHAIN_START;
  }
  // No "else" - a visit can be both the start and end of a chain!
  if (redirect_index == specifics.redirect_entries_size() - 1 &&
      !specifics.redirect_chain_end_incomplete()) {
    page_transition |= ui::PAGE_TRANSITION_CHAIN_END;
  }
  // Finally, add the redirect type (if any).
  if (specifics.redirect_entries(redirect_index).has_redirect_type()) {
    switch (specifics.redirect_entries(redirect_index).redirect_type()) {
      case sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT:
        page_transition |= ui::PAGE_TRANSITION_CLIENT_REDIRECT;
        break;
      case sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT:
        page_transition |= ui::PAGE_TRANSITION_SERVER_REDIRECT;
        break;
    }
  }
  row.transition = ui::PageTransitionFromInt(page_transition);

  if (redirect_index == 0) {
    // The first visit in a chain stores the chain's referring/opener visit (if
    // any).
    row.originator_referring_visit = specifics.originator_referring_visit_id();
    row.originator_opener_visit = specifics.originator_opener_visit_id();
    if (row.originator_referring_visit == kInvalidVisitID) {
      row.external_referrer_url = GURL(specifics.referrer_url());
    }
  } else {
    // All later visits in the chain are implicitly referred to by the preceding
    // visit.
    // Note: For legacy visits (i.e. coming from older clients still using the
    // Sessions integration), originator_visit_id will be unset, so redirect
    // chain links are lost here. They'll be populated in AddEntityInBackend()
    // instead.
    row.originator_referring_visit =
        specifics.redirect_entries(redirect_index - 1).originator_visit_id();
  }

  // The last visit in a chain stores the visit duration (earlier visits, i.e.
  // redirects, are not considered to have a duration).
  if (redirect_index == specifics.redirect_entries_size() - 1) {
    row.visit_duration = base::Microseconds(specifics.visit_duration_micros());
  }

  return row;
}

std::optional<VisitContextAnnotations> MakeContextAnnotations(
    const sync_pb::HistorySpecifics& specifics,
    int redirect_index) {
  // Context annotations are only attached to the last visit in a chain.
  if (redirect_index != specifics.redirect_entries_size() - 1) {
    return std::nullopt;
  }
  VisitContextAnnotations annotations;
  if (specifics.has_browser_type()) {
    annotations.on_visit.browser_type =
        BrowserTypeFromProto(specifics.browser_type());
  }
  annotations.on_visit.window_id =
      SessionID::FromSerializedValue(specifics.window_id());
  annotations.on_visit.tab_id =
      SessionID::FromSerializedValue(specifics.tab_id());
  annotations.on_visit.task_id = specifics.task_id();
  annotations.on_visit.root_task_id = specifics.root_task_id();
  annotations.on_visit.parent_task_id = specifics.parent_task_id();
  annotations.on_visit.response_code = specifics.http_response_code();
  return annotations;
}

std::optional<VisitContentAnnotations> MakeContentAnnotations(
    const sync_pb::HistorySpecifics& specifics,
    int redirect_index) {
  // Content annotations are only attached to the last visit in a chain.
  if (redirect_index != specifics.redirect_entries_size() - 1) {
    return std::nullopt;
  }
  VisitContentAnnotations annotations;
  annotations.page_language = specifics.page_language();
  annotations.password_state =
      PasswordStateFromProto(specifics.password_state());
  annotations.has_url_keyed_image = specifics.has_url_keyed_image();
  if (!specifics.related_searches().empty()) {
    annotations.related_searches =
        std::vector<std::string>(specifics.related_searches().begin(),
                                 specifics.related_searches().end());
  }
  if (!specifics.categories().empty()) {
    for (const auto& category : specifics.categories()) {
      annotations.model_annotations.categories.emplace_back(category.id(),
                                                            category.weight());
    }
  }
  return annotations;
}

// `included_visit_ids` may be nullptr.
std::unique_ptr<syncer::EntityData> MakeEntityData(
    const std::string& local_cache_guid,
    const std::vector<AnnotatedVisit>& redirect_visits,
    bool redirect_chain_middle_trimmed,
    const GURL& referrer_url,
    const std::vector<GURL>& favicon_urls,
    int64_t local_cluster_id,
    std::vector<VisitID>* included_visit_ids,
    std::optional<std::string> app_id) {
  DCHECK(!local_cache_guid.empty());
  DCHECK(!redirect_visits.empty());

  auto entity_data = std::make_unique<syncer::EntityData>();
  sync_pb::HistorySpecifics* history = entity_data->specifics.mutable_history();

  // The first and last visit in the redirect chain are special: The first is
  // where the user intended to go (via typing the URL, clicking on a link, etc)
  // and the last one is where they actually ended up.
  const VisitRow& first_visit = redirect_visits.front().visit_row;
  const VisitRow& last_visit = redirect_visits.back().visit_row;

  // Take the visit time and the originator client ID from the last visit,
  // though they should be the same across all visits in the chain anyway.
  history->set_visit_time_windows_epoch_micros(
      last_visit.visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  const bool is_local_entity = last_visit.originator_cache_guid.empty();
  history->set_originator_cache_guid(
      is_local_entity ? local_cache_guid : last_visit.originator_cache_guid);

  for (const AnnotatedVisit& annotated_visit : redirect_visits) {
    const URLRow& url = annotated_visit.url_row;
    const VisitRow& visit = annotated_visit.visit_row;

    // Add the visit ID to the out param vector indicating it was included.
    if (included_visit_ids) {
      included_visit_ids->push_back(visit.visit_id);
    }

    auto* redirect_entry = history->add_redirect_entries();
    redirect_entry->set_originator_visit_id(
        is_local_entity ? visit.visit_id : visit.originator_visit_id);
    redirect_entry->set_url(url.url().spec());
    redirect_entry->set_title(base::UTF16ToUTF8(url.title()));
    redirect_entry->set_hidden(url.hidden());

    if (ui::PageTransitionIsRedirect(visit.transition)) {
      if (visit.transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
        redirect_entry->set_redirect_type(
            sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT);
      } else {
        // Since we checked ui::PageTransitionIsRedirect(), either the client or
        // the server redirect flag must be set.
        DCHECK(visit.transition & ui::PAGE_TRANSITION_SERVER_REDIRECT);
        redirect_entry->set_redirect_type(
            sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT);
      }
    }
  }

  // The transition should be the same across the whole redirect chain, apart
  // from redirect-related qualifiers. Take the transition from the first visit.
  history->mutable_page_transition()->set_core_transition(
      syncer::ToSyncPageTransition(first_visit.transition));
  history->mutable_page_transition()->set_blocked(
      (first_visit.transition & ui::PAGE_TRANSITION_BLOCKED) != 0);
  history->mutable_page_transition()->set_forward_back(
      (first_visit.transition & ui::PAGE_TRANSITION_FORWARD_BACK) != 0);
  history->mutable_page_transition()->set_from_address_bar(
      (first_visit.transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0);
  history->mutable_page_transition()->set_home_page(
      (first_visit.transition & ui::PAGE_TRANSITION_HOME_PAGE) != 0);

  // The chain_start/end markers are inverted in the proto.
  history->set_redirect_chain_start_incomplete(
      (first_visit.transition & ui::PAGE_TRANSITION_CHAIN_START) == 0);
  // Exception: The chain *end* marker needs to be taken from the last visit!
  history->set_redirect_chain_end_incomplete(
      (last_visit.transition & ui::PAGE_TRANSITION_CHAIN_END) == 0);
  // Note: Typically, chain_start_incomplete and chain_end_incomplete will both
  // end up being false here. However, in some cases (notably, client
  // redirects), a single redirect chain may be split up over multiple entities,
  // in which case one (or even both) might be true.

  history->set_redirect_chain_middle_trimmed(redirect_chain_middle_trimmed);

  // Referring visit and opener visit are taken from the *first* visit in the
  // chain, since they only make sense for that one.
  history->set_originator_referring_visit_id(first_visit.referring_visit);
  history->set_originator_opener_visit_id(first_visit.opener_visit);

  if (referrer_url.is_valid()) {
    history->set_referrer_url(referrer_url.spec());
  }

  // The final visit is the one where the user actually ended up, so it's the
  // only one that can have a (non-zero) visit duration.
  history->set_visit_duration_micros(
      last_visit.visit_duration.InMicroseconds());

  // Add annotation fields. The last visit in the chain is the one that has
  // annotations attached (if any).
  // NOTE: Currently only the "on_visit" fields of the context annotation are
  // supported. When adding any non-"on_visit" field to sync, reconsider how
  // VisitUpdateReason::kSetOnCloseContextAnnotations is handled (but mind
  // additional traffic to the server!)
  const VisitContextAnnotations& context_annotations =
      redirect_visits.back().context_annotations;
  sync_pb::SyncEnums::BrowserType browser_type =
      BrowserTypeToProto(context_annotations.on_visit.browser_type);
  if (browser_type != sync_pb::SyncEnums_BrowserType_BROWSER_TYPE_UNKNOWN) {
    history->set_browser_type(browser_type);
  }
  history->set_window_id(context_annotations.on_visit.window_id.id());
  history->set_tab_id(context_annotations.on_visit.tab_id.id());
  history->set_task_id(context_annotations.on_visit.task_id);
  history->set_root_task_id(context_annotations.on_visit.root_task_id);
  history->set_parent_task_id(context_annotations.on_visit.parent_task_id);
  history->set_http_response_code(context_annotations.on_visit.response_code);
  // NOTE: Only "on_visit" fields are supported, see above.

  const VisitContentAnnotations& content_annotations =
      redirect_visits.back().content_annotations;
  history->set_page_language(content_annotations.page_language);
  history->set_password_state(
      PasswordStateToProto(content_annotations.password_state));
  history->set_has_url_keyed_image(content_annotations.has_url_keyed_image);
  for (const auto& category :
       content_annotations.model_annotations.categories) {
    auto* category_to_sync = history->add_categories();
    category_to_sync->set_id(category.id);
    category_to_sync->set_weight(category.weight);
  }
  if (!content_annotations.related_searches.empty()) {
    history->mutable_related_searches()->Add(
        content_annotations.related_searches.begin(),
        content_annotations.related_searches.end());
  }

  if (!favicon_urls.empty()) {
    // If there are multiple favicon URLs (which should be rare), they're
    // returned in roughly best-to-worst order (see
    // FaviconDatabase::GetIconMappingsForPageURL), so just take the first.
    const GURL& url = favicon_urls.front();
    if (url.is_valid() && url.spec().size() <= kMaxFaviconUrlSizeToSync) {
      history->set_favicon_url(url.spec());
    }
  }

  history->set_originator_cluster_id(local_cluster_id);
  if (app_id) {
    history->set_app_id(*app_id);
  }

  // The entity name is used for debugging purposes; choose something that's a
  // decent tradeoff between "unique" and "readable".
  entity_data->name =
      base::StringPrintf("%s-%s", history->originator_cache_guid().c_str(),
                         redirect_visits.back().url_row.url().spec().c_str());

  return entity_data;
}

// Returns whether all of the URLs in `specifics` are actually considered
// syncable, and eligible for being added to the history DB.
bool SpecificsContainsOnlyValidURLs(
    const sync_pb::HistorySpecifics& specifics,
    const HistoryBackendForSync* history_backend) {
  for (int i = 0; i < specifics.redirect_entries_size(); i++) {
    GURL url(specifics.redirect_entries(i).url());
    // Note: If HistoryBackend::CanAddURL() is false, then the backend would
    // reject this item anyway. But checking it here allows for better error
    // recording (as a specifics error, rather than a DB error).
    if (!ShouldSync(url) || !history_backend->CanAddURL(url)) {
      return false;
    }
  }
  return true;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncHistorySpecificsError)
enum class SpecificsError {
  kMissingRequiredFields = 0,
  kTooOld = 1,
  kTooNew = 2,
  kUnwantedURL = 3,
  kMaxValue = kUnwantedURL
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncHistorySpecificsError)

// Checks the given `specifics` for validity, i.e. whether it passes some basic
// validation checks, and returns the appropriate error if it doesn't.
std::optional<SpecificsError> GetSpecificsError(
    const sync_pb::HistorySpecifics& specifics,
    const HistoryBackendForSync* history_backend) {
  // Check for required fields: visit_time and originator_cache_guid must not be
  // empty, and there must be at least one entry in the redirects list.
  if (specifics.visit_time_windows_epoch_micros() == 0 ||
      specifics.originator_cache_guid().empty() ||
      specifics.redirect_entries_size() == 0) {
    return SpecificsError::kMissingRequiredFields;
  }

  base::Time visit_time = GetVisitTime(specifics);

  // Already-expired visits are not valid. (They wouldn't really cause any harm,
  // but the history backend would just immediately expire them.)
  if (history_backend->IsExpiredVisitTime(visit_time)) {
    return SpecificsError::kTooOld;
  }

  // Visits that are too far in the future are not valid.
  if (visit_time > base::Time::Now() + kMaxWriteToTheFuture) {
    return SpecificsError::kTooNew;
  }

  // Visits to "unwanted" URLs are not valid. Such "unwanted" URLs usually
  // shouldn't end up on the server in the first place, but in some cases they
  // might (e.g. due to older clients still using SESSIONS, which is less
  // strict about filtering URLs).
  if (!SpecificsContainsOnlyValidURLs(specifics, history_backend)) {
    return SpecificsError::kUnwantedURL;
  }

  return {};
}

void RecordSpecificsError(SpecificsError error) {
  base::UmaHistogramEnumeration("Sync.History.IncomingSpecificsError", error);
}

}  // namespace

HistorySyncBridge::HistorySyncBridge(
    HistoryBackendForSync* history_backend,
    HistorySyncMetadataDatabase* sync_metadata_database,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : DataTypeSyncBridge(std::move(change_processor)),
      history_backend_(history_backend),
      sync_metadata_database_(sync_metadata_database) {
  DCHECK(history_backend_);

  history_backend_observation_.Observe(history_backend_.get());
  LoadMetadata();
}

HistorySyncBridge::~HistorySyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
HistorySyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      sync_metadata_database_, syncer::HISTORY,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError> HistorySyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // Since HISTORY is in ApplyUpdatesImmediatelyTypes(), MergeFullSyncData()
  // should never be called.
  NOTREACHED_IN_MIGRATION();
  return {};
}

std::optional<syncer::ModelError>
HistorySyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!processing_syncer_changes_);
  DCHECK(sync_metadata_database_);
  // Set flag to stop accepting history change notifications from backend.
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  VisitIDRemapper id_remapper(history_backend_);

  for (const std::unique_ptr<syncer::EntityChange>& entity_change :
       entity_changes) {
    DCHECK(entity_change->data().specifics.has_history());
    const sync_pb::HistorySpecifics& specifics =
        entity_change->data().specifics.history();

    // Check validity requirements.
    std::optional<SpecificsError> specifics_error =
        GetSpecificsError(specifics, history_backend_);
    if (specifics_error.has_value()) {
      DVLOG(1) << "Skipping invalid visit, reason "
               << static_cast<int>(*specifics_error);
      RecordSpecificsError(*specifics_error);
      continue;
    }

    if (specifics.originator_cache_guid() == GetLocalCacheGuid()) {
      // This is likely a reflection, i.e. an update that came from this client.
      // (Unless a different client is misbehaving and sending data with this
      // client's cache GUID.) So no need to do anything with it; the data is
      // already here.
      // Note: For other data types, the processor filters out reflection
      // updates before they reach the bridge, but here that's not possible
      // because metadata is not tracked.
      continue;
    }

    switch (entity_change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        // First try updating an existing row. Since metadata isn't tracked for
        // this data type, the processor can't distinguish "ADD" from "UPDATE".
        // Note: Because metadata isn't tracked (and thus no version numbers
        // exist), it's theoretically possible to receive an older version of an
        // already-existing entity here. This should be very rare in practice
        // and would be tricky to handle (would have to store version numbers
        // elsewhere), so just ignore this case.
        if (UpdateEntityInBackend(&id_remapper, specifics)) {
          // Updating worked - there was a matching visit in the DB already.
          // Nothing further to be done here.
        } else {
          // Updating didn't work, so actually add the data instead.
          if (!AddEntityInBackend(&id_remapper, specifics)) {
            // Something went wrong.
            RecordDatabaseError(SyncHistoryDatabaseError::
                                    kApplyIncrementalSyncChangesAddSyncedVisit);
            break;
          }
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        // Deletes are not supported - they're handled via
        // HISTORY_DELETE_DIRECTIVE instead. And, since metadata isn't tracked,
        // the processor should never send deletions anyway (even if a different
        // client uploaded a tombstone). [Edge case: Metadata for unsynced
        // entities *is* tracked, but then an incoming tombstone would result in
        // a conflict that'd be resolved as "local edit wins over remote
        // deletion", so still no ACTION_DELETE would arrive here.]
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  id_remapper.RemapIDs();

  std::optional<syncer::ModelError> metadata_error =
      change_processor()->GetError();
  if (metadata_error) {
    RecordDatabaseError(
        SyncHistoryDatabaseError::kApplyIncrementalSyncChangesWriteMetadata);
  }

  // ApplyIncrementalSyncChanges() gets called both for incoming remote changes
  // (i.e. for GetUpdates) and after a successful Commit. In either case,
  // there's now likely some local metadata that's not needed anymore, so go and
  // clean that up.
  UntrackAndClearMetadataForSyncedEntities();

  return metadata_error;
}

void HistorySyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Delete all foreign visits from the DB.
  history_backend_->DeleteAllForeignVisitsAndResetIsKnownToSync();

  DataTypeSyncBridge::ApplyDisableSyncChanges(
      std::move(delete_metadata_change_list));
}

std::unique_ptr<syncer::DataBatch> HistorySyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  return GetDataImpl(storage_keys);
}

std::unique_ptr<syncer::DataBatch> HistorySyncBridge::GetDataImpl(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    base::Time visit_time =
        HistorySyncMetadataDatabase::StorageKeyToVisitTime(key);
    VisitRow final_visit;
    if (!history_backend_->GetLastVisitByTime(visit_time, &final_visit)) {
      continue;
    }

    // Purposely don't mark visits as known to sync here, as this bit must have
    // already been set before, when the visit was first seen by Sync.
    std::vector<std::unique_ptr<syncer::EntityData>> entity_data_list =
        QueryRedirectChainAndMakeEntityData(final_visit,
                                            /*included_visit_ids=*/nullptr);
    // Typically, `entity_data_list` will have exactly one entry. In some error
    // cases (corrupted DB), it may be empty, and in some cases the redirect
    // chain may have been split into multiple entities. In that case, the last
    // entity should be the one corresponding to the `key`.
    if (entity_data_list.empty()) {
      continue;
    }
    std::unique_ptr<syncer::EntityData> entity_data =
        std::move(entity_data_list.back());
    // The last entity's visit time should almost always match the desired one,
    // but again, in some rare DB error cases it may not.
    if (entity_data->specifics.history().visit_time_windows_epoch_micros() !=
        visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds()) {
      continue;
    }
    batch->Put(key, std::move(entity_data));
  }

  return batch;
}

std::unique_ptr<syncer::DataBatch> HistorySyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sync_metadata_database_) {
    return nullptr;
  }

  auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
  if (!sync_metadata_database_->GetAllSyncMetadata(metadata_batch.get())) {
    RecordDatabaseError(SyncHistoryDatabaseError::kGetAllDataReadMetadata);
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading metadata from HistorySyncMetadataDatabase."});
  }
  StorageKeyList storage_keys;
  for (const auto& [storage_key, metadata] : metadata_batch->GetAllMetadata()) {
    storage_keys.push_back(storage_key);
  }
  return GetDataImpl(std::move(storage_keys));
}

std::string HistorySyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entity_data.specifics.has_history())
      << "EntityData does not have history specifics.";

  const sync_pb::HistorySpecifics& history = entity_data.specifics.history();
  return base::NumberToString(history.visit_time_windows_epoch_micros());
}

std::string HistorySyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entity_data.specifics.has_history())
      << "EntityData does not have history specifics.";

  const sync_pb::HistorySpecifics& history = entity_data.specifics.history();
  return HistorySyncMetadataDatabase::StorageKeyFromMicrosSinceWindowsEpoch(
      history.visit_time_windows_epoch_micros());
}

syncer::ConflictResolution HistorySyncBridge::ResolveConflict(
    const std::string& storage_key,
    const syncer::EntityData& remote_data) const {
  // Real conflicts can't happen for this data type, since every client only
  // ever updates its own entities. There's one specific edge case that gets
  // detected as a conflict:
  // - An entity gets committed. After successful commit, the entity metadata
  //   gets untracked.
  // - The entity gets changed, and thus is pending commit again, but...
  // - ...before the commit actually happens, a GetUpdates happens which returns
  //   the reflection from the first commit. Because the entity metadata was
  //   cleared, the processor can't detect that this is a reflection (see
  //   ProcessorEntity::IsVersionAlreadyKnown()), so this gets flagged as a
  //   conflict.
  // In this case, the default resolution of using the remote data would cancel
  // the pending commit (and thus cause a change to be lost), so use the local
  // data here.
  // For extra safety, make sure that the entity's originator is actually this
  // device. This should always be true in practice.
  if (remote_data.specifics.history().originator_cache_guid() ==
      GetLocalCacheGuid()) {
    return syncer::ConflictResolution::kUseLocal;
  }
  return DataTypeSyncBridge::ResolveConflict(storage_key, remote_data);
}

void HistorySyncBridge::OnURLVisited(HistoryBackend* history_backend,
                                     const URLRow& url_row,
                                     const VisitRow& visit_row) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MaybeCommit(visit_row);
}

void HistorySyncBridge::OnURLsModified(HistoryBackend* history_backend,
                                       const URLRows& changed_urls,
                                       bool is_from_expiration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ShouldCommitRightNow()) {
    return;
  }

  // Not interested in expirations - both the server side and other clients have
  // their own, independent expiration logic, so no need to send any updates.
  if (is_from_expiration) {
    return;
  }

  for (const URLRow& url_row : changed_urls) {
    VisitRow visit_row;
    if (history_backend_->GetMostRecentVisitForURL(url_row.id(), &visit_row) &&
        visit_row.originator_cache_guid.empty()) {
      // It's the URL corresponding to a local visit - probably the title got
      // updated.
      MaybeCommit(visit_row);
    }
  }
}

void HistorySyncBridge::OnHistoryDeletions(HistoryBackend* history_backend,
                                           bool all_history,
                                           bool expired,
                                           const URLRows& deleted_rows,
                                           const std::set<GURL>& favicon_urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If individual URLs get deleted, we're notified about their removed visits
  // via OnVisitDeleted(), so there's nothing to be done here. But if all
  // history is cleared, there are no individual notifications, so handle that
  // case here.
  if (!all_history) {
    return;
  }

  // No need to send any actual deletions: A HistoryDeleteDirective will take
  // care of that. Just untrack all entities and clear their metadata. (The only
  // case where such metadata actually exists is if there are entities that are
  // waiting for a commit. Clear their metadata, to cancel those commits.)
  UntrackAndClearMetadataForAllEntities();
}

void HistorySyncBridge::OnVisitUpdated(const VisitRow& visit_row,
                                       VisitUpdateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (reason) {
    case VisitUpdateReason::kSetPageLanguage:
    case VisitUpdateReason::kSetPasswordState:
    case VisitUpdateReason::kUpdateVisitDuration:
    case VisitUpdateReason::kUpdateTransition:
    case VisitUpdateReason::kAddContextAnnotations:
      // Standard case: These are all interesting, process this update.
      break;
    case VisitUpdateReason::kUpdateSyncedVisit:
      // UpdateSyncedVisit() should only be called by this bridge (so typically
      // `processing_syncer_changes_` should be true here, but this doesn't hold
      // in some tests). Anyway, if a foreign visit somehow does get updated on
      // this device (e.g. due to a bug), better *not* to send out updates and
      // potentially mess up other clients. So ignore this.
      return;
    case VisitUpdateReason::kSetOnCloseContextAnnotations:
      // None of the on-close context annotations are synced, so ignore this.
      return;
  }

  MaybeCommit(visit_row);
}

void HistorySyncBridge::OnVisitDeleted(const VisitRow& visit_row) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No need to send an actual deletion: Either this was an expiry, in which
  // no deletion should be sent, or if it's an actual deletion, then a
  // HistoryDeleteDirective will take care of that. Just untrack the entity and
  // delete its metadata (just in case this entity was waiting to be committed -
  // otherwise no metadata exists anyway).
  std::string storage_key = GetStorageKeyFromVisitRow(visit_row);
  if (sync_metadata_database_) {
    sync_metadata_database_->ClearEntityMetadata(syncer::HISTORY, storage_key);
  }
  change_processor()->UntrackEntityForStorageKey(storage_key);
}

void HistorySyncBridge::SetSyncTransportState(
    syncer::SyncService::TransportState state) {
  sync_transport_state_ = state;
}

void HistorySyncBridge::OnDatabaseError() {
  sync_metadata_database_ = nullptr;
  RecordDatabaseError(SyncHistoryDatabaseError::kOnDatabaseError);
  change_processor()->ReportError(
      {FROM_HERE, "HistoryDatabase encountered error"});
}

void HistorySyncBridge::LoadMetadata() {
  if (!sync_metadata_database_) {
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!sync_metadata_database_->GetAllSyncMetadata(batch.get())) {
    RecordDatabaseError(SyncHistoryDatabaseError::kLoadMetadata);
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading metadata from HistorySyncMetadataDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

bool HistorySyncBridge::ShouldCommitRightNow() const {
  if (!sync_metadata_database_) {
    return false;  // History DB is not functional; don't commit anything.
  }

  if (processing_syncer_changes_) {
    return false;  // These are changes originating from us, ignore.
  }

  switch (sync_transport_state_) {
    case syncer::SyncService::TransportState::DISABLED:
    case syncer::SyncService::TransportState::PAUSED:
      // Sync is disabled or paused, nothing should be committed.
      return false;
    case syncer::SyncService::TransportState::START_DEFERRED:
    case syncer::SyncService::TransportState::INITIALIZING:
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case syncer::SyncService::TransportState::CONFIGURING:
    case syncer::SyncService::TransportState::ACTIVE:
      // In all of these states, Sync is enabled in principle and expected to
      // start up soon, so changes may be committed (subject to further
      // conditions below).
      break;
  }

  if (!change_processor()->IsTrackingMetadata()) {
    // The processor isn't ready - either Sync is disabled for this data type,
    // or the initial download is still ongoing.
    return false;
  }

  return true;
}

void HistorySyncBridge::MaybeCommit(const VisitRow& visit_row) {
  // First check if the overall state allows committing right now.
  if (!ShouldCommitRightNow()) {
    return;
  }

  // If this visit is not the end of a redirect chain, ignore it. Note that
  // visits that are not part of a redirect chain are considered to be both
  // start and end of a chain, so these are *not* ignored here.
  if (!(visit_row.transition & ui::PAGE_TRANSITION_CHAIN_END)) {
    return;
  }

  // If this visit originally came from a different device, don't update it.
  // This shouldn't usually happen, but if it does happen for some reason (e.g.
  // due to a bug elsewhere), better not to mess up other clients.
  if (!visit_row.originator_cache_guid.empty()) {
    return;
  }

  // Attempt converting the the visit into Sync's format. In some cases, the
  // conversion process catches additional un-syncable conditions, so early exit
  // to account for that case as well.
  std::vector<VisitID> included_visit_ids;
  std::vector<std::unique_ptr<syncer::EntityData>> entity_data_list =
      QueryRedirectChainAndMakeEntityData(visit_row, &included_visit_ids);

  // Special case: If there are more than 2 entities (i.e. sub-chains), there's
  // no need to commit more than the last 2. In that case, the last entity is
  // the only new one, and the one before that was likely updated (e.g. by
  // removing the chain-end marker, and setting a visit duration). All previous
  // entities must have been previously committed and must be unchanged.
  if (entity_data_list.size() > 2) {
    entity_data_list.erase(entity_data_list.begin(),
                           entity_data_list.end() - 2);
  }

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  for (auto& entity_data : entity_data_list) {
    std::string storage_key = GetStorageKey(*entity_data);
    change_processor()->Put(storage_key, std::move(entity_data),
                            metadata_change_list.get());
  }

  // Mark these visits as sent in the database. They are sent in a few seconds.
  for (auto visit_id : included_visit_ids) {
    history_backend_->MarkVisitAsKnownToSync(visit_id);
  }
}

std::vector<std::unique_ptr<syncer::EntityData>>
HistorySyncBridge::QueryRedirectChainAndMakeEntityData(
    const VisitRow& final_visit,
    std::vector<VisitID>* included_visit_ids) {
  // Query the redirect chain that ended in this visit.
  std::vector<VisitRow> redirect_visits =
      history_backend_->GetRedirectChain(final_visit);
  if (redirect_visits.empty()) {
    // This can happen if there's invalid data in the DB (e.g. broken referrer
    // "links").
    return {};
  }
  DCHECK_EQ(redirect_visits.back().visit_id, final_visit.visit_id);

  // Typically, all visits in a redirect chain have the same timestamp. However,
  // in some cases, a redirect chain may be extended retroactively (with visits
  // with a different timestamp). In that case, split the chain into multiple
  // subchains, which will become separate Sync entities.
  std::vector<std::unique_ptr<syncer::EntityData>> entities;
  auto subchain_begin = redirect_visits.begin();
  while (subchain_begin != redirect_visits.end()) {
    // `subchain_begin` points to the beginning of the current subchain.
    base::Time chain_time = subchain_begin->visit_time;
    auto subchain_end = subchain_begin + 1;
    while (subchain_end != redirect_visits.end() &&
           subchain_end->visit_time == chain_time) {
      ++subchain_end;
    }
    // Now `subchain_end` points just beyond the end of the current subchain
    // (i.e. first entry with a different timestamp or `redirect_visits.end()`).

    // Grab the current subchain.
    std::vector<VisitRow> subchain_visits(
        std::make_move_iterator(subchain_begin),
        std::make_move_iterator(subchain_end));

    // If the redirect chain is excessively long, trim it at the middle.
    bool chain_middle_trimmed = false;
    if (subchain_visits.size() > kMaxRedirectsPerEntity) {
      int keep = kMaxRedirectsPerEntity / 2;
      subchain_visits.erase(subchain_visits.begin() + keep,
                            subchain_visits.end() - keep);
      chain_middle_trimmed = true;
    }

    // Make `subchain_begin` point to the beginning of the *next* subchain, for
    // the next iteration.
    subchain_begin = subchain_end;

    // Query the URL and annotation info for the current subchain.
    std::vector<AnnotatedVisit> annotated_visits =
        history_backend_->ToAnnotatedVisitsFromRows(
            subchain_visits,
            /*compute_redirect_chain_start_properties=*/false);
    if (annotated_visits.empty()) {
      // Again, this can happen if there's invalid data in the DB. In that case,
      // skip this subchain but still try to handle any others.
      continue;
    }

    // If there are any unsyncable URLs in the chain, skip the whole thing.
    // (Typically, unsyncable URLs like file:// or data:// shouldn't have
    // redirects anyway.)
    for (const AnnotatedVisit& visit : annotated_visits) {
      if (!ShouldSync(visit.url_row.url())) {
        return {};
      }
    }

    // Convert the current subchain into a SyncEntity.
    GURL referrer_url;
    VisitID referrer_id = annotated_visits.front().visit_row.referring_visit;
    if (referrer_id != kInvalidVisitID) {
      referrer_url = GetURLForVisit(referrer_id);
    } else {
      referrer_url = annotated_visits.front().visit_row.external_referrer_url;
    }
    // Note: `favicon_urls` may legitimately be empty, that's fine.
    std::vector<GURL> favicon_urls = history_backend_->GetFaviconURLsForURL(
        annotated_visits.back().url_row.url());
    // Note: `local_cluster_id` can legitimately be 0 and only get it for the
    // first visit, as the cluster id for everything in the redirect chain
    // should be the same (except potentially in unit tests).
    int64_t local_cluster_id = history_backend_->GetClusterIdContainingVisit(
        redirect_visits.front().visit_id);
    entities.push_back(MakeEntityData(GetLocalCacheGuid(), annotated_visits,
                                      chain_middle_trimmed, referrer_url,
                                      favicon_urls, local_cluster_id,
                                      included_visit_ids, final_visit.app_id));
  }

  return entities;
}

GURL HistorySyncBridge::GetURLForVisit(VisitID visit_id) {
  if (visit_id == kInvalidVisitID) {
    return GURL();
  }

  VisitRow visit_row;
  if (!history_backend_->GetVisitByID(visit_id, &visit_row)) {
    return GURL();
  }

  URLRow url_row;
  if (!history_backend_->GetURLByID(visit_row.url_id, &url_row)) {
    return GURL();
  }

  return url_row.url();
}

bool HistorySyncBridge::AddEntityInBackend(
    VisitIDRemapper* id_remapper,
    const sync_pb::HistorySpecifics& specifics) {
  // Add all the visits in the redirect chain.
  VisitID referring_visit_id = 0;
  for (int i = 0; i < specifics.redirect_entries_size(); i++) {
    VisitRow visit_row = MakeVisitRow(specifics, i);
    // Trivial in-chain remapping: Populate the `referring_visit` IDs along the
    // redirect chain. Do this here because old clients don't fill originator
    // visits IDs, so the remapper can't help. For such clients we can at least
    // do this to have the links inside this redirect chain. For new clients,
    // might as well do this part here too since it's correct.
    if (i > 0) {
      visit_row.referring_visit = referring_visit_id;
    }
    std::optional<VisitContextAnnotations> context_annotations =
        MakeContextAnnotations(specifics, i);
    std::optional<VisitContentAnnotations> content_annotations =
        MakeContentAnnotations(specifics, i);
    VisitID added_visit_id = history_backend_->AddSyncedVisit(
        GURL(specifics.redirect_entries(i).url()),
        base::UTF8ToUTF16(specifics.redirect_entries(i).title()),
        specifics.redirect_entries(i).hidden(), visit_row, context_annotations,
        content_annotations);
    if (added_visit_id == 0) {
      // Visit failed to be added to the DB - unclear if/how this can happen.
      return false;
    }
    referring_visit_id = added_visit_id;

    // If the sending client supports syncing its clusters, add the appropriate
    // details to history.
    DCHECK(!specifics.originator_cache_guid().empty());
    if (specifics.originator_cluster_id() > 0) {
      // Populate the visit to a synced cluster.
      history::ClusterVisit cluster_visit;
      cluster_visit.annotated_visit.visit_row = visit_row;
      cluster_visit.annotated_visit.visit_row.visit_id = added_visit_id;
      history_backend_->AddVisitToSyncedCluster(
          cluster_visit, specifics.originator_cache_guid(),
          specifics.originator_cluster_id());
    }

    // Remapping chain extremities (i.e. first and last visit in the chain) via
    // `id_remapper`: The first visit in the chain can refer to a visit outside
    // of the chain. Similarly, the last visit can be referred to by a visit
    // outside of the chain (its referring visit ID was already set though).
    if (i == 0 || i == specifics.redirect_entries_size() - 1) {
      id_remapper->RegisterVisit(
          added_visit_id, visit_row.originator_cache_guid,
          visit_row.originator_visit_id, visit_row.originator_referring_visit,
          visit_row.originator_opener_visit);
    }
  }

  return true;
}

bool HistorySyncBridge::UpdateEntityInBackend(
    VisitIDRemapper* id_remapper,
    const sync_pb::HistorySpecifics& specifics) {
  // Only try updating the final visit in a chain - earlier visits (i.e.
  // redirects) can't get updated anyway.
  // TODO(crbug.com/40059424): Verify whether only updating the chain end
  // is indeed sufficient.
  int index = specifics.redirect_entries_size() - 1;
  VisitRow final_visit_row = MakeVisitRow(specifics, index);
  std::optional<VisitContextAnnotations> context_annotations =
      MakeContextAnnotations(specifics, index);
  std::optional<VisitContentAnnotations> content_annotations =
      MakeContentAnnotations(specifics, index);
  // Note: UpdateSyncedVisit() keeps any existing local referrer/opener IDs in
  // place, and the originator IDs are never updated in practice, so there's no
  // need to invoke the ID remapper here (in contrast to AddEntityInBackend()).
  VisitID updated_visit_id = history_backend_->UpdateSyncedVisit(
      GURL(specifics.redirect_entries(index).url()),
      base::UTF8ToUTF16(specifics.redirect_entries(index).title()),
      specifics.redirect_entries(index).hidden(), final_visit_row,
      context_annotations, content_annotations);
  if (updated_visit_id == 0) {
    return false;
  }

  return true;
}

void HistorySyncBridge::UntrackAndClearMetadataForAllEntities() {
  if (sync_metadata_database_) {
    sync_metadata_database_->ClearAllEntityMetadata();
  }
  for (const std::string& storage_key :
       change_processor()->GetAllTrackedStorageKeys()) {
    change_processor()->UntrackEntityForStorageKey(storage_key);
  }
}

void HistorySyncBridge::UntrackAndClearMetadataForSyncedEntities() {
  DCHECK(sync_metadata_database_);
  for (const std::string& storage_key :
       change_processor()->GetAllTrackedStorageKeys()) {
    if (change_processor()->IsEntityUnsynced(storage_key)) {
      // "Unsynced" entities (i.e. those with local changes that still need to
      // be committed) have to be tracked, so *don't* clear their metadata.
      continue;
    }
    sync_metadata_database_->ClearEntityMetadata(syncer::HISTORY, storage_key);
    change_processor()->UntrackEntityForStorageKey(storage_key);
  }
}

std::string HistorySyncBridge::GetLocalCacheGuid() const {
  // Before the processor is tracking metadata, the cache GUID isn't known.
  DCHECK(change_processor()->IsTrackingMetadata());
  return change_processor()->TrackedCacheGuid();
}

}  // namespace history
