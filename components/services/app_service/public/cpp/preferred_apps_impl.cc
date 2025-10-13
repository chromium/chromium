// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_impl.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_converter.h"

namespace {

const base::FilePath::CharType kPreferredAppsDirname[] =
    FILE_PATH_LITERAL("PreferredApps");

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreferredAppsFileIOAction {
  kWriteSuccess = 0,
  kWriteFailed = 1,
  kReadSuccess = 2,
  kReadFailed = 3,
  kMaxValue = kReadFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreferredAppsUpdateAction {
  kAdd = 0,
  kDeleteForFilter = 1,
  kDeleteForAppId = 2,
  kUpgraded = 3,
  kMaxValue = kUpgraded,
};

// Performs blocking I/O. Called on another thread.
void WriteDataBlocking(const base::FilePath& preferred_apps_file,
                       const std::string& preferred_apps) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::WriteFile(preferred_apps_file, preferred_apps)) {
    DVLOG(0) << "Fail to write preferred apps to " << preferred_apps_file;
  }
}

// Performs blocking I/O. Called on another thread.
std::string ReadDataBlocking(const base::FilePath& preferred_apps_file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::string preferred_apps_string;
  base::ReadFileToString(preferred_apps_file, &preferred_apps_string);
  return preferred_apps_string;
}

}  // namespace

