// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/featured/featured_client.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check_is_test.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/featured/fake_featured_client.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/constants/featured.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash::featured {

namespace {

FeaturedClient* g_instance = nullptr;

struct FileWatchOptions {
  FeaturedClient::ListenForTrialCallback listen_callback;
  const base::FilePath expected_dir;
};

void RecordEarlyBootTrialInUMA(const std::string& trial_name,
                               const std::string& group_name) {
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  // This records the trial in UMA.
  trial->Activate();
}

// Assumes |filename| is valid (that its directory name is correct).
bool RecordEarlyBootTrialInChrome(
    FeaturedClient::ListenForTrialCallback listen_callback,
    const base::FilePath& filename) {
  base::FieldTrial::ActiveGroup active_group;
  if (!FeaturedClient::ParseTrialFilename(filename, active_group)) {
    return false;
  }
  listen_callback.Run(active_group.trial_name, active_group.group_name);
  return true;
}

void RecordEarlyBootTrialAfterChromeStartup(
    const FileWatchOptions& opts,
    const base::FilePathWatcher::ChangeInfo& change_info,
    const base::FilePath& path,
    bool error) {
  if (error || path.DirName() != opts.expected_dir) {
    // TODO(b/296394808): Add UMA metric if we enter this code path since it
    // is not expected.
    return;
  }

  if (change_info.file_path_type !=
          base::FilePathWatcher::FilePathType::kFile ||
      change_info.change_type != base::FilePathWatcher::ChangeType::kCreated) {
    // Only record field trial files that were just created. We do not want to
    // record a field trial on any of the other change options like
    // base::FilePathWatcher::ChangeType::kModified or
    // base::FilePathWatcher::ChangeType::kDeleted.
    return;
  }

  // TODO(b/296394808): Add UMA metric if unable to record trial due to parse
  // error.
  RecordEarlyBootTrialInChrome(opts.listen_callback, path);
}

void ListenForActiveEarlyBootTrials(base::FilePathWatcher* watcher,
                                    const FileWatchOptions& opts) {
  base::FilePathWatcher::WatchOptions options = {
      // Watches for changes in a directory.
      .type = base::FilePathWatcher::Type::kRecursive,
      // Reports the path of modified files in the directory.
      .report_modified_path = true};

  watcher->WatchWithChangeInfo(
      opts.expected_dir, options,
      base::BindRepeating(&RecordEarlyBootTrialAfterChromeStartup, opts));
}

void ReadTrialsActivatedBeforeChromeStartup(const FileWatchOptions& opts) {
  base::DirReaderPosix reader(opts.expected_dir.value().c_str());
  if (!reader.IsValid()) {
    // TODO(b/296394808): Add UMA metric if we are unable to enumerate trials
    // activated before Chrome startup.
    return;
  }

  while (reader.Next()) {
    if (std::string(reader.name()) == "." ||
        std::string(reader.name()) == "..") {
      continue;
    }
    // TODO(b/296394808): Add UMA metric if unable to record trial due to
    // parse error.
    RecordEarlyBootTrialInChrome(opts.listen_callback,
                                 base::FilePath(reader.name()));
  }
}

// We need to delete the FilePathWatcher instance via a posted task since we
// call FilePathWatche::Watch() on a posted task. The documentation states the
// instance must be destroyed on the same sequence it watches from.
void DeleteWatcher(std::unique_ptr<base::FilePathWatcher> watcher) {
  watcher.reset();
}

// Production implementation of FeaturedClient.
class FeaturedClientImpl : public FeaturedClient {
 public:
  FeaturedClientImpl() : watcher_(std::make_unique<base::FilePathWatcher>()) {}

  FeaturedClientImpl(const FeaturedClient&) = delete;
  FeaturedClientImpl operator=(const FeaturedClient&) = delete;

