// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/featured/featured_client.h"

#include <string>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
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

// Production implementation of FeaturedClient.
class FeaturedClientImpl : public FeaturedClient {
 public:
  FeaturedClientImpl() = default;

  FeaturedClientImpl(const FeaturedClient&) = delete;
  FeaturedClientImpl operator=(const FeaturedClient&) = delete;

  ~FeaturedClientImpl() override = default;

  void Init(dbus::Bus* const bus) {
    featured_service_proxy_ =
        bus->GetObjectProxy(::featured::kFeaturedServiceName,
                            dbus::ObjectPath(::featured::kFeaturedServicePath));

    listen_callback_ =
        base::BindRepeating(&FeaturedClientImpl::RecordEarlyBootTrialInUMA,
                            weak_ptr_factory_.GetWeakPtr());
    ListenForActiveEarlyBootTrials();
  }

  void InitWithCallbackForTesting(dbus::Bus* const bus,  // IN-TEST
                                  const base::FilePath& expected_dir,
                                  ListenForTrialCallback callback) {
    CHECK_IS_TEST();
    featured_service_proxy_ =
        bus->GetObjectProxy(::featured::kFeaturedServiceName,
                            dbus::ObjectPath(::featured::kFeaturedServicePath));
    expected_dir_ = expected_dir;
    listen_callback_ = callback;
    ListenForActiveEarlyBootTrials();
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
  void RecordEarlyBootTrialInUMA(const std::string& trial_name,
                                 const std::string& group_name) {
    base::FieldTrial* trial =
        base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
    // This records the trial in UMA.
    trial->Activate();
  }

  void RecordEarlyBootTrialInChrome(const base::FilePath& path, bool error) {
    if (error || path.DirName() != expected_dir_) {
      // TODO(b/296394808): Add UMA metric if we enter this code path since it
      // is not expected.
      return;
    }

    base::FieldTrial::ActiveGroup active_group;
    if (ParseTrialFilename(path, active_group)) {
      listen_callback_.Run(active_group.trial_name, active_group.group_name);
    }
  }

  void ListenForActiveEarlyBootTrials() {
    base::FilePathWatcher::WatchOptions options = {
        // Watches for changes in a directory.
        .type = base::FilePathWatcher::Type::kRecursive,
        // Reports the path of modified files in the directory.
        .report_modified_path = true};

    watcher_.WatchWithOptions(
        expected_dir_, options,
        base::BindRepeating(&FeaturedClientImpl::RecordEarlyBootTrialInChrome,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Watches for early-boot trial files written to `expected_dir_`.
  base::FilePathWatcher watcher_;

  // Callback used when early-boot trial files are written to `expected_dir_`.
  FeaturedClient::ListenForTrialCallback listen_callback_;

  // Directory where active trial files on platform are written to, defaulting
  // to feature::kActiveTrialFileDirectory. The default value can be overridden
  // with InitializeForTesting().
  base::FilePath expected_dir_ =
      base::FilePath(feature::kActiveTrialFileDirectory);

  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> featured_service_proxy_ = nullptr;

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
      ->InitWithCallbackForTesting(bus, expected_dir, callback);  // IN-TEST
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
  std::vector<base::StringPiece> components =
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