namespace apps {

PreferredAppsImpl::PreferredAppsImpl(
    Host* host,
    const base::FilePath& profile_dir,
    base::OnceClosure read_completed_for_testing,
    base::OnceClosure write_completed_for_testing)
    : host_(host),
      profile_dir_(profile_dir),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      read_completed_for_testing_(std::move(read_completed_for_testing)),
      write_completed_for_testing_(std::move(write_completed_for_testing)) {
  DCHECK(host_);
  InitializePreferredApps();
}

PreferredAppsImpl::~PreferredAppsImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PreferredAppsImpl::RemovePreferredApp(const std::string& app_id) {
  RunAfterPreferredAppsReady(
      base::BindOnce(&PreferredAppsImpl::RemovePreferredAppImpl,
                     weak_ptr_factory_.GetWeakPtr(), app_id));
}

void PreferredAppsImpl::SetSupportedLinksPreference(
    const std::string& app_id,
    IntentFilters all_link_filters) {
  RunAfterPreferredAppsReady(base::BindOnce(
      &PreferredAppsImpl::SetSupportedLinksPreferenceImpl,
      weak_ptr_factory_.GetWeakPtr(), app_id, std::move(all_link_filters)));
}

#if BUILDFLAG(IS_CHROMEOS)
void PreferredAppsImpl::SetProtocolLinkPreference(
    const std::string& app_id,
    IntentFilterPtr protocol_link_filter) {
  RunAfterPreferredAppsReady(base::BindOnce(
      &PreferredAppsImpl::SetProtocolLinkPreferenceImpl,
      weak_ptr_factory_.GetWeakPtr(), app_id, std::move(protocol_link_filter)));
}

void PreferredAppsImpl::RemoveProtocolLinkFilters(
    const std::string& app_id,
    IntentFilters protocol_link_filters) {
  RunAfterPreferredAppsReady(
      base::BindOnce(&PreferredAppsImpl::RemoveProtocolLinkFiltersImpl,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(protocol_link_filters)));
}
#endif

void PreferredAppsImpl::RemoveSupportedLinksPreference(
    const std::string& app_id) {
  RunAfterPreferredAppsReady(
      base::BindOnce(&PreferredAppsImpl::RemoveSupportedLinksPreferenceImpl,
                     weak_ptr_factory_.GetWeakPtr(), app_id));
}

void PreferredAppsImpl::InitializePreferredApps() {
  ReadFromJSON(profile_dir_);
}

void PreferredAppsImpl::WriteToJSON(
    const base::FilePath& profile_dir,
    const apps::PreferredAppsList& preferred_apps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If currently is writing preferred apps to file, set a flag to write after
  // the current write completed.
  if (writing_preferred_apps_) {
    should_write_preferred_apps_to_file_ = true;
    return;
  }

  writing_preferred_apps_ = true;

  auto preferred_apps_value =
      apps::ConvertPreferredAppsToValue(preferred_apps.GetReference());

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(preferred_apps_value);

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WriteDataBlocking,
                     profile_dir.Append(kPreferredAppsDirname), json_string),
      base::BindOnce(&PreferredAppsImpl::WriteCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PreferredAppsImpl::WriteCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  writing_preferred_apps_ = false;
  if (!should_write_preferred_apps_to_file_) {
    // Call the testing callback if it is set.
    if (write_completed_for_testing_) {
      std::move(write_completed_for_testing_).Run();
    }
    return;
  }
  // If need to perform another write, write the most up to date preferred apps
  // from memory to file.
  should_write_preferred_apps_to_file_ = false;
  WriteToJSON(profile_dir_, preferred_apps_list_);
}

void PreferredAppsImpl::ReadFromJSON(const base::FilePath& profile_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadDataBlocking,
                     profile_dir.Append(kPreferredAppsDirname)),
      base::BindOnce(&PreferredAppsImpl::ReadCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PreferredAppsImpl::ReadCompleted(std::string preferred_apps_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool preferred_apps_upgraded = false;
  if (preferred_apps_string.empty()) {
    preferred_apps_list_.Init();
  } else {
    std::string json_string;
    JSONStringValueDeserializer deserializer(preferred_apps_string);
    int error_code;
    std::string error_message;
    auto preferred_apps_value =
        deserializer.Deserialize(&error_code, &error_message);

    if (!preferred_apps_value) {
      DVLOG(0) << "Fail to deserialize json value from string with error code: "
               << error_code << " and error message: " << error_message;
      preferred_apps_list_.Init();
    } else {
      preferred_apps_upgraded = IsUpgradedForSharing(*preferred_apps_value);
      auto preferred_apps =
          apps::ParseValueToPreferredApps(*preferred_apps_value);
      if (!preferred_apps_upgraded) {
        UpgradePreferredApps(preferred_apps);
      }
      preferred_apps_list_.Init(std::move(preferred_apps));
    }
  }
  if (!preferred_apps_upgraded) {
    WriteToJSON(profile_dir_, preferred_apps_list_);
  }

  while (!pending_preferred_apps_tasks_.empty()) {
    std::move(pending_preferred_apps_tasks_.front()).Run();
    pending_preferred_apps_tasks_.pop();
  }

  if (read_completed_for_testing_) {
    std::move(read_completed_for_testing_).Run();
  }
}

void PreferredAppsImpl::RunAfterPreferredAppsReady(base::OnceClosure task) {
  if (preferred_apps_list_.IsInitialized()) {
    std::move(task).Run();
  } else {
    pending_preferred_apps_tasks_.push(std::move(task));
  }
}

void PreferredAppsImpl::RemovePreferredAppImpl(const std::string& app_id) {
  IntentFilters removed_filters = preferred_apps_list_.DeleteAppId(app_id);
  if (!removed_filters.empty()) {
    WriteToJSON(profile_dir_, preferred_apps_list_);
  }
}

void PreferredAppsImpl::SetSupportedLinksPreferenceImpl(
    const std::string& app_id,
    IntentFilters all_link_filters) {
  base::flat_map<std::string, IntentFilters> removed;

  for (auto& filter : all_link_filters) {
    CHECK(apps_util::IsSupportedLinkForApp(app_id, filter));
    auto replaced_apps = preferred_apps_list_.AddPreferredApp(app_id, filter);

    // If we removed overlapping supported links when adding the new app, those
    // affected apps no longer handle all their Supported Links filters and so
    // need to have all their other Supported Links filters removed.
    // Additionally, track all removals in the |removed| map so that subscribers
    // can be notified correctly.
    for (auto& replaced_app_and_filters : replaced_apps) {
      const std::string& removed_app_id = replaced_app_and_filters.first;
      bool first_removal_for_app = !base::Contains(removed, app_id);
      bool did_replace_supported_link = std::ranges::any_of(
          replaced_app_and_filters.second,
          [&removed_app_id](const auto& filter) {
            return apps_util::IsSupportedLinkForApp(removed_app_id, filter);
          });

      IntentFilters& removed_filters_for_app = removed[removed_app_id];
      removed_filters_for_app.insert(
          removed_filters_for_app.end(),
          std::make_move_iterator(replaced_app_and_filters.second.begin()),
          std::make_move_iterator(replaced_app_and_filters.second.end()));

      // We only need to remove other supported links once per app.
      if (first_removal_for_app && did_replace_supported_link) {
        IntentFilters removed_filters =
            preferred_apps_list_.DeleteSupportedLinks(removed_app_id);
        removed_filters_for_app.insert(
            removed_filters_for_app.end(),
            std::make_move_iterator(removed_filters.begin()),
            std::make_move_iterator(removed_filters.end()));
      }
    }
  }

  WriteToJSON(profile_dir_, preferred_apps_list_);

  // Notify publishers: The new app has been set to open links, and all removed
  // apps no longer handle links.
  host_->OnSupportedLinksPreferenceChanged(app_id,
                                           /*open_in_app=*/true);
  for (const auto& removed_app_and_filters : removed) {
    host_->OnSupportedLinksPreferenceChanged(removed_app_and_filters.first,
                                             /*open_in_app=*/false);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void PreferredAppsImpl::SetProtocolLinkPreferenceImpl(
    const std::string& app_id,
    IntentFilterPtr protocol_link_filter) {
  CHECK(!apps_util::IsSupportedLinkForApp(app_id, protocol_link_filter));
  preferred_apps_list_.AddPreferredApp(app_id, protocol_link_filter);
  WriteToJSON(profile_dir_, preferred_apps_list_);

  // We don't dispatch any events to observers as protocol links are not equal
  // to supported links.
}

void PreferredAppsImpl::RemoveProtocolLinkFiltersImpl(
    const std::string& app_id,
    IntentFilters protocol_link_filters) {
  bool needs_write = false;
  for (const auto& filter : protocol_link_filters) {
    CHECK(!apps_util::IsSupportedLinkForApp(app_id, filter));
    needs_write |=
        !preferred_apps_list_.DeletePreferredApp(app_id, filter).empty();
  }
  if (needs_write) {
    WriteToJSON(profile_dir_, preferred_apps_list_);
  }

  // We don't dispatch any events to observers as protocol links are not equal
  // to supported links.
}
#endif

void PreferredAppsImpl::RemoveSupportedLinksPreferenceImpl(
    const std::string& app_id) {
  IntentFilters removed_filters =
      preferred_apps_list_.DeleteSupportedLinks(app_id);

  if (!removed_filters.empty()) {
    WriteToJSON(profile_dir_, preferred_apps_list_);
  }

  host_->OnSupportedLinksPreferenceChanged(app_id,
                                           /*open_in_app=*/false);
}

}  // namespace apps