  ~FeaturedClientImpl() override {
    file_listener_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteWatcher, std::move(watcher_)));
  }

  void Init(dbus::Bus* const bus) {
    InitWithCallback(bus, base::FilePath(feature::kActiveTrialFileDirectory),
                     base::BindRepeating(&RecordEarlyBootTrialInUMA));
  }

  void InitForTesting(dbus::Bus* const bus,  // IN-TEST
                      const base::FilePath& expected_dir,
                      ListenForTrialCallback callback) {
    CHECK_IS_TEST();
    InitWithCallback(bus, expected_dir, callback);
  }

  void HandleSeedFetchedResponse(
      base::OnceCallback<void(bool success)> callback,
      dbus::Response* response) {
    if (!response ||
        response->GetMessageType() != dbus::Message::MESSAGE_METHOD_RETURN) {
      LOG(WARNING) << "Received invalid response for HandleSeedFetched";
      std::move(callback).Run(false);
      return;
    }
    std::move(callback).Run(true);
  }

  void HandleSeedFetched(
      const ::featured::SeedDetails& safe_seed,
      base::OnceCallback<void(bool success)> callback) override {
    dbus::MethodCall method_call(::featured::kFeaturedInterface,
                                 "HandleSeedFetched");

    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(safe_seed);

    featured_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FeaturedClientImpl::HandleSeedFetchedResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void InitWithCallback(dbus::Bus* const bus,
                        const base::FilePath& expected_dir,
                        ListenForTrialCallback callback) {
    featured_service_proxy_ =
        bus->GetObjectProxy(::featured::kFeaturedServiceName,
                            dbus::ObjectPath(::featured::kFeaturedServicePath));
    expected_dir_ = expected_dir;
    listen_callback_ = callback;
    FileWatchOptions opts = {.listen_callback = callback,
                             .expected_dir = expected_dir};
    file_listener_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ListenForActiveEarlyBootTrials, watcher_.get(), opts));
    file_listener_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ReadTrialsActivatedBeforeChromeStartup, opts));
  }

  // Callback used when early-boot trial files are written to `expected_dir_`.
  FeaturedClient::ListenForTrialCallback listen_callback_;

  // Directory where active trial files on platform are written to.
  base::FilePath expected_dir_;

  // Watches for early-boot trial files written to `expected_dir_`.
  std::unique_ptr<base::FilePathWatcher> watcher_;

  raw_ptr<dbus::ObjectProxy> featured_service_proxy_ = nullptr;

  // Sequence runner that an post tasks that may block.
  scoped_refptr<base::SequencedTaskRunner> file_listener_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FeaturedClientImpl> weak_ptr_factory_{this};
};

}  // namespace

FeaturedClient::FeaturedClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FeaturedClient::~FeaturedClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void FeaturedClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new FeaturedClientImpl())->Init(bus);
}

// static
void FeaturedClient::InitializeFake() {
  new FakeFeaturedClient();
}

// static
void FeaturedClient::InitializeForTesting(dbus::Bus* bus,
                                          const base::FilePath& expected_dir,
                                          ListenForTrialCallback callback) {
  DCHECK(bus);
  (new FeaturedClientImpl())
      ->InitForTesting(bus, expected_dir, callback);  // IN-TEST
}

// static
void FeaturedClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
FeaturedClient* FeaturedClient::Get() {
  return g_instance;
}

// static
bool FeaturedClient::ParseTrialFilename(
    const base::FilePath& path,
    base::FieldTrial::ActiveGroup& active_group) {
  std::string filename = path.BaseName().value();
  std::vector<std::string_view> components =
      base::SplitStringPiece(filename, feature::kTrialGroupSeparator,
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (components.size() != 2) {
    LOG(ERROR) << "Active trial filename not of the form TrialName,GroupName: "
               << filename;
    return false;
  }

  std::string trial_name = base::UnescapeURLComponent(
      components[0],
      base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  std::string group_name = base::UnescapeURLComponent(
      components[1],
      base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  active_group.trial_name = trial_name;
  active_group.group_name = group_name;

  return true;
}

}  // namespace ash::featured
